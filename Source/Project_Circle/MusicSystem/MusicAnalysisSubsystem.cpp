#include "MusicAnalysisSubsystem.h"
#include "Algo/Sort.h"

// Helper function to get a percentile value from a sorted array of floats.
float GetPercentile(const TArray<float>& SortedData, float Percentile)
{
	if (SortedData.Num() == 0) return 0.f;
	int32 Index = FMath::Clamp(FMath::RoundToInt(SortedData.Num() * Percentile) - 1, 0, SortedData.Num() - 1);
	return SortedData[Index];
}

// CalculateAPS using confidence-weighted hit objects.
float CalculateWeightedAPS(const TArray<const FConfidentHitObject*>& HitObjects, float DurationSec, float BaseBeatLength)
{
	if (DurationSec <= 0 || HitObjects.Num() == 0) return 0.f;

	float TotalWeightedActions = 0;
	for (const FConfidentHitObject* HO : HitObjects)
	{
		float ActionValue = HO->Confidence;
		if (HO->CombinedHitSound & 2) ActionValue += 0.2f; // Whistle
		if (HO->CombinedHitSound & 8) ActionValue += 0.2f; // Clap
		if (HO->CombinedHitSound & 4) ActionValue += 0.6f; // Finish
		TotalWeightedActions += ActionValue;

		if (HO->HitObjectType & 2)
		{
			TotalWeightedActions += (ActionValue * HO->Repeats);
			const float SliderDuration = HO->SliderEndTimeMS - HO->TimestampMS;
			const float TickInterval = (BaseBeatLength > 0 && HO->SliderTickRate > 0) ? FMath::Max(20.f, BaseBeatLength / HO->SliderTickRate) : -1.f;
			if (SliderDuration > 0 && TickInterval > 0)
			{
				const float SinglePassDuration = SliderDuration / FMath::Max(1, HO->Repeats);
				int32 TickCount = FMath::FloorToInt(SinglePassDuration / TickInterval);
				if (FMath::IsNearlyEqual(SinglePassDuration, (float)(TickCount * TickInterval), 10.f)) TickCount = FMath::Max(0, TickCount - 1);
				if (TickCount > 0) TotalWeightedActions += TickCount * HO->Repeats;
			}
		}
	}
	return TotalWeightedActions / DurationSec;
}

// Overload for the primary map analysis which doesn't use confidence.
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

void UMusicAnalysisSubsystem::StartSongAnalysis(UDataTable* PrimaryDataTable, const TArray<UDataTable*>& AllSongDataTables, float DifficultyBias)
{
	if (!PrimaryDataTable || AllSongDataTables.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("MusicAnalysisSubsystem: A Primary Data Table and at least one Council Data Table must be provided."));
		return;
	}
	
	bAnalysisComplete = false;

	CurrentSongAnalysis = AnalyzeRhythmSections(PrimaryDataTable, AllSongDataTables, DifficultyBias);
	
	if (CurrentSongAnalysis.RhythmSections.Num() > 0)
	{
		bAnalysisComplete = true;

		// Reset runtime state
		NextEventIndex = 0;
		LastProcessedMusicProgressMs = -1;
		NextBeatTimestampMS = 0;
		CurrentRhythmSectionIndex = 0;
		CurrentBPM = 0.f;
		CurrentMeter = 4;
		CurrentBreakEndTimeMS = -1;
		NoteEventQueue.Empty();
	}
}

void UMusicAnalysisSubsystem::UpdateMusicTime(float CurrentTimeSeconds)
{
	if (!bAnalysisComplete) return;
	int32 CurrentTimeMs = FMath::RoundToInt(CurrentTimeSeconds * 1000.0f);
	if (CurrentTimeMs > LastProcessedMusicProgressMs)
	{
		ProcessMusicEvents();
		LastProcessedMusicProgressMs = CurrentTimeMs;
	}
}

bool UMusicAnalysisSubsystem::LoadCouncilData(const TArray<UDataTable*>& AllSongDataTables, float DifficultyBias)
{
	MasterUninheritedTimingPoints.Empty();
	MasterAudioBeats.Empty();
	ConfidentHitObjects.Empty();

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
					MasterAudioBeats.Add(Value);
					break;
				default: break;
			}
		});
	}

	TempConfidentHOMap.GenerateValueArray(ConfidentHitObjects);
	ConfidentHitObjects.Sort([](const FConfidentHitObject& A, const FConfidentHitObject& B) {
		return A.TimestampMS < B.TimestampMS;
	});

	return ConfidentHitObjects.Num() > 0;
}


FSongAnalysisResult UMusicAnalysisSubsystem::AnalyzeRhythmSections(UDataTable* PrimaryDataTable, const TArray<UDataTable*>& AllSongDataTables, float DifficultyBias)
{
	FSongAnalysisResult Result;
	
	// --- STEP 1: The King Decrees the Law (Structural Analysis on Primary Map) ---
	TArray<const FMusicData*> PrimaryHitObjects;
	TArray<FMusicData> PrimaryTimingPoints, PrimaryUninheritedTPs;
	TArray<const FMusicData*> PrimaryAllEvents;
	
	PrimaryDataTable->ForeachRow<FMusicData>("", [&](const FName&, const FMusicData& Value){
		PrimaryAllEvents.Add(&Value);
		if(Value.EntryType == EGameplayEntryType::HitObject) PrimaryHitObjects.Add(&Value);
		else if (Value.EntryType == EGameplayEntryType::TimingPoint)
		{
			PrimaryTimingPoints.Add(Value);
			if(Value.Uninherited == 1) PrimaryUninheritedTPs.Add(Value);
		}
	});

	if (PrimaryUninheritedTPs.Num() == 0 || PrimaryHitObjects.Num() == 0) return Result;

	TSet<int32> BoundarySet;
	BoundarySet.Add(0);
	for (const FMusicData& TP : PrimaryUninheritedTPs) BoundarySet.Add(TP.TimestampMS);

	bool bKiai = false;
	for (const FMusicData* Data : PrimaryAllEvents)
	{
		if (Data->EntryType == EGameplayEntryType::TimingPoint) { if (((Data->Effects & 1) != 0) != bKiai) { bKiai = !bKiai; BoundarySet.Add(Data->TimestampMS); } }
		else if (Data->EntryType == EGameplayEntryType::Break) { BoundarySet.Add(Data->TimestampMS); BoundarySet.Add(Data->BreakEndTimeMS); }
	}
	
	const int32 LookbackWindowMS = 4000, StepSizeMS = 500, SnapWindowMS = 250;
	const float DropThreshold = 0.5f, SpikeThreshold = 2.0f;
	TArray<int32> MajorBoundaries = BoundarySet.Array();
	MajorBoundaries.Sort();

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

			float CurrentWindowAPS = CalculateAPS(ImmediateObjects, StepSizeMS / 1000.f, CurrentBaseBeatLength, CurrentTickRate);
			if(LastWindowAPS < 0) LastWindowAPS = CalculateAPS(HistoryObjects, LookbackWindowMS / 1000.f, CurrentBaseBeatLength, CurrentTickRate);
			
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

	TArray<int32> FinalBoundaries = BoundarySet.Array();
	FinalBoundaries.Sort();
	FinalBoundaries.Add(PrimaryHitObjects.Last()->TimestampMS + 5000);


	// --- STEP 2: The Council Deliberates (Load ALL data for intensity profiling) ---
	if (!LoadCouncilData(AllSongDataTables, DifficultyBias)) return Result;

	TArray<FSectionProfileData> ProfileList;
	TArray<float> AllCouncilActionScores;
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
		
		float CouncilAPS = CalculateWeightedAPS(SectionHOs, DurationSec, Profile.BaseBeatLength);
		
		float AudioEnergy = 0.f;
		// Audio energy can be averaged from all difficulties
		// ... (Audio energy logic would go here if needed)

		Profile.HybridApsScore = CouncilAPS + AudioEnergy; // Simplified for now
		AllCouncilActionScores.Add(Profile.HybridApsScore);
		ProfileList.Add(Profile);
	}
	
	if (ProfileList.Num() == 0) return Result;

	// --- STEP 3: Final Judgment (Classify beat based on council's scores) ---
	AllCouncilActionScores.Sort();
	float HighThreshold = GetPercentile(AllCouncilActionScores, 0.70f);
	float VeryHighThreshold = GetPercentile(AllCouncilActionScores, 0.90f);

	UE_LOG(LogTemp, Warning, TEXT("--- DEEP RHYTHM ANALYSIS ---"));
	UE_LOG(LogTemp, Warning, TEXT("Calculated Unified Intensity Thresholds: High=%.3f, VeryHigh=%.3f"), HighThreshold, VeryHighThreshold);

	for (int32 i = 0; i < ProfileList.Num(); ++i) {
		FSectionProfileData& Profile = ProfileList[i];
		float GameplayBeatLength = Profile.BaseBeatLength;
		if (GameplayBeatLength <= 0) continue;
		
		float ScoreToCheck = Profile.HybridApsScore;
		
		FString BeatDivision = TEXT("Normal (x1)");
		if (ScoreToCheck >= VeryHighThreshold && VeryHighThreshold > 0.1f) { GameplayBeatLength /= 4.0f; BeatDivision = TEXT("Very High (x4)"); }
		else if (ScoreToCheck >= HighThreshold && HighThreshold > 0.1f) { GameplayBeatLength /= 2.0f; BeatDivision = TEXT("High (x2)"); }

		UE_LOG(LogTemp, Log, TEXT("Profile [%d] | Start: %dms | Unified Score: %.3f | Decision: %s -> New Beat Length: %.2fms"),
			i, Profile.StartTime, Profile.HybridApsScore, *BeatDivision, GameplayBeatLength);
		
		FGameplayRhythmSection FinalSection;
		FinalSection.StartTimeMS = Profile.StartTime;
		FinalSection.BeatLengthMS = GameplayBeatLength;
		FinalSection.BPM = (GameplayBeatLength > 0) ? 60000.0f / GameplayBeatLength : 0.f;
		
		int32 Anchor = 0;
		for (const FMusicData* HO : PrimaryHitObjects) { if (HO->TimestampMS >= Profile.StartTime) { Anchor = HO->TimestampMS; break; } }
		FinalSection.AnchorTimestampMS = Anchor > 0 ? Anchor : Profile.StartTime;

		Result.RhythmSections.Add(FinalSection);
	}
	UE_LOG(LogTemp, Warning, TEXT("--- END OF DEEP ANALYSIS ---"));
	
	// Final merge logic for very short sections
	if (Result.RhythmSections.Num() < 2) return Result;
	
	TArray<FGameplayRhythmSection> MergedSections;
	MergedSections.Add(Result.RhythmSections[0]);
	for (int32 i = 1; i < Result.RhythmSections.Num(); ++i) {
		FGameplayRhythmSection& Current = Result.RhythmSections[i];
		FGameplayRhythmSection& Last = MergedSections.Last();
		
		float BaseBeatForLast = 500;
		for (const FMusicData& TP : PrimaryUninheritedTPs) { if (TP.TimestampMS <= Last.StartTimeMS) BaseBeatForLast = TP.BeatLength; }
		const float MinDurationMS = FMath::Max(250.f, BaseBeatForLast * 2.0f);

		if (Current.StartTimeMS - Last.StartTimeMS < MinDurationMS) {
			if (Current.BPM > Last.BPM) { Last.BPM = Current.BPM; Last.BeatLengthMS = Current.BeatLengthMS; }
		} else if (!FMath::IsNearlyEqual(Current.BPM, Last.BPM, 1.f)) {
			MergedSections.Add(Current);
		}
	}
	Result.RhythmSections = MergedSections;

	// Build the final runtime event timeline using the PRIMARY map's data, as it's the ground truth.
	RuntimeEventTimeline.Empty();
	PrimaryDataTable->ForeachRow<FMusicData>("", [&](const FName&, const FMusicData& Value){ RuntimeEventTimeline.Add(Value); });
	RuntimeEventTimeline.Sort([](const FMusicData& A, const FMusicData& B) { return A.TimestampMS < B.TimestampMS; });
	
	return Result;
}

void UMusicAnalysisSubsystem::GenerateSliderSubEvents(const FMusicData& SliderData)
{
	if (!(SliderData.HitObjectType & 2)) return;
	float BaseBeatLength = 500.f;
	
	if (MasterUninheritedTimingPoints.Num() > 0) {
		TArray<FMusicData> TPs;
		MasterUninheritedTimingPoints.GenerateValueArray(TPs);
		TPs.Sort([](const FMusicData& A, const FMusicData& B){ return A.TimestampMS < B.TimestampMS; });
		
		for(int32 i = TPs.Num() - 1; i >= 0; --i) {
			if(TPs[i].TimestampMS <= SliderData.TimestampMS) {
				BaseBeatLength = TPs[i].BeatLength;
				break;
			}
		}
	}
	
	const float SliderDuration = SliderData.SliderEndTimeMS - SliderData.TimestampMS;
	const float TickInterval = (BaseBeatLength > 0 && SliderData.SliderTickRate > 0) ? FMath::Max(20.f, BaseBeatLength / SliderData.SliderTickRate) : -1.f;

	if (SliderDuration > 0 && TickInterval > 0) {
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

void UMusicAnalysisSubsystem::ProcessMusicEvents()
{
	int32 CurrentTimeMs = LastProcessedMusicProgressMs;
	if (!bAnalysisComplete) return;
	UpdateRhythmSection(CurrentTimeMs);
	ProcessBeatTicks(CurrentTimeMs);

	while (true)
	{
		const FMusicData* NextMajorEvent = (NextEventIndex < RuntimeEventTimeline.Num()) ? &RuntimeEventTimeline[NextEventIndex] : nullptr;
		FQueuedNoteEvent* NextSubEvent = (NoteEventQueue.Num() > 0) ? &NoteEventQueue.Last() : nullptr;
		int32 NextMajorEventTime = NextMajorEvent ? NextMajorEvent->TimestampMS : INT_MAX;
		int32 NextSubEventTime = NextSubEvent ? NextSubEvent->TimestampMS : INT_MAX;
		
		if (FMath::Min(NextMajorEventTime, NextSubEventTime) > CurrentTimeMs) break;

		if (NextMajorEventTime <= NextSubEventTime) {
			if (NextMajorEvent->EntryType == EGameplayEntryType::HitObject) {
				OnNoteHit.Broadcast(NextMajorEvent->TimestampMS, NextMajorEvent->HitObjectType, NextMajorEvent->HitSound);
				if (NextMajorEvent->HitObjectType & 2) GenerateSliderSubEvents(*NextMajorEvent);
			} else if (NextMajorEvent->EntryType == EGameplayEntryType::TimingPoint) {
				if(NextMajorEvent->Uninherited == 1 && NextMajorEvent->Meter != CurrentMeter) {
					CurrentMeter = NextMajorEvent->Meter; OnMeterChanged.Broadcast(CurrentMeter);
				}
			} else if (NextMajorEvent->EntryType == EGameplayEntryType::Break) {
				// Set the end time of the break we are now entering
				CurrentBreakEndTimeMS = NextMajorEvent->BreakEndTimeMS;
				OnBreakStart.Broadcast(NextMajorEvent->TimestampMS, NextMajorEvent->BreakEndTimeMS);
			}
			NextEventIndex++;
		} else {
			OnNoteHit.Broadcast(NextSubEvent->TimestampMS, NextSubEvent->NoteType, NextSubEvent->OriginalHitSound);
			NoteEventQueue.Pop();
		}
	}
	
	// Check if a break has just ended by comparing the old time and new time.
	if (CurrentBreakEndTimeMS > 0 && LastProcessedMusicProgressMs < CurrentBreakEndTimeMS && CurrentTimeMs >= CurrentBreakEndTimeMS)
	{
		// We don't need to find the event, we just know a break ended.
		// The specific start/end times aren't as important for the OnBreakEnd event.
		OnBreakEnd.Broadcast(0, CurrentBreakEndTimeMS); // We can pass dummy start time
	}
}

void UMusicAnalysisSubsystem::UpdateRhythmSection(int32 InCurrentTimeMS)
{
	if(CurrentSongAnalysis.RhythmSections.Num() == 0) return;
	
	int32 NewRhythmSectionIndex = CurrentRhythmSectionIndex;
	while (NewRhythmSectionIndex < CurrentSongAnalysis.RhythmSections.Num() - 1 && InCurrentTimeMS >= CurrentSongAnalysis.RhythmSections[NewRhythmSectionIndex + 1].StartTimeMS)
	{
		NewRhythmSectionIndex++;
	}

	if (NewRhythmSectionIndex != CurrentRhythmSectionIndex || CurrentBPM == 0.f) {
		CurrentRhythmSectionIndex = NewRhythmSectionIndex;
		const FGameplayRhythmSection& CurrentSection = CurrentSongAnalysis.RhythmSections[CurrentRhythmSectionIndex];
		if(!FMath::IsNearlyEqual(CurrentBPM, CurrentSection.BPM)) {
			CurrentBPM = CurrentSection.BPM;
			OnBPMChanged.Broadcast(CurrentBPM);
		}
		
		// Reset our beat counter for the new section
		CurrentBeatInSession = 0; 
		
		// Set the NextBeatTimestampMS to the anchor, but we will rely less on it now
		NextBeatTimestampMS = CurrentSection.AnchorTimestampMS;
	}
}

void UMusicAnalysisSubsystem::ProcessBeatTicks(int32 InCurrentTimeMS)
{
	// Check against the break end time. This is much more robust.
	if (InCurrentTimeMS < CurrentBreakEndTimeMS || CurrentSongAnalysis.RhythmSections.Num() == 0) return;

	const FGameplayRhythmSection& CurrentSection = CurrentSongAnalysis.RhythmSections[CurrentRhythmSectionIndex];
	const float BeatLength = CurrentSection.BeatLengthMS;
	const int32 Anchor = CurrentSection.AnchorTimestampMS;

	if (BeatLength <= 0 || InCurrentTimeMS < Anchor) return;

	// Lag-catchup logic: If we are way past the last beat, figure out where we should be.
	if (NextBeatTimestampMS < InCurrentTimeMS - FMath::RoundToInt(BeatLength * 4))
	{
		const float BeatsPassed = (InCurrentTimeMS - Anchor) / BeatLength;
		CurrentBeatInSession = FMath::FloorToInt(BeatsPassed) + 1;
	}

	// Calculate the precise timestamp of the next beat using drift-free logic
	NextBeatTimestampMS = Anchor + FMath::RoundToInt(CurrentBeatInSession * BeatLength);

	// Fire events for all beats that should have occurred by now
	while (InCurrentTimeMS >= NextBeatTimestampMS)
	{
		OnBeatTriggered.Broadcast(NextBeatTimestampMS / 1000.0f);
		
		// Increment the beat counter and calculate the NEXT beat's precise time
		CurrentBeatInSession++;
		NextBeatTimestampMS = Anchor + FMath::RoundToInt(CurrentBeatInSession * BeatLength);
	}
}