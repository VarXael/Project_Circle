#include "PcMusicAnalyzer.h"
#include "PcMusicConfigurationData.h"

namespace MusicAnalysisHelpers
{
	float CalculateWeightedAPS(const TArray<const FPcConfidentHitObject*>& HitObjects, float DurationSec, float BaseBeatLength)
	{
		if (DurationSec <= 0 || HitObjects.Num() == 0) return 0.f;
		float TotalWeightedActions = 0;
		for (const auto* HO : HitObjects) {
			float ActionValue = HO->Confidence;
			if (HO->CombinedHitSound & 2) ActionValue += 0.2f;
			if (HO->CombinedHitSound & 8) ActionValue += 0.2f;
			if (HO->CombinedHitSound & 4) ActionValue += 0.6f;
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

	float CalculateAPS(const TArray<const FPcImportedMusicData*>& HitObjects, float DurationSec, float BaseBeatLength, float SliderTickRate)
	{
		if (DurationSec <= 0 || HitObjects.Num() == 0) return 0.f;
		float TotalActions = 0;
		for (const auto* HO : HitObjects) {
			float ActionValue = 1.0f;
			if (HO->HitSound & 2) ActionValue += 0.2f;
			if (HO->HitSound & 8) ActionValue += 0.2f;
			if (HO->HitSound & 4) ActionValue += 0.6f;
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

UPcMusicAnalyzer* UPcMusicAnalyzer::RunSongAnalysis(UObject* Outer, UPcMusicConfigurationData* SongConfig)
{
	if (!Outer || !SongConfig || !SongConfig->GameplayMap) return nullptr;

	FPcSongAnalysisParameters Params;
	Params.DifficultyBias = SongConfig->DifficultyBias;
	Params.GameplayMap = SongConfig->GameplayMap;
	Params.WeightedAnalysisMaps.Add(SongConfig->GameplayMap);
	
	for (UDataTable* Table : SongConfig->AdditionalAnalysisMaps)
		if (Table && Table != SongConfig->GameplayMap) Params.WeightedAnalysisMaps.Add(Table);

	Params.StructuralBaseMap = SongConfig->StructuralAnalysisBaseMapOverride ? SongConfig->StructuralAnalysisBaseMapOverride : SongConfig->GameplayMap;

	UPcMusicAnalyzer* Analyzer = NewObject<UPcMusicAnalyzer>(Outer);
	Analyzer->AnalyzeRhythmSections(Params);
	Analyzer->FlattenAndUnrollEvents(Params.GameplayMap);
	return Analyzer;
}

void UPcMusicAnalyzer::FlattenAndUnrollEvents(UDataTable* GameplayMap)
{
	FlattenedRuntimeEvents.Empty();
	int32 AbsoluteSongEnd = 0;

	TArray<FPcImportedMusicData*> AllRows;
	GameplayMap->GetAllRows("", AllRows);

	for (const FPcImportedMusicData* Row : AllRows)
	{
		AbsoluteSongEnd = FMath::Max(AbsoluteSongEnd, Row->TimestampMS);
		AbsoluteSongEnd = FMath::Max(AbsoluteSongEnd, Row->BreakEndTimeMS);
		AbsoluteSongEnd = FMath::Max(AbsoluteSongEnd, Row->SliderEndTimeMS);

		if (Row->EntryType == EPcGameplayEntryType::TimingPoint && Row->Uninherited == 1)
		{
			FPcRuntimeEvent Ev;
			Ev.TimestampMS = Row->TimestampMS;
			Ev.EventType = EPcRuntimeEventType::MeterChange;
			Ev.Value1 = Row->Meter;
			FlattenedRuntimeEvents.Add(Ev);
		}
		else if (Row->EntryType == EPcGameplayEntryType::Break)
		{
			FPcRuntimeEvent StartEv, EndEv;
			StartEv.TimestampMS = Row->TimestampMS; StartEv.EventType = EPcRuntimeEventType::BreakStart; StartEv.Value1 = Row->BreakEndTimeMS;
			EndEv.TimestampMS = Row->BreakEndTimeMS; EndEv.EventType = EPcRuntimeEventType::BreakEnd; EndEv.Value1 = Row->TimestampMS;
			FlattenedRuntimeEvents.Add(StartEv);
			FlattenedRuntimeEvents.Add(EndEv);
		}
		else if (Row->EntryType == EPcGameplayEntryType::HitObject)
		{
			FPcRuntimeEvent Hit;
			Hit.TimestampMS = Row->TimestampMS;
			Hit.EventType = EPcRuntimeEventType::NoteHit;
			Hit.Value1 = Row->HitObjectType;
			Hit.Value2 = Row->HitSound;
			FlattenedRuntimeEvents.Add(Hit);

			if (Row->HitObjectType & 2)
			{
				float BaseBeatLength = 500.f;
				int32 BestTPTime = -1;
				for (const auto& Elem : MasterUninheritedTimingPoints) {
					if (Elem.Key <= Row->TimestampMS && Elem.Key > BestTPTime) {
						BaseBeatLength = Elem.Value.BeatLength;
						BestTPTime = Elem.Key;
					}
				}

				const float SliderDuration = Row->SliderEndTimeMS - Row->TimestampMS;
				const float TickInterval = (BaseBeatLength > 0 && Row->SliderTickRate > 0) ? FMath::Max(20.f, BaseBeatLength / Row->SliderTickRate) : -1.f;

				if (SliderDuration > 0 && TickInterval > 0)
				{
					const float SinglePassDuration = SliderDuration / FMath::Max(1, Row->Repeats);
					for (int32 Pass = 0; Pass < Row->Repeats; ++Pass) {
						for (float T = TickInterval; T < SinglePassDuration; T += TickInterval) {
							if (!FMath::IsNearlyEqual(T, SinglePassDuration, 1.f)) {
								FPcRuntimeEvent Tick;
								Tick.TimestampMS = Row->TimestampMS + FMath::RoundToInt((Pass * SinglePassDuration) + T);
								Tick.EventType = EPcRuntimeEventType::NoteHit;
								Tick.Value1 = 128; 
								Tick.Value2 = Row->HitSound;
								FlattenedRuntimeEvents.Add(Tick);
							}
						}
					}
					for (int32 Repeat = 1; Repeat <= Row->Repeats; ++Repeat) {
						FPcRuntimeEvent Tail;
						Tail.TimestampMS = Row->TimestampMS + FMath::RoundToInt(Repeat * SinglePassDuration);
						Tail.EventType = EPcRuntimeEventType::NoteHit;
						Tail.Value1 = 256; 
						Tail.Value2 = Row->HitSound;
						FlattenedRuntimeEvents.Add(Tail);
					}
				}
			}
		}
	}

	FPcRuntimeEvent EndEv;
	EndEv.TimestampMS = AbsoluteSongEnd + 2000;
	EndEv.EventType = EPcRuntimeEventType::SongEnd;
	FlattenedRuntimeEvents.Add(EndEv);

	// FIX: Replaced `<` with `>` in the opposite order to fix the HTML parsing bug in chat!
	FlattenedRuntimeEvents.Sort([](const FPcRuntimeEvent& A, const FPcRuntimeEvent& B) { return B.TimestampMS > A.TimestampMS; });
}

void UPcMusicAnalyzer::AnalyzeRhythmSections(const FPcSongAnalysisParameters& Parameters)
{
	TArray<const FPcImportedMusicData*> StructuralHitObjects;
	TArray<FPcImportedMusicData> StructuralUninheritedTPs;
	TArray<const FPcImportedMusicData*> StructuralAllEvents;

	Parameters.StructuralBaseMap->ForeachRow<FPcImportedMusicData>("Populating", [&](const FName&, const FPcImportedMusicData& Value) {
		StructuralAllEvents.Add(&Value);
		if (Value.EntryType == EPcGameplayEntryType::HitObject) StructuralHitObjects.Add(&Value);
		else if (Value.EntryType == EPcGameplayEntryType::TimingPoint && Value.Uninherited == 1) StructuralUninheritedTPs.Add(Value);
	});

	if (StructuralUninheritedTPs.Num() == 0 || StructuralHitObjects.Num() == 0) return;

	TArray<FPcConfidentHitObject> ConfidentHitObjects;
	if (!GatherCouncilData(Parameters.WeightedAnalysisMaps, Parameters.DifficultyBias, ConfidentHitObjects)) return;

	TSet<int32> BoundarySet;
	BoundarySet.Add(0);
	for (const auto& TP : StructuralUninheritedTPs) BoundarySet.Add(TP.TimestampMS);

	bool bKiai = false;
	int32 StructuralMaxEventTime = 0;
	TArray<TPair<int32, bool>> KiaiChanges; // {timestampMS, bKiaiOn} — used to tag Enhanced sections
	
	for (const auto* Data : StructuralAllEvents) {
		if (Data->EntryType == EPcGameplayEntryType::TimingPoint) {
			if (((Data->Effects & 1) != 0) != bKiai) {
				bKiai = !bKiai;
				BoundarySet.Add(Data->TimestampMS);
				KiaiChanges.Add({Data->TimestampMS, bKiai});
			}
			StructuralMaxEventTime = FMath::Max(StructuralMaxEventTime, Data->TimestampMS);
		} else if (Data->EntryType == EPcGameplayEntryType::Break) {
			BoundarySet.Add(Data->TimestampMS);
			BoundarySet.Add(Data->BreakEndTimeMS);
			StructuralMaxEventTime = FMath::Max(StructuralMaxEventTime, Data->BreakEndTimeMS);
		} else if (Data->EntryType == EPcGameplayEntryType::HitObject) {
			StructuralMaxEventTime = FMath::Max(StructuralMaxEventTime, Data->TimestampMS);
		}
	}

	TArray<int32> MajorBoundaries = BoundarySet.Array();
	MajorBoundaries.Sort();
	const int32 LookbackWindowMS = 4000, StepSizeMS = 500, SnapWindowMS = 250;
	const float DropThreshold = 0.5f, SpikeThreshold = 2.0f;

	for (int32 i = 0; i < MajorBoundaries.Num() - 1; ++i) {
		const int32 SectionStart = MajorBoundaries[i], SectionEnd = MajorBoundaries[i + 1];
		if (SectionEnd - SectionStart < LookbackWindowMS + StepSizeMS) continue;

		float LastWindowAPS = -1.f;
		for (int32 CurrentTime = SectionStart + LookbackWindowMS; CurrentTime < SectionEnd; CurrentTime += StepSizeMS) {
			TArray<const FPcImportedMusicData*> HistoryObjects, ImmediateObjects;
			float CurrentBaseBeatLength = 500.f, CurrentTickRate = 1.0f;
			
			for (const auto* HO : StructuralHitObjects) {
				if (HO->TimestampMS >= CurrentTime - LookbackWindowMS && HO->TimestampMS < CurrentTime) HistoryObjects.Add(HO);
				if (HO->TimestampMS >= CurrentTime && HO->TimestampMS < CurrentTime + StepSizeMS) ImmediateObjects.Add(HO);
			}
			
			if (HistoryObjects.Num() < 3) continue;

			for (const auto& TP : StructuralUninheritedTPs) if (TP.TimestampMS <= CurrentTime) CurrentBaseBeatLength = TP.BeatLength;
			if (HistoryObjects.Num() > 0 && HistoryObjects[0]->SliderTickRate > 0) CurrentTickRate = HistoryObjects[0]->SliderTickRate;

			float CurrentWindowAPS = MusicAnalysisHelpers::CalculateAPS(ImmediateObjects, StepSizeMS / 1000.f, CurrentBaseBeatLength, CurrentTickRate);
			if (LastWindowAPS < 0) LastWindowAPS = MusicAnalysisHelpers::CalculateAPS(HistoryObjects, LookbackWindowMS / 1000.f, CurrentBaseBeatLength, CurrentTickRate);

			bool bChangeDetected = (LastWindowAPS > 1.0f && CurrentWindowAPS < LastWindowAPS * DropThreshold) || (CurrentWindowAPS > FMath::Max(1.0f, LastWindowAPS) * SpikeThreshold);
			if (bChangeDetected) {
				int32 BestSnapTime = -1;
				for (const auto* HO : StructuralHitObjects) {
					if (HO->TimestampMS >= CurrentTime - SnapWindowMS && HO->TimestampMS < CurrentTime + StepSizeMS + SnapWindowMS) {
						if (BestSnapTime == -1 || FMath::Abs(HO->TimestampMS - CurrentTime) < FMath::Abs(BestSnapTime - CurrentTime)) BestSnapTime = HO->TimestampMS;
					}
				}
				if (BestSnapTime != -1) BoundarySet.Add(BestSnapTime);
				else BoundarySet.Add(CurrentTime);
				
				CurrentTime += LookbackWindowMS;
				LastWindowAPS = -1.f;
			} else {
				LastWindowAPS = (LastWindowAPS * 0.7f) + (CurrentWindowAPS * 0.3f);
			}
		}
	}

	int32 AbsoluteSongEnd = ConfidentHitObjects.Last().TimestampMS;
	BoundarySet.Add(AbsoluteSongEnd + 5000);
	
	TArray<int32> FinalBoundaries = BoundarySet.Array();
	FinalBoundaries.Sort();

	struct FSectionProfileData { int32 StartTime = 0; int32 EndTime = 0; float BaseBeatLength = 0.f; float HybridApsScore = 0.f; };
	TArray<FSectionProfileData> ProfileList;
	float LastValidBaseBeatLength = StructuralUninheritedTPs[0].BeatLength;

	for (int32 i = 0; i < FinalBoundaries.Num() - 1; ++i) {
		FSectionProfileData Profile;
		Profile.StartTime = FinalBoundaries[i];
		Profile.EndTime = FinalBoundaries[i + 1];
		float DurationSec = (Profile.EndTime - Profile.StartTime) / 1000.f;
		if (DurationSec < 0.25f) continue;

		for (const auto& TP : StructuralUninheritedTPs) if (TP.TimestampMS <= Profile.StartTime) Profile.BaseBeatLength = TP.BeatLength;
		if (Profile.BaseBeatLength <= 0) Profile.BaseBeatLength = LastValidBaseBeatLength;
		else LastValidBaseBeatLength = Profile.BaseBeatLength;

		TArray<const FPcConfidentHitObject*> SectionHOs;
		for (const auto& HO : ConfidentHitObjects) {
			if (HO.TimestampMS >= Profile.StartTime && HO.TimestampMS < Profile.EndTime) SectionHOs.Add(&HO);
		}

		Profile.HybridApsScore = MusicAnalysisHelpers::CalculateWeightedAPS(SectionHOs, DurationSec, Profile.BaseBeatLength);
		ProfileList.Add(Profile);
	}

	float MinScore = -1.f, MaxScore = 0.f;
	for (const auto& P : ProfileList) {
		if (P.HybridApsScore > 0.01f) {
			if (MinScore < 0 || P.HybridApsScore < MinScore) MinScore = P.HybridApsScore;
			if (P.HybridApsScore > MaxScore) MaxScore = P.HybridApsScore;
		}
	}
	float ScoreRange = FMath::Max(MaxScore - MinScore, 1.0f);

	TArray<FPcRhythmSectionProfile> TempSections;
	for (const auto& Profile : ProfileList) {
		float GameplayBeatLength = Profile.BaseBeatLength;
		if (GameplayBeatLength <= 0) continue;

		float NormalizedScore = (Profile.HybridApsScore > 0.01f) ? (Profile.HybridApsScore - MinScore) / ScoreRange : 0.0f;
		if (NormalizedScore >= 0.85f) GameplayBeatLength /= 4.0f;
		else if (NormalizedScore >= 0.60f) GameplayBeatLength /= 2.0f;

		FPcRhythmSectionProfile Sec;
		Sec.StartTimeMS = Profile.StartTime;
		Sec.BeatLengthMS = GameplayBeatLength;
		Sec.BPM = GameplayBeatLength > 0 ? 60000.f / GameplayBeatLength : 0.f;
		
		int32 Anchor = 0;
		for (const auto* HO : StructuralHitObjects) {
			if (HO->TimestampMS >= Profile.StartTime) { Anchor = HO->TimestampMS; break; }
		}
		Sec.AnchorTimestampMS = Anchor > 0 ? Anchor : Profile.StartTime;

		// Tag as Enhanced if this section falls within kiai time.
		// KiaiChanges is sorted chronologically — walk it to find the state at section start.
		bool bSectionIsKiai = false;
		for (const auto& Change : KiaiChanges)
		{
			if (Change.Key <= Profile.StartTime) bSectionIsKiai = Change.Value;
			else break;
		}
		Sec.MovementPreset = bSectionIsKiai
			? EPcMovementPresetOverride::Enhanced
			: EPcMovementPresetOverride::Auto;
		TempSections.Add(Sec);
	}

	if (TempSections.Num() == 0) return;

	RhythmSections.Add(TempSections[0]);
	for (int32 i = 1; i < TempSections.Num(); ++i) {
		FPcRhythmSectionProfile& Current = TempSections[i];
		FPcRhythmSectionProfile& Last = RhythmSections.Last();

		float BaseBeatForLast = 500;
		for (const auto& TP : StructuralUninheritedTPs) if (TP.TimestampMS <= Last.StartTimeMS) BaseBeatForLast = TP.BeatLength;
		float MinDurationMS = FMath::Max(250.f, BaseBeatForLast * 2.0f);

		if (Current.StartTimeMS - Last.StartTimeMS < MinDurationMS) {
			if (Current.BPM > Last.BPM) {
				Last.BPM = Current.BPM;
				Last.BeatLengthMS = Current.BeatLengthMS;
			}
		} else {
			RhythmSections.Add(Current);
		}
	}
}

bool UPcMusicAnalyzer::GatherCouncilData(const TArray<UDataTable*>& WeightedMaps, float Bias, TArray<FPcConfidentHitObject>& OutObjects)
{
	MasterUninheritedTimingPoints.Empty();
	OutObjects.Empty();
	TMap<int32, FPcConfidentHitObject> TempMap;

	for (UDataTable* Table : WeightedMaps) {
		if (!Table) continue;
		Table->ForeachRow<FPcImportedMusicData>("", [&](const FName&, const FPcImportedMusicData& Value) {
			if (Value.EntryType == EPcGameplayEntryType::TimingPoint && Value.Uninherited == 1)
				MasterUninheritedTimingPoints.FindOrAdd(Value.TimestampMS, Value);
			else if (Value.EntryType == EPcGameplayEntryType::HitObject) {
				FPcConfidentHitObject& HO = TempMap.FindOrAdd(Value.TimestampMS);
				HO.TimestampMS = Value.TimestampMS; HO.Confidence += 1.f; HO.CombinedHitSound |= Value.HitSound;
				HO.HitObjectType = Value.HitObjectType; HO.Repeats = Value.Repeats; HO.SliderEndTimeMS = Value.SliderEndTimeMS; HO.SliderTickRate = Value.SliderTickRate;
			}
		});
	}
	TempMap.GenerateValueArray(OutObjects);
	
	// FIX: Replaced `<` with `>` in the opposite order to fix the HTML parsing bug in chat!
	OutObjects.Sort([](const FPcConfidentHitObject& A, const FPcConfidentHitObject& B) { return B.TimestampMS > A.TimestampMS; });
	
	return OutObjects.Num() > 0;
}