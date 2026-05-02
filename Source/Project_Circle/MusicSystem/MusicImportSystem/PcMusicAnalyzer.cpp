#include "PcMusicAnalyzer.h"
#include "PcMusicConfigurationData.h"

namespace MusicAnalysisHelpers
{
	// Helpers can remain empty or stripped down as we no longer need the complex APS jitter math
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
			// CRITICAL FIX: Add the base Note Hit unconditionally! (This restores the missing circles)
			FPcRuntimeEvent Hit;
			Hit.TimestampMS = Row->TimestampMS;
			Hit.EventType = EPcRuntimeEventType::NoteHit;
			Hit.Value1 = Row->HitObjectType;
			Hit.Value2 = Row->HitSound;
			FlattenedRuntimeEvents.Add(Hit);

			// Unroll slider ticks and tails
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

	// Sort mathematically ascending to ensure chronological order
	FlattenedRuntimeEvents.Sort([](const FPcRuntimeEvent& A, const FPcRuntimeEvent& B) { return A.TimestampMS < B.TimestampMS; });
}

void UPcMusicAnalyzer::AnalyzeRhythmSections(const FPcSongAnalysisParameters& Parameters)
{
	TArray<FPcConfidentHitObject> ConfidentHitObjects;
	if (!GatherCouncilData(Parameters.WeightedAnalysisMaps, Parameters.DifficultyBias, ConfidentHitObjects)) return;

	TArray<const FPcImportedMusicData*> StructuralHitObjects;
	TArray<FPcImportedMusicData> StructuralUninheritedTPs;
	TArray<const FPcImportedMusicData*> StructuralAllEvents;

	Parameters.StructuralBaseMap->ForeachRow<FPcImportedMusicData>("Populating", [&](const FName&, const FPcImportedMusicData& Value) {
		StructuralAllEvents.Add(&Value);
		if (Value.EntryType == EPcGameplayEntryType::HitObject) StructuralHitObjects.Add(&Value);
		else if (Value.EntryType == EPcGameplayEntryType::TimingPoint && Value.Uninherited == 1) StructuralUninheritedTPs.Add(Value);
	});

	if (StructuralUninheritedTPs.Num() == 0 || StructuralHitObjects.Num() == 0) return;

	TSet<int32> BoundarySet;
	BoundarySet.Add(0);
	for (const auto& TP : StructuralUninheritedTPs) BoundarySet.Add(TP.TimestampMS);

	bool bKiai = false;
	TArray<TPair<int32, bool>> KiaiChanges; 
	
	for (const auto* Data : StructuralAllEvents) {
		if (Data->EntryType == EPcGameplayEntryType::TimingPoint) {
			if (((Data->Effects & 1) != 0) != bKiai) {
				bKiai = !bKiai;
				BoundarySet.Add(Data->TimestampMS);
				KiaiChanges.Add({Data->TimestampMS, bKiai});
			}
		} else if (Data->EntryType == EPcGameplayEntryType::Break) {
			BoundarySet.Add(Data->TimestampMS);
			BoundarySet.Add(Data->BreakEndTimeMS);
		}
	}

	BoundarySet.Add(ConfidentHitObjects.Num() > 0 ? ConfidentHitObjects.Last().TimestampMS + 5000 : 300000);

	TArray<int32> FinalBoundaries = BoundarySet.Array();
	FinalBoundaries.Sort();

	// Smooth Sections: Only divide based on strict musical timing points & drops (Kiai). No more APS chopping!
	for (int32 i = 0; i < FinalBoundaries.Num() - 1; ++i) {
		int32 StartTime = FinalBoundaries[i];
		int32 EndTime = FinalBoundaries[i + 1];
		if ((EndTime - StartTime) < 250) continue;

		float BaseBeatLength = 500.f;
		for (const auto& TP : StructuralUninheritedTPs) {
			if (TP.TimestampMS <= StartTime) BaseBeatLength = TP.BeatLength;
		}

		bool bSectionIsKiai = false;
		for (const auto& Change : KiaiChanges) {
			if (Change.Key <= StartTime) bSectionIsKiai = Change.Value;
			else break;
		}

		FPcRhythmSectionProfile Sec;
		Sec.StartTimeMS = StartTime;
		Sec.BeatLengthMS = BaseBeatLength; 
		Sec.BPM = BaseBeatLength > 0 ? 60000.f / BaseBeatLength : 0.f;
		Sec.AnchorTimestampMS = StartTime; 
		Sec.MovementPreset = bSectionIsKiai ? EPcMovementPresetOverride::Enhanced : EPcMovementPresetOverride::Normal;

		RhythmSections.Add(Sec);
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
	OutObjects.Sort([](const FPcConfidentHitObject& A, const FPcConfidentHitObject& B) { return A.TimestampMS < B.TimestampMS; });
	return OutObjects.Num() > 0;
}