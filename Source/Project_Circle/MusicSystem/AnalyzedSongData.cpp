// Fill out your copyright notice in the Description page of Project Settings.

#include "AnalyzedSongData.h"
#include "Engine/DataTable.h"
#include "Algo/Sort.h"

// --- Helper Structs and Functions ---

// THIS IS THE FIX: The missing struct definition is now here.
// It's a private implementation detail of the analysis, so it belongs in the .cpp file.
struct FSectionProfileData
{
	int32 StartTime;
	int32 EndTime;
	float BaseBeatLength;
	float HybridApsScore;
};

namespace MusicAnalysisHelpers
{
	float CalculateWeightedAPS(const TArray<const FConfidentHitObject*>& HitObjects, float DurationSec, float BaseBeatLength)
	{
		if (DurationSec <= 0 || HitObjects.Num() == 0) return 0.f;
		float TotalWeightedActions = 0;
		for (const FConfidentHitObject* HO : HitObjects) {
			float ActionValue = HO->Confidence;
			if (HO->CombinedHitSound & 2) ActionValue += 0.2f; if (HO->CombinedHitSound & 8) ActionValue += 0.2f; if (HO->CombinedHitSound & 4) ActionValue += 0.6f;
			TotalWeightedActions += ActionValue;
			if (HO->HitObjectType & 2) {
				TotalWeightedActions += (ActionValue * HO->Repeats);
				const float SliderDuration = HO->SliderEndTimeMS - HO->TimestampMS;
				const float TickInterval = (BaseBeatLength > 0 && HO->SliderTickRate > 0) ? FMath::Max(20.f, BaseBeatLength / HO->SliderTickRate) : -1.f;
				if (SliderDuration > 0 && TickInterval > 0) {
					const float SinglePassDuration = SliderDuration / FMath::Max(1, HO->Repeats);
					int32 TickCount = FMath::FloorToInt(SinglePassDuration / TickInterval);
					if (FMath::IsNearlyEqual(SinglePassDuration, (float)(TickCount * TickInterval), 10.f)) TickCount = FMath::Max(0, TickCount - 1);
					if (TickCount > 0) TotalWeightedActions += TickCount * HO->Repeats;
				}
			}
		}
		return TotalWeightedActions / DurationSec;
	}

	float CalculateAPS(const TArray<const FMusicData*>& HitObjects, float DurationSec, float BaseBeatLength, float SliderTickRate)
	{
		if (DurationSec <= 0 || HitObjects.Num() == 0) return 0.f;
		float TotalActions = 0;
		for (const FMusicData* HO : HitObjects) {
			float ActionValue = 1.0f;
			if (HO->HitSound & 2) ActionValue += 0.2f; if (HO->HitSound & 8) ActionValue += 0.2f; if (HO->HitSound & 4) ActionValue += 0.6f;
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
}


// --- UAnalyzedSongData Implementation ---

UAnalyzedSongData* UAnalyzedSongData::CreateAnalyzedSongData(UObject* Outer, UDataTable* PrimaryDataTable, const TArray<UDataTable*>& AllSongDataTables, float DifficultyBias)
{
	if (!PrimaryDataTable || AllSongDataTables.Num() == 0 || !Outer)
	{
		UE_LOG(LogTemp, Error, TEXT("AnalyzedSongData: Cannot create, missing Primary Data Table, Council Data Tables, or Outer object."));
		return nullptr;
	}

	UAnalyzedSongData* NewAnalysis = NewObject<UAnalyzedSongData>(Outer);
	NewAnalysis->AnalyzeRhythmSections(PrimaryDataTable, AllSongDataTables, DifficultyBias);

	if (NewAnalysis->Result.RhythmSections.Num() > 0)
	{
		return NewAnalysis;
	}

	UE_LOG(LogTemp, Error, TEXT("AnalyzedSongData: Analysis failed to produce any rhythm sections."));
	return nullptr;
}

// THIS IS THE FIX: A new, cleaner helper function to gather all council data in one pass.
bool UAnalyzedSongData::GatherCouncilData(const TArray<UDataTable*>& AllSongDataTables, float DifficultyBias, TArray<FConfidentHitObject>& OutConfidentHitObjects)
{
	MasterUninheritedTimingPoints.Empty();
	OutConfidentHitObjects.Empty();

	TMap<int32, FConfidentHitObject> TempConfidentHOMap;
	const int32 NumDifficulties = AllSongDataTables.Num();

	for (int32 i = 0; i < NumDifficulties; ++i)
	{
		UDataTable* Table = AllSongDataTables[i];
		if (!Table) continue;
		
		const float NormalizedPosition = (NumDifficulties > 1) ? static_cast<float>(i) / (NumDifficulties - 1.0f) : 0.5f;
		const float DifficultyWeight = 1.0f + (NormalizedPosition - 0.5f) * 2.0f * FMath::Clamp(DifficultyBias, -1.0f, 1.0f);

		Table->ForeachRow<FMusicData>(TEXT("Loading Council Data"), [&](const FName& Key, const FMusicData& Value)
		{
			switch (Value.EntryType)
			{
				case EGameplayEntryType::TimingPoint:
					if (Value.Uninherited == 1) MasterUninheritedTimingPoints.FindOrAdd(Value.TimestampMS, Value);
					break;
				case EGameplayEntryType::HitObject:
				{
					FConfidentHitObject& ConfidentHO = TempConfidentHOMap.FindOrAdd(Value.TimestampMS);
					ConfidentHO.TimestampMS = Value.TimestampMS;
					ConfidentHO.Confidence += DifficultyWeight; 
					ConfidentHO.CombinedHitSound |= Value.HitSound;
					ConfidentHO.HitObjectType = Value.HitObjectType;
					ConfidentHO.Repeats = Value.Repeats;
					ConfidentHO.SliderEndTimeMS = Value.SliderEndTimeMS;
					ConfidentHO.SliderTickRate = Value.SliderTickRate;
					break;
				}
				case EGameplayEntryType::AudioBeat:
				default: 
					break;
			}
		});
	}

	TempConfidentHOMap.GenerateValueArray(OutConfidentHitObjects);
	OutConfidentHitObjects.Sort([](const FConfidentHitObject& A, const FConfidentHitObject& B) {
		return A.TimestampMS < B.TimestampMS;
	});

	return OutConfidentHitObjects.Num() > 0;
}

void UAnalyzedSongData::AnalyzeRhythmSections(UDataTable* PrimaryDataTable, const TArray<UDataTable*>& AllSongDataTables, float DifficultyBias)
{
	// --- STEP 1: The King Decrees the Law (Structural Analysis on Primary Map) ---
	TArray<const FMusicData*> PrimaryHitObjects;
	TArray<FMusicData> PrimaryUninheritedTPs;
	TArray<const FMusicData*> PrimaryAllEvents;
	
	PrimaryDataTable->ForeachRow<FMusicData>("", [&](const FName&, const FMusicData& Value){
		PrimaryAllEvents.Add(&Value);
		if(Value.EntryType == EGameplayEntryType::HitObject) PrimaryHitObjects.Add(&Value);
		else if (Value.EntryType == EGameplayEntryType::TimingPoint && Value.Uninherited == 1) PrimaryUninheritedTPs.Add(Value);
	});

	if (PrimaryUninheritedTPs.Num() == 0 || PrimaryHitObjects.Num() == 0) return;

	TSet<int32> BoundarySet;
	BoundarySet.Add(0);
	for (const FMusicData& TP : PrimaryUninheritedTPs) BoundarySet.Add(TP.TimestampMS);

	bool bKiai = false;
	int32 PrimaryMaxEventTime = 0;
	for (const FMusicData* Data : PrimaryAllEvents) {
		if (Data->EntryType == EGameplayEntryType::TimingPoint) {
			if (((Data->Effects & 1) != 0) != bKiai) { bKiai = !bKiai; BoundarySet.Add(Data->TimestampMS); }
			PrimaryMaxEventTime = FMath::Max(PrimaryMaxEventTime, Data->TimestampMS);
		} else if (Data->EntryType == EGameplayEntryType::Break) {
			BoundarySet.Add(Data->TimestampMS); BoundarySet.Add(Data->BreakEndTimeMS);
			PrimaryMaxEventTime = FMath::Max(PrimaryMaxEventTime, Data->BreakEndTimeMS);
		} else if(Data->EntryType == EGameplayEntryType::HitObject) {
			PrimaryMaxEventTime = FMath::Max(PrimaryMaxEventTime, Data->TimestampMS);
		}
	}
	
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
			for(const FMusicData* HO : PrimaryHitObjects) {
				if (HO->TimestampMS >= CurrentTime - LookbackWindowMS && HO->TimestampMS < CurrentTime) HistoryObjects.Add(HO);
				if (HO->TimestampMS >= CurrentTime && HO->TimestampMS < CurrentTime + StepSizeMS) ImmediateObjects.Add(HO);
			}
			if(HistoryObjects.Num() < 3) continue;
			
			for(const FMusicData& TP : PrimaryUninheritedTPs) { if(TP.TimestampMS <= CurrentTime) CurrentBaseBeatLength = TP.BeatLength; }
			if (HistoryObjects.Num() > 0 && HistoryObjects[0]->SliderTickRate > 0) CurrentTickRate = HistoryObjects[0]->SliderTickRate;

			float CurrentWindowAPS = MusicAnalysisHelpers::CalculateAPS(ImmediateObjects, StepSizeMS / 1000.f, CurrentBaseBeatLength, CurrentTickRate);
			if(LastWindowAPS < 0) LastWindowAPS = MusicAnalysisHelpers::CalculateAPS(HistoryObjects, LookbackWindowMS / 1000.f, CurrentBaseBeatLength, CurrentTickRate);
			
			bool bChangeDetected = (LastWindowAPS > 1.0f && CurrentWindowAPS < LastWindowAPS * DropThreshold) || (CurrentWindowAPS > FMath::Max(1.0f, LastWindowAPS) * SpikeThreshold);
			if (bChangeDetected) {
				int32 BestSnapTime = -1;
				for(const FMusicData* HO : PrimaryHitObjects) {
					if(HO->TimestampMS >= CurrentTime - SnapWindowMS && HO->TimestampMS < CurrentTime + StepSizeMS + SnapWindowMS) {
						if (BestSnapTime == -1 || FMath::Abs(HO->TimestampMS - CurrentTime) < FMath::Abs(BestSnapTime - CurrentTime)) BestSnapTime = HO->TimestampMS;
					}
				}
				if(BestSnapTime != -1) BoundarySet.Add(BestSnapTime); else BoundarySet.Add(CurrentTime);
				CurrentTime += LookbackWindowMS; LastWindowAPS = -1.f;
			} else { LastWindowAPS = (LastWindowAPS * 0.7f) + (CurrentWindowAPS * 0.3f); }
		}
	}

	// --- STEP 2: The Council Deliberates & Regency ---
    TArray<FConfidentHitObject> ConfidentHitObjects;
    if (!GatherCouncilData(AllSongDataTables, DifficultyBias, ConfidentHitObjects))
    {
        // No confident hit objects found, cannot proceed with analysis.
        return;
    }

	const int32 AbsoluteSongEnd = ConfidentHitObjects.Last().TimestampMS;
	this->AbsoluteSongEndTimeMS = AbsoluteSongEnd + 2000;

	if (AbsoluteSongEnd > PrimaryMaxEventTime + LookbackWindowMS)
	{
		float LastWindowWAPS = -1.f;
		for (int32 CurrentTime = PrimaryMaxEventTime + LookbackWindowMS; CurrentTime < AbsoluteSongEnd; CurrentTime += StepSizeMS)
		{
			TArray<const FConfidentHitObject*> HistoryObjects, ImmediateObjects;
			for(const FConfidentHitObject& HO : ConfidentHitObjects) {
				if (HO.TimestampMS >= CurrentTime - LookbackWindowMS && HO.TimestampMS < CurrentTime) HistoryObjects.Add(&HO);
				if (HO.TimestampMS >= CurrentTime && HO.TimestampMS < CurrentTime + StepSizeMS) ImmediateObjects.Add(&HO);
			}
			if(HistoryObjects.Num() == 0) continue;

			float CurrentBaseBeatLength = 500.f;
			for(const auto& Elem : MasterUninheritedTimingPoints) { if(Elem.Value.TimestampMS <= CurrentTime) CurrentBaseBeatLength = Elem.Value.BeatLength; }

			float CurrentWindowWAPS = MusicAnalysisHelpers::CalculateWeightedAPS(ImmediateObjects, StepSizeMS / 1000.f, CurrentBaseBeatLength);
			if(LastWindowWAPS < 0) LastWindowWAPS = MusicAnalysisHelpers::CalculateWeightedAPS(HistoryObjects, LookbackWindowMS / 1000.f, CurrentBaseBeatLength);
			
			bool bChangeDetected = (LastWindowWAPS > 1.0f && CurrentWindowWAPS < LastWindowWAPS * DropThreshold) || (CurrentWindowWAPS > FMath::Max(1.0f, LastWindowWAPS) * SpikeThreshold);
			if(bChangeDetected) {
				BoundarySet.Add(CurrentTime);
				CurrentTime += LookbackWindowMS; LastWindowWAPS = -1.f;
			} else { LastWindowWAPS = (LastWindowWAPS * 0.7f) + (CurrentWindowWAPS * 0.3f); }
		}
	}

	TArray<int32> FinalBoundaries = BoundarySet.Array();
	FinalBoundaries.Sort();
	FinalBoundaries.AddUnique(AbsoluteSongEnd + 5000);

	// --- STEP 3: Unified Judgment and Profiling ---
	TArray<FSectionProfileData> ProfileList;
	float LastValidBaseBeatLength = (PrimaryUninheritedTPs.Num() > 0) ? PrimaryUninheritedTPs[0].BeatLength : 500.f;

	for (int32 i = 0; i < FinalBoundaries.Num() - 1; ++i) {
		FSectionProfileData Profile;
		Profile.StartTime = FinalBoundaries[i];
		Profile.EndTime = FinalBoundaries[i+1];
		const float DurationSec = (Profile.EndTime - Profile.StartTime) / 1000.f;
		if (DurationSec < 0.25f) continue;
		
		for (const FMusicData& TP : PrimaryUninheritedTPs) { if (TP.TimestampMS <= Profile.StartTime) Profile.BaseBeatLength = TP.BeatLength; }
		if (Profile.BaseBeatLength <= 0) Profile.BaseBeatLength = LastValidBaseBeatLength; else LastValidBaseBeatLength = Profile.BaseBeatLength;

		TArray<const FConfidentHitObject*> SectionHOs;
		for (const FConfidentHitObject& HO : ConfidentHitObjects) { if (HO.TimestampMS >= Profile.StartTime && HO.TimestampMS < Profile.EndTime) SectionHOs.Add(&HO); }
		
		Profile.HybridApsScore = MusicAnalysisHelpers::CalculateWeightedAPS(SectionHOs, DurationSec, Profile.BaseBeatLength);
		ProfileList.Add(Profile);
	}
	
	if (ProfileList.Num() == 0) return;
	
	float MinScore = -1.f, MaxScore = 0.f;
	for(const FSectionProfileData& Profile : ProfileList) {
		if (Profile.HybridApsScore > 0.01f) {
			if (MinScore < 0 || Profile.HybridApsScore < MinScore) MinScore = Profile.HybridApsScore;
			if (Profile.HybridApsScore > MaxScore) MaxScore = Profile.HybridApsScore;
		}
	}
	const float ScoreRange = (MaxScore - MinScore > 0.01f) ? (MaxScore - MinScore) : 1.0f;
	
	UE_LOG(LogTemp, Warning, TEXT("--- DEEP RHYTHM ANALYSIS (NORMALIZED) ---"));
	UE_LOG(LogTemp, Warning, TEXT("Score Range: Min=%.3f, Max=%.3f"), MinScore, MaxScore);
	
	TArray<FGameplayRhythmSection> TempSections;
	for (int32 i = 0; i < ProfileList.Num(); ++i) {
		FSectionProfileData& Profile = ProfileList[i];
		float GameplayBeatLength = Profile.BaseBeatLength;
		if (GameplayBeatLength <= 0) continue;

		const float NormalizedScore = (Profile.HybridApsScore > 0.01f) ? (Profile.HybridApsScore - MinScore) / ScoreRange : 0.0f;
		FString BeatDivision = TEXT("Normal (x1)");
		if (NormalizedScore >= 0.85f) { GameplayBeatLength /= 4.0f; BeatDivision = TEXT("Very High (x4)"); }
		else if (NormalizedScore >= 0.60f) { GameplayBeatLength /= 2.0f; BeatDivision = TEXT("High (x2)"); }

		UE_LOG(LogTemp, Log, TEXT("Profile [%d] | Start: %dms | Raw Score: %.3f | Norm Score: %.3f | Decision: %s -> New Beat Length: %.2fms"),
			i, Profile.StartTime, Profile.HybridApsScore, NormalizedScore, *BeatDivision, GameplayBeatLength);
		
		FGameplayRhythmSection FinalSection;
		FinalSection.StartTimeMS = Profile.StartTime;
		FinalSection.BeatLengthMS = GameplayBeatLength;
		FinalSection.BPM = (GameplayBeatLength > 0) ? 60000.0f / GameplayBeatLength : 0.f;
		
		int32 Anchor = 0;
		for (const FMusicData* HO : PrimaryHitObjects) { if (HO->TimestampMS >= Profile.StartTime) { Anchor = HO->TimestampMS; break; } }
		FinalSection.AnchorTimestampMS = Anchor > 0 ? Anchor : Profile.StartTime;
		TempSections.Add(FinalSection);
	}
	UE_LOG(LogTemp, Warning, TEXT("--- END OF DEEP ANALYSIS ---"));
	
	if (TempSections.Num() == 0) return;
	
	Result.RhythmSections.Add(TempSections[0]);
	for (int32 i = 1; i < TempSections.Num(); ++i) {
		FGameplayRhythmSection& Current = TempSections[i];
		FGameplayRhythmSection& Last = Result.RhythmSections.Last();
		
		float BaseBeatForLast = 500;
		for (const FMusicData& TP : PrimaryUninheritedTPs) { if (TP.TimestampMS <= Last.StartTimeMS) BaseBeatForLast = TP.BeatLength; }
		const float MinDurationMS = FMath::Max(250.f, BaseBeatForLast * 2.0f);

		if (Current.StartTimeMS - Last.StartTimeMS < MinDurationMS) {
			if (Current.BPM > Last.BPM) { Last.BPM = Current.BPM; Last.BeatLengthMS = Current.BeatLengthMS; }
		} else if (!FMath::IsNearlyEqual(Current.BPM, Last.BPM, 1.f)) {
			Result.RhythmSections.Add(Current);
		}
	}

	// Finally, populate the runtime event timeline for the subsystem to use.
	RuntimeEventTimeline.Empty();
	PrimaryDataTable->ForeachRow<FMusicData>("", [&](const FName&, const FMusicData& Value){ RuntimeEventTimeline.Add(Value); });
	RuntimeEventTimeline.Sort([](const FMusicData& A, const FMusicData& B) { return A.TimestampMS < B.TimestampMS; });
}