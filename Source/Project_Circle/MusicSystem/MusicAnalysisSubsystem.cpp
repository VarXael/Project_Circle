#include "MusicAnalysisSubsystem.h"
#include "Algo/Sort.h"

// Helper function to get a percentile value from a sorted array of floats.
float GetPercentile(const TArray<float>& SortedData, float Percentile)
{
	if (SortedData.Num() == 0) return 0.f;
	int32 Index = FMath::Clamp(FMath::RoundToInt(SortedData.Num() * Percentile) - 1, 0, SortedData.Num() - 1);
	return SortedData[Index];
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
		if (HO->HitSound & 4) ActionValue += 0.6f; // Finish
		TotalActions += ActionValue;
		if (HO->HitObjectType & 2) {
			TotalActions += (ActionValue * HO->Repeats);
			const float SliderDuration = HO->SliderEndTimeMS - HO->TimestampMS;
			const float TickInterval = (BaseBeatLength > 0 && SliderTickRate > 0) ? FMath::Max(20.f, BaseBeatLength / SliderTickRate) : -1.f;
			if (SliderDuration > 0 && TickInterval > 0) {
				const float SinglePassDuration = SliderDuration / FMath::Max(1, HO->Repeats);
				int32 TickCount = FMath::FloorToInt(SinglePassDuration / TickInterval);
				if (FMath::IsNearlyEqual(SinglePassDuration, (float)(TickCount * TickInterval), 10.f)) TickCount = FMath::Max(0, TickCount - 1);
				if (TickCount > 0) TotalActions += TickCount * HO->Repeats;
			}
		}
	}
	return TotalActions / DurationSec;
}

void UMusicAnalysisSubsystem::StartSongAnalysis(UDataTable* MusicDataTable)
{
	if (!MusicDataTable)
	{
		UE_LOG(LogTemp, Error, TEXT("MusicAnalysisSubsystem: Invalid DataTable provided for analysis."));
		return;
	}
	bAnalysisComplete = false;
	if (LoadMusicDataFromTable(MusicDataTable))
	{
		CurrentSongAnalysis = AnalyzeRhythmSections();
		bAnalysisComplete = true;
		NextEventIndex = 0;
		LastProcessedMusicProgressMs = -1;
		NextBeatTimestampMS = 0;
		CurrentRhythmSectionIndex = 0;
		CurrentBPM = 0.f;
		CurrentMeter = 4;
		bInBreakPeriod = false;
		NoteEventQueue.Empty();
	}
}

void UMusicAnalysisSubsystem::UpdateMusicTime(float CurrentTimeSeconds)
{
	if (!bAnalysisComplete) return;
	int32 CurrentTimeMs = FMath::RoundToInt(CurrentTimeSeconds * 1000.0f);
	if (CurrentTimeMs > LastProcessedMusicProgressMs)
	{
		ProcessMusicEvents(CurrentTimeMs);
		LastProcessedMusicProgressMs = CurrentTimeMs;
	}
}

bool UMusicAnalysisSubsystem::LoadMusicDataFromTable(UDataTable* MusicDataTable)
{
	if (!MusicDataTable) return false;
	LoadedMusicData.Empty();
	MusicDataTable->ForeachRow<FMusicData>(TEXT("Loading Music Data"), [this](const FName& Key, const FMusicData& Value) { LoadedMusicData.Add(Value); });
	return LoadedMusicData.Num() > 0;
}

FSongAnalysisResult UMusicAnalysisSubsystem::AnalyzeRhythmSections()
{
	// This is your current, working analysis logic. It is unchanged.
	FSongAnalysisResult Result;
	if (LoadedMusicData.Num() == 0) return Result;
	TArray<FMusicData> UninheritedTPs;
	TArray<const FMusicData*> AllHitObjects, AllAudioBeats, AllTimingPoints;
	for (const FMusicData& Data : LoadedMusicData) {
		if (Data.EntryType == EGameplayEntryType::TimingPoint) {
			AllTimingPoints.Add(&Data);
			if (Data.Uninherited == 1) UninheritedTPs.Add(Data);
		} else if (Data.EntryType == EGameplayEntryType::HitObject) AllHitObjects.Add(&Data);
		else if (Data.EntryType == EGameplayEntryType::AudioBeat) AllAudioBeats.Add(&Data);
	}
	if (UninheritedTPs.Num() == 0 || AllHitObjects.Num() == 0) return Result;
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
	TArray<FSectionProfileData> ProfileList;
	TArray<float> AllActionScores;
	float LastValidBaseBeatLength = (UninheritedTPs.Num() > 0) ? UninheritedTPs[0].BeatLength : 500.f;
	for (int32 i = 0; i < FinalBoundaries.Num() - 1; ++i) {
		FSectionProfileData Profile;
		Profile.StartTime = FinalBoundaries[i]; Profile.EndTime = FinalBoundaries[i + 1];
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
	if (ProfileList.Num() == 0) return Result;
	
	float HighThreshold, VeryHighThreshold;
	AllActionScores.Sort();
	HighThreshold = GetPercentile(AllActionScores, 0.70f);
	VeryHighThreshold = GetPercentile(AllActionScores, 0.90f);
	
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
		FinalSection.StartTimeMS = Profile.StartTime; FinalSection.BeatLengthMS = GameplayBeatLength;
		FinalSection.BPM = (GameplayBeatLength > 0) ? 60000.0f / GameplayBeatLength : 0.f;
		FinalSection.AnchorTimestampMS = Profile.AnchorTimestamp > 0 ? Profile.AnchorTimestamp : Profile.StartTime;
		TempSections.Add(FinalSection);
	}
	
	if (TempSections.Num() == 0) return Result;
	Result.RhythmSections.Add(TempSections[0]);
	for (int32 i = 1; i < TempSections.Num(); ++i) {
		FGameplayRhythmSection& Current = TempSections[i];
		FGameplayRhythmSection& Last = Result.RhythmSections.Last();
		float BaseBeatForLast = 500;
		for (const FMusicData& TP : UninheritedTPs) { if (TP.TimestampMS <= Last.StartTimeMS) BaseBeatForLast = TP.BeatLength; }
		const float MinDurationMS = FMath::Max(250.f, BaseBeatForLast * 2.0f);
		if (Current.StartTimeMS - Last.StartTimeMS < MinDurationMS) {
			if (Current.BPM > Last.BPM) { Last.BPM = Current.BPM; Last.BeatLengthMS = Current.BeatLengthMS; }
		} else if (!FMath::IsNearlyEqual(Current.BPM, Last.BPM, 1.f)) { Result.RhythmSections.Add(Current); }
	}
	return Result;
}


void UMusicAnalysisSubsystem::GenerateSliderSubEvents(const FMusicData& SliderData)
{
	if (!(SliderData.HitObjectType & 2)) return;
	float BaseBeatLength = 500.f, SliderTickRate = 1.0f;
	
	TArray<const FMusicData*> RelevantTPs;
	for(const auto& Data : LoadedMusicData) {
		if (Data.EntryType == EGameplayEntryType::TimingPoint && Data.TimestampMS <= SliderData.TimestampMS) RelevantTPs.Add(&Data);
		if (Data.EntryType == EGameplayEntryType::HitObject && Data.SliderTickRate > 0) SliderTickRate = Data.SliderTickRate;
	}
	for (const FMusicData* TP : RelevantTPs) if(TP->Uninherited == 1) BaseBeatLength = TP->BeatLength;
	
	const float SliderDuration = SliderData.SliderEndTimeMS - SliderData.TimestampMS;
	const float TickInterval = (BaseBeatLength > 0 && SliderTickRate > 0) ? FMath::Max(20.f, BaseBeatLength / SliderTickRate) : -1.f;

	if (SliderDuration > 0 && TickInterval > 0)
	{
		const float SinglePassDuration = SliderDuration / FMath::Max(1, SliderData.Repeats);
		for (int32 Pass = 0; Pass < SliderData.Repeats; ++Pass) {
			for (float TimeAlongPass = TickInterval; TimeAlongPass < SinglePassDuration; TimeAlongPass += TickInterval) {
				if (!FMath::IsNearlyEqual(TimeAlongPass, SinglePassDuration, 1.f)) {
					FQueuedNoteEvent TickEvent;
					TickEvent.TimestampMS = SliderData.TimestampMS + FMath::RoundToInt((Pass * SinglePassDuration) + TimeAlongPass);
					TickEvent.NoteType = EQueuedNoteType::SliderTick;
					TickEvent.OriginalHitSound = SliderData.HitSound;
					NoteEventQueue.Add(TickEvent);
				}
			}
		}
		for (int32 Repeat = 1; Repeat <= SliderData.Repeats; ++Repeat) {
			FQueuedNoteEvent TailEvent;
			TailEvent.TimestampMS = SliderData.TimestampMS + FMath::RoundToInt(Repeat * SinglePassDuration);
			TailEvent.NoteType = EQueuedNoteType::SliderTail;
			TailEvent.OriginalHitSound = SliderData.HitSound;
			NoteEventQueue.Add(TailEvent);
		}
	}
	NoteEventQueue.Sort([](const FQueuedNoteEvent& A, const FQueuedNoteEvent& B) { return A.TimestampMS > B.TimestampMS; });
}

void UMusicAnalysisSubsystem::ProcessMusicEvents(int32 InCurrentTimeMS)
{
	if (!bAnalysisComplete) return;
	UpdateRhythmSection(InCurrentTimeMS);
	ProcessBeatTicks(InCurrentTimeMS);
	while (true)
	{
		const FMusicData* NextMajorEvent = (NextEventIndex < LoadedMusicData.Num()) ? &LoadedMusicData[NextEventIndex] : nullptr;
		FQueuedNoteEvent* NextSubEvent = (NoteEventQueue.Num() > 0) ? &NoteEventQueue.Last() : nullptr;
		int32 NextMajorEventTime = NextMajorEvent ? NextMajorEvent->TimestampMS : INT_MAX;
		int32 NextSubEventTime = NextSubEvent ? NextSubEvent->TimestampMS : INT_MAX;
		if (FMath::Min(NextMajorEventTime, NextSubEventTime) > InCurrentTimeMS) break;

		if (NextMajorEventTime <= NextSubEventTime) {
			if (NextMajorEvent->EntryType == EGameplayEntryType::HitObject) {
				OnNoteHit.Broadcast(NextMajorEvent->TimestampMS, NextMajorEvent->HitObjectType, NextMajorEvent->HitSound);
				if (NextMajorEvent->HitObjectType & 2) GenerateSliderSubEvents(*NextMajorEvent);
			} else if (NextMajorEvent->EntryType == EGameplayEntryType::TimingPoint) {
				if(NextMajorEvent->Uninherited == 1 && NextMajorEvent->Meter != CurrentMeter) {
					CurrentMeter = NextMajorEvent->Meter; OnMeterChanged.Broadcast(CurrentMeter);
				}
			} else if (NextMajorEvent->EntryType == EGameplayEntryType::Break) {
				bInBreakPeriod = true; OnBreakStart.Broadcast(NextMajorEvent->TimestampMS, NextMajorEvent->BreakEndTimeMS);
			}
			NextEventIndex++;
		} else {
			OnNoteHit.Broadcast(NextSubEvent->TimestampMS, NextSubEvent->NoteType, NextSubEvent->OriginalHitSound);
			NoteEventQueue.Pop();
		}
	}
	if(bInBreakPeriod) {
		for(const auto& Event : LoadedMusicData) {
			if(Event.EntryType == EGameplayEntryType::Break && LastProcessedMusicProgressMs < Event.BreakEndTimeMS && InCurrentTimeMS >= Event.BreakEndTimeMS) {
				bInBreakPeriod = false; OnBreakEnd.Broadcast(Event.TimestampMS, Event.BreakEndTimeMS);
				break;
			}
		}
	}
}

void UMusicAnalysisSubsystem::UpdateRhythmSection(int32 InCurrentTimeMS)
{
	const auto& Sections = CurrentSongAnalysis.RhythmSections;
	if (Sections.Num() == 0) return;
	int32 NewRhythmSectionIndex = CurrentRhythmSectionIndex;
	while (NewRhythmSectionIndex < Sections.Num() - 1 && InCurrentTimeMS >= Sections[NewRhythmSectionIndex + 1].StartTimeMS) NewRhythmSectionIndex++;
	if (NewRhythmSectionIndex != CurrentRhythmSectionIndex || CurrentBPM == 0) {
		CurrentRhythmSectionIndex = NewRhythmSectionIndex;
		const FGameplayRhythmSection& CurrentSection = Sections[CurrentRhythmSectionIndex];
		if(!FMath::IsNearlyEqual(CurrentBPM, CurrentSection.BPM)) {
			CurrentBPM = CurrentSection.BPM; OnBPMChanged.Broadcast(CurrentBPM);
		}
		NextBeatTimestampMS = CurrentSection.AnchorTimestampMS;
	}
}

void UMusicAnalysisSubsystem::ProcessBeatTicks(int32 InCurrentTimeMS)
{
	if (bInBreakPeriod || CurrentSongAnalysis.RhythmSections.Num() == 0) return;
	const FGameplayRhythmSection& CurrentSection = CurrentSongAnalysis.RhythmSections[CurrentRhythmSectionIndex];
	const float BeatLength = CurrentSection.BeatLengthMS;
	if (BeatLength <= 0) return;
	if (NextBeatTimestampMS < InCurrentTimeMS - FMath::RoundToInt(BeatLength * 2)) {
		const float BeatsSinceAnchor = (InCurrentTimeMS - CurrentSection.AnchorTimestampMS) / BeatLength;
		const int32 CurrentBeatNumber = FMath::FloorToInt(BeatsSinceAnchor);
		NextBeatTimestampMS = CurrentSection.AnchorTimestampMS + FMath::RoundToInt((CurrentBeatNumber + 1) * BeatLength);
	}
	while (InCurrentTimeMS >= NextBeatTimestampMS) {
		OnBeatTriggered.Broadcast(NextBeatTimestampMS / 1000.0f);
		NextBeatTimestampMS += FMath::RoundToInt(BeatLength);
	}
}