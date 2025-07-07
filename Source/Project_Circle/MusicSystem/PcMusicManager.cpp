// Fill out your copyright notice in the Description page of Project Settings.

#include "PcMusicManager.h"
#include "MetasoundOutput.h"
#include "MusicData.h"
#include "Components/AudioComponent.h"
#include "MetasoundOutputSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Algo/Sort.h"
#include "Kismet/KismetMathLibrary.h"

// A temporary struct to hold the full profile of a section for analysis.
struct FSectionProfileData
{
	int32 StartTime;
	int32 EndTime; // Added to make duration calculations easier
	float BaseBeatLength;
	int32 AnchorTimestamp;
	float HybridApsScore; // The final intensity score combining all evidence.
};

// Helper function to get a percentile value from a sorted array of floats.
float GetPercentile(const TArray<float>& SortedData, float Percentile)
{
	if (SortedData.Num() == 0) return 0.f;
	int32 Index = FMath::Clamp(FMath::RoundToInt(SortedData.Num() * Percentile) - 1, 0, SortedData.Num() - 1);
	return SortedData[Index];
}

FString SectionTypeToString(EGameplaySectionType Type)
{
	const UEnum* EnumPtr = FindObject<UEnum>(nullptr, TEXT("/Script/Project_Circle.EGameplaySectionType"), true);
	if (!EnumPtr) return FString("Invalid");
	return EnumPtr->GetNameStringByValue((int64)Type);
}

// CalculateAPS with Hit Sound Weighting
float CalculateAPS(const TArray<const FMusicData*>& HitObjects, float DurationSec, float BaseBeatLength, float SliderTickRate)
{
	if (DurationSec <= 0 || HitObjects.Num() == 0) return 0.f;
	float TotalActions = 0;
	for (const FMusicData* HO : HitObjects)
	{
		float ActionValue = 1.0f;
		if (HO->HitSound & 2) ActionValue += 0.2f; // Whistle
		if (HO->HitSound & 8) ActionValue += 0.2f; // Clap
		if (HO->HitSound & 4) ActionValue += 0.6f; // Finish is most important
		TotalActions += ActionValue;
		if (HO->HitObjectType & 2) { // Slider
			TotalActions += (ActionValue * HO->Repeats);
			const float SliderDuration = HO->SliderEndTimeMS - HO->TimestampMS;
			// Ensure BaseBeatLength and SliderTickRate are valid to prevent division by zero
			const float TickInterval = (BaseBeatLength > 0 && SliderTickRate > 0) ? FMath::Max(20.f, BaseBeatLength / SliderTickRate) : -1.f;
			if (SliderDuration > 0 && TickInterval > 0) {
				const float SinglePassDuration = SliderDuration / FMath::Max(1, HO->Repeats);
				int32 TickCount = FMath::FloorToInt(SinglePassDuration / TickInterval);
				if (FMath::IsNearlyEqual(SinglePassDuration, (float)(TickCount * TickInterval), 10.f)) {
					TickCount = FMath::Max(0, TickCount - 1);
				}
				if (TickCount > 0) TotalActions += TickCount * HO->Repeats;
			}
		}
	}
	return TotalActions / DurationSec;
}

APcMusicManager::APcMusicManager()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootSceneComponent"));
	MusicAudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("MusicAudioComponent"));
	MusicAudioComponent->SetupAttachment(RootComponent);
	MusicAudioComponent->bAutoActivate = false;
	NextEventIndex = 0;
	CurrentMusicProgress = 0.0f;
	CurrentBPM = 0.0f;
	CurrentBeatLengthMs = 0.0f;
	CurrentSectionType = EGameplaySectionType::Normal;
	CurrentMeter = 4;
	bInBreakPeriod = false;
	CurrentBreakEndTimeMS = 0;
	LastProcessedMusicProgressMs = -1;
	NextBeatTimestampMS = 0;
	CurrentRhythmSectionIndex = 0;
}

void APcMusicManager::BeginPlay() { Super::BeginPlay(); }

void APcMusicManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (OnMetasoundOutputValueChanged.IsBound()) { OnMetasoundOutputValueChanged.Unbind(); }
	Super::EndPlay(EndPlayReason);
}

void APcMusicManager::Tick(float DeltaTime) { Super::Tick(DeltaTime); }

void APcMusicManager::StartMusicPlayback()
{
	if (MainMusicMetaSound && MusicAudioComponent && SongWaveAsset && DataTableMusicInfo)
	{
		if (LoadMusicDataFromTable() && LoadedMusicData.Num() > 0)
		{
			AnalyzeAndGenerateRhythmSections();
			NextEventIndex = 0;
			CurrentMusicProgress = 0.0f;
			CurrentBPM = 0.0f;
			CurrentBeatLengthMs = 0.0f;
			CurrentSectionType = EGameplaySectionType::Normal;
			CurrentMeter = 4;
			bInBreakPeriod = false;
			CurrentBreakEndTimeMS = 0;
			LastProcessedMusicProgressMs = -1;
			NextBeatTimestampMS = 0;
			CurrentRhythmSectionIndex = 0;
			if (OnMetasoundOutputValueChanged.IsBound()) { OnMetasoundOutputValueChanged.Unbind(); }
			MusicAudioComponent->Stop();
			MusicAudioComponent->SetSound(MainMusicMetaSound);
			MusicAudioComponent->SetWaveParameter(FName("Song"), SongWaveAsset);
			MusicAudioComponent->Play();
			if (UWorld* World = GetWorld())
			{
				OnMetasoundOutputValueChanged.BindUFunction(this, FName("OnMetaSoundCurrentTimeChanged"));
				if (UMetaSoundOutputSubsystem* MetaSoundOutputSubsystem = World->GetSubsystem<
					UMetaSoundOutputSubsystem>())
				{
					MetaSoundOutputSubsystem->WatchOutput(MusicAudioComponent, FName("CurrentTime"),
					                                      OnMetasoundOutputValueChanged);
				}
			}
		}
	}
}

bool APcMusicManager::LoadMusicDataFromTable()
{
	if (!DataTableMusicInfo) return false;
	LoadedMusicData.Empty();
	DataTableMusicInfo->ForeachRow<FMusicData>(
		TEXT("Loading Music Data"), [this](const FName& Key, const FMusicData& Value) { LoadedMusicData.Add(Value); });
	return LoadedMusicData.Num() > 0;
}


void APcMusicManager::AnalyzeAndGenerateRhythmSections()
{
	RhythmSections.Empty();
	if (LoadedMusicData.Num() == 0) return;

	TArray<FMusicData> UninheritedTPs;
	TArray<const FMusicData*> AllHitObjects, AllAudioBeats, AllTimingPoints;
	for (const FMusicData& Data : LoadedMusicData) {
		if (Data.EntryType == EGameplayEntryType::TimingPoint) {
			AllTimingPoints.Add(&Data);
			if (Data.Uninherited == 1) UninheritedTPs.Add(Data);
		} else if (Data.EntryType == EGameplayEntryType::HitObject) AllHitObjects.Add(&Data);
		else if (Data.EntryType == EGameplayEntryType::AudioBeat) AllAudioBeats.Add(&Data);
	}
	if (UninheritedTPs.Num() == 0 || AllHitObjects.Num() == 0) return;

	// --- PHASE 1: Establish Ground Truth & Major Boundaries ---
	TSet<int32> BoundarySet;
	BoundarySet.Add(0);
	for (const FMusicData& TP : UninheritedTPs) BoundarySet.Add(TP.TimestampMS);
	bool bKiai = false;
	for (const FMusicData& Data : LoadedMusicData) {
		if (Data.EntryType == EGameplayEntryType::TimingPoint) { if (((Data.Effects & 1) != 0) != bKiai) { bKiai = !bKiai; BoundarySet.Add(Data.TimestampMS); } }
		else if (Data.EntryType == EGameplayEntryType::Break) { BoundarySet.Add(Data.TimestampMS); BoundarySet.Add(Data.BreakEndTimeMS); }
	}
	BoundarySet.Add(AllHitObjects.Last()->TimestampMS + 5000);
	TArray<int32> MajorBoundaries = BoundarySet.Array();
	MajorBoundaries.Sort();
	
	// --- PHASE 2: Refine Boundaries with Change Point Detection ---
	const int32 LookbackWindowMS = 4000, StepSizeMS = 500, SnapWindowMS = 250;
	const float DropThreshold = 0.5f, SpikeThreshold = 2.0f;
	for (int32 i = 0; i < MajorBoundaries.Num() - 1; ++i) {
		const int32 SectionStart = MajorBoundaries[i], SectionEnd = MajorBoundaries[i+1];
		if (SectionEnd - SectionStart < LookbackWindowMS + StepSizeMS) continue;
		float LastWindowAPS = -1.f;
		for (int32 CurrentTime = SectionStart + LookbackWindowMS; CurrentTime < SectionEnd; CurrentTime += StepSizeMS) {
			TArray<const FMusicData*> HistoryObjects, ImmediateObjects;
			float CurrentBaseBeatLength = 500.f, CurrentTickRate = 1.0f;
			for(const FMusicData* HO : AllHitObjects) {
				if (HO->TimestampMS >= CurrentTime - LookbackWindowMS && HO->TimestampMS < CurrentTime) HistoryObjects.Add(HO);
				if (HO->TimestampMS >= CurrentTime && HO->TimestampMS < CurrentTime + StepSizeMS) ImmediateObjects.Add(HO);
			}
			if(HistoryObjects.Num() < 3) continue;
			for(const FMusicData& TP : UninheritedTPs) { if(TP.TimestampMS <= CurrentTime) CurrentBaseBeatLength = TP.BeatLength; }
			if (HistoryObjects.Num() > 0 && HistoryObjects[0]->SliderTickRate > 0) CurrentTickRate = HistoryObjects[0]->SliderTickRate;
			float CurrentWindowAPS = CalculateAPS(ImmediateObjects, StepSizeMS / 1000.f, CurrentBaseBeatLength, CurrentTickRate);
			if(LastWindowAPS < 0) LastWindowAPS = CalculateAPS(HistoryObjects, LookbackWindowMS / 1000.f, CurrentBaseBeatLength, CurrentTickRate);
			bool bChangeDetected = (LastWindowAPS > 1.0f && CurrentWindowAPS < LastWindowAPS * DropThreshold) || (CurrentWindowAPS > FMath::Max(1.0f, LastWindowAPS) * SpikeThreshold);
			if (bChangeDetected) {
				int32 BestSnapTime = -1, FinishSnapTime = -1;
				for(const FMusicData* HO : AllHitObjects) {
					if(HO->TimestampMS >= CurrentTime - SnapWindowMS && HO->TimestampMS < CurrentTime + StepSizeMS + SnapWindowMS) {
						if (HO->HitSound & 4) { FinishSnapTime = HO->TimestampMS; break; }
						if (BestSnapTime == -1 || FMath::Abs(HO->TimestampMS - CurrentTime) < FMath::Abs(BestSnapTime - CurrentTime)) BestSnapTime = HO->TimestampMS;
					}
				}
				if(FinishSnapTime != -1) BoundarySet.Add(FinishSnapTime);
				else if(BestSnapTime != -1) BoundarySet.Add(BestSnapTime);
				else BoundarySet.Add(CurrentTime);
				CurrentTime += LookbackWindowMS; LastWindowAPS = -1.f;
			} else { LastWindowAPS = (LastWindowAPS * 0.7f) + (CurrentWindowAPS * 0.3f); }
		}
	}
	TArray<int32> FinalBoundaries = BoundarySet.Array();
	FinalBoundaries.Sort();
	
	// --- PHASE 3: Final Analysis on Refined Boundaries ---
	TArray<FSectionProfileData> ProfileList;
	TArray<float> AllActionScores;
	float LastValidBaseBeatLength = (UninheritedTPs.Num() > 0) ? UninheritedTPs[0].BeatLength : 500.f;
	for (int32 i = 0; i < FinalBoundaries.Num() - 1; ++i) {
		FSectionProfileData Profile;
		Profile.StartTime = FinalBoundaries[i];
		Profile.EndTime = FinalBoundaries[i + 1];
		const float DurationSec = (Profile.EndTime - Profile.StartTime) / 1000.f;
		if (DurationSec < 0.25f) continue;
		for (const FMusicData& TP : UninheritedTPs) { if (TP.TimestampMS <= Profile.StartTime) Profile.BaseBeatLength = TP.BeatLength; }
		if (Profile.BaseBeatLength <= 0) Profile.BaseBeatLength = LastValidBaseBeatLength; else LastValidBaseBeatLength = Profile.BaseBeatLength;
		TArray<const FMusicData*> SectionHOs; float GlobalSliderTickRate = 1.0f;
		for (const FMusicData* HO : AllHitObjects) { if (HO->TimestampMS >= Profile.StartTime && HO->TimestampMS < Profile.EndTime) { SectionHOs.Add(HO); if (SectionHOs.Num() == 1 && HO->SliderTickRate > 0) GlobalSliderTickRate = HO->SliderTickRate; } }
		float ActionsPerSecond = CalculateAPS(SectionHOs, DurationSec, Profile.BaseBeatLength, GlobalSliderTickRate);
		float AudioEnergy = 0.f; TArray<const FMusicData*> SectionAudioBeats;
		for (const FMusicData* AB : AllAudioBeats) { if (AB->TimestampMS >= Profile.StartTime && AB->TimestampMS < Profile.EndTime) SectionAudioBeats.Add(AB); }
		if (SectionAudioBeats.Num() > 0) { for(const FMusicData* AB : SectionAudioBeats) { AudioEnergy += AB->AudioBeatStrength; } AudioEnergy /= SectionAudioBeats.Num(); }
		Profile.HybridApsScore = ActionsPerSecond + (AudioEnergy * 0.2f);
		AllActionScores.Add(Profile.HybridApsScore);
		ProfileList.Add(Profile);
	}
	if (ProfileList.Num() == 0) return;

	// --- PHASE 4: Self-Calibrating Thresholds & Rhythm Assignment ---
	float HighThreshold, VeryHighThreshold;
	TArray<float> KiaiScores;
	for (int32 i = 0; i < ProfileList.Num(); ++i) {
		bool bIsKiaiSection = false;
		for (const FMusicData* TP : AllTimingPoints) {
			if (TP->TimestampMS >= ProfileList[i].StartTime && TP->TimestampMS < ProfileList[i].EndTime) {
				if (TP->Effects & 1) { bIsKiaiSection = true; break; }
			}
		}
		if (bIsKiaiSection && ProfileList[i].HybridApsScore > 0) KiaiScores.Add(ProfileList[i].HybridApsScore);
	}

	if (KiaiScores.Num() > 1) { 
		float AvgKiaiScore = 0.f;
		for(float Score : KiaiScores) AvgKiaiScore += Score;
		AvgKiaiScore /= KiaiScores.Num();
		HighThreshold = AvgKiaiScore * 0.8f;
		VeryHighThreshold = AvgKiaiScore * 1.35f;
		UE_LOG(LogTemp, Log, TEXT("Calibrating thresholds from Kiai. Avg Kiai Score: %.2f"), AvgKiaiScore);
	} else { 
		AllActionScores.Sort();
		HighThreshold = GetPercentile(AllActionScores, 0.70f);
		VeryHighThreshold = GetPercentile(AllActionScores, 0.90f);
		UE_LOG(LogTemp, Log, TEXT("Calibrating thresholds from Percentile."));
	}

	TArray<FGameplayRhythmSection> TempSections;
	for (int32 i = 0; i < ProfileList.Num(); ++i) {
		FSectionProfileData& Profile = ProfileList[i];
		int32 Anchor = 0;
		for (const FMusicData* HO : AllHitObjects) { if (HO->TimestampMS >= Profile.StartTime) { Anchor = HO->TimestampMS; break; } }
		Profile.AnchorTimestamp = Anchor;
		float GameplayBeatLength = Profile.BaseBeatLength;
		if (GameplayBeatLength <= 0) continue;
		float ScoreToCheck = Profile.HybridApsScore;
		if (i > 0) {
			const float PreviousScore = FMath::Max(ProfileList[i-1].HybridApsScore, 0.1f);
			if (ScoreToCheck > PreviousScore * 1.8f) { ScoreToCheck *= 1.2f; }
		}
		if (ScoreToCheck >= VeryHighThreshold && VeryHighThreshold > 0) GameplayBeatLength /= 4.0f;
		else if (ScoreToCheck >= HighThreshold && HighThreshold > 0) GameplayBeatLength /= 2.0f;
		
		FGameplayRhythmSection FinalSection;
		FinalSection.StartTimeMS = Profile.StartTime;
		FinalSection.BeatLengthMS = GameplayBeatLength;
		FinalSection.BPM = (GameplayBeatLength > 0) ? 60000.0f / GameplayBeatLength : 0.f;
		FinalSection.AnchorTimestampMS = Profile.AnchorTimestamp > 0 ? Profile.AnchorTimestamp : Profile.StartTime;
		TempSections.Add(FinalSection);
	}
	
	// --- PHASE 5: Structural Smoothing Pass ---
	if (TempSections.Num() == 0) return;
	RhythmSections.Add(TempSections[0]);
	for (int32 i = 1; i < TempSections.Num(); ++i) {
		FGameplayRhythmSection& Current = TempSections[i];
		FGameplayRhythmSection& Last = RhythmSections.Last();
		
		float BaseBeatForLast = 500;
		for (const FMusicData& TP : UninheritedTPs) { if (TP.TimestampMS <= Last.StartTimeMS) BaseBeatForLast = TP.BeatLength; }
		
		const float MinDurationMS = FMath::Max(250.f, BaseBeatForLast * 2.0f);
		
		if (Current.StartTimeMS - Last.StartTimeMS < MinDurationMS) {
			if (Current.BPM > Last.BPM) {
				Last.BPM = Current.BPM;
				Last.BeatLengthMS = Current.BeatLengthMS;
			}
		} else if (FMath::IsNearlyEqual(Current.BPM, Last.BPM, 1.f)) {
			// Merge identical sections
		}
		else {
			RhythmSections.Add(Current);
		}
	}

	// Final Log
	UE_LOG(LogTemp, Warning, TEXT("--- FINAL RHYTHM SECTIONS ---"));
	for (const auto& Section : RhythmSections) {
		UE_LOG(LogTemp, Warning, TEXT("Final Section @ %dms: BPM = %.1f"), Section.StartTimeMS, Section.BPM);
	}
}

void APcMusicManager::OnMetaSoundCurrentTimeChanged(FName OutputName, const FMetaSoundOutput& Output)
{
	if (Output.IsValid() && Output.IsType<Metasound::FTime>())
	{
		Metasound::FTime RetrievedTimeStruct;
		if (Output.Get<Metasound::FTime>(RetrievedTimeStruct))
		{
			CurrentMusicProgress = static_cast<float>(RetrievedTimeStruct.GetSeconds());
			int32 CurrentMusicProgressMs = FMath::RoundToInt(CurrentMusicProgress * 1000.0f);
			if (CurrentMusicProgressMs > LastProcessedMusicProgressMs)
			{
				ProcessMusicEvents(CurrentMusicProgressMs);
				LastProcessedMusicProgressMs = CurrentMusicProgressMs;
			}
		}
	}
}

void APcMusicManager::ProcessMusicEvents(int32 InCurrentTimeMS)
{
	if (LoadedMusicData.Num() == 0) return;
	UpdateRhythmSection(InCurrentTimeMS);
	ProcessBeatTicks(InCurrentTimeMS);
	CheckForBreakEnd(InCurrentTimeMS);
	while (NextEventIndex < LoadedMusicData.Num())
	{
		const FMusicData& NextEvent = LoadedMusicData[NextEventIndex];
		if (InCurrentTimeMS < NextEvent.TimestampMS) break;
		if (NextEvent.EntryType == EGameplayEntryType::HitObject)
		{
			OnHitObjectTriggered(NextEvent.TimestampMS, NextEvent.HitObjectType);
		}
		else if (NextEvent.EntryType == EGameplayEntryType::TimingPoint) { UpdateMeter(NextEvent); }
		else if (NextEvent.EntryType == EGameplayEntryType::Break) { ProcessBreakStartEvent(NextEvent); }
		NextEventIndex++;
	}
}

void APcMusicManager::UpdateRhythmSection(int32 InCurrentTimeMS)
{
	if (RhythmSections.Num() == 0) return;
	int32 NewRhythmSectionIndex = CurrentRhythmSectionIndex;
	while (NewRhythmSectionIndex < RhythmSections.Num() - 1 && InCurrentTimeMS >= RhythmSections[NewRhythmSectionIndex +
		1].StartTimeMS) { NewRhythmSectionIndex++; }
	if (NewRhythmSectionIndex != CurrentRhythmSectionIndex || CurrentBPM == 0)
	{
		CurrentRhythmSectionIndex = NewRhythmSectionIndex;
		const FGameplayRhythmSection& CurrentSection = RhythmSections[CurrentRhythmSectionIndex];
		CurrentBPM = CurrentSection.BPM;
		CurrentBeatLengthMs = CurrentSection.BeatLengthMS;
		OnBPMChanged(CurrentBPM);
		if (CurrentSectionType != CurrentSection.SectionType)
		{
			CurrentSectionType = CurrentSection.SectionType;
			K2_OnSectionTypeChanged(CurrentSectionType);
		}
		NextBeatTimestampMS = CurrentSection.AnchorTimestampMS;
	}
}

void APcMusicManager::ProcessBeatTicks(int32 InCurrentTimeMS)
{
	if (CurrentBeatLengthMs <= 0 || bInBreakPeriod || CurrentSectionType == EGameplaySectionType::Break) return;
	if (NextBeatTimestampMS < InCurrentTimeMS - FMath::RoundToInt(CurrentBeatLengthMs * 2))
	{
		const FGameplayRhythmSection& CurrentSection = RhythmSections[CurrentRhythmSectionIndex];
		if (CurrentSection.BeatLengthMS <= 0) return;
		const float BeatsSinceAnchor = (InCurrentTimeMS - CurrentSection.AnchorTimestampMS) / CurrentSection.
			BeatLengthMS;
		const int32 CurrentBeatNumber = FMath::FloorToInt(BeatsSinceAnchor);
		NextBeatTimestampMS = CurrentSection.AnchorTimestampMS + FMath::RoundToInt(
			(CurrentBeatNumber + 1) * CurrentSection.BeatLengthMS);
	}
	while (InCurrentTimeMS >= NextBeatTimestampMS)
	{
		OnBeatTriggered.Broadcast(NextBeatTimestampMS / 1000.0f);
		K2_OnBeatTriggered(NextBeatTimestampMS / 1000.0f);
		if (CurrentBeatLengthMs <= 0) break;
		NextBeatTimestampMS += FMath::RoundToInt(CurrentBeatLengthMs);
	}
}

void APcMusicManager::CheckForBreakEnd(int32 InCurrentTimeMS)
{
	if (bInBreakPeriod && InCurrentTimeMS >= CurrentBreakEndTimeMS)
	{
		bInBreakPeriod = false;
		OnBreakEnd(CurrentBreakEndTimeMS);
	}
}

void APcMusicManager::ProcessBreakStartEvent(const FMusicData& BreakData)
{
	bInBreakPeriod = true;
	CurrentBreakEndTimeMS = BreakData.BreakEndTimeMS;
	OnBreakStart(BreakData.TimestampMS, BreakData.BreakEndTimeMS);
}

void APcMusicManager::UpdateMeter(const FMusicData& TimingPointData)
{
	if (TimingPointData.Uninherited == 1 && TimingPointData.Meter != CurrentMeter)
	{
		CurrentMeter = TimingPointData.Meter;
		OnMeterChanged(CurrentMeter);
	}
}