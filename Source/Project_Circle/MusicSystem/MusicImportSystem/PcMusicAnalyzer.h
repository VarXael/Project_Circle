#pragma once

#include "CoreMinimal.h"
#include "PcMusicAnalysisTypes.h"
#include "PcMusicAnalyzer.generated.h"

UCLASS(BlueprintType)
class PROJECT_CIRCLE_API UPcMusicAnalyzer : public UObject
{
	GENERATED_BODY()

public:
	static UPcMusicAnalyzer* RunSongAnalysis(UObject* Outer, UPcMusicConfigurationData* SongConfig);

	const TArray<FPcRhythmSectionProfile>& GetRhythmSections() const { return RhythmSections; }
	const TArray<FPcRuntimeEvent>& GetRuntimeEvents() const { return FlattenedRuntimeEvents; }

private:
	void AnalyzeRhythmSections(const FPcSongAnalysisParameters& Parameters);
	void FlattenAndUnrollEvents(UDataTable* GameplayMap);
	bool GatherCouncilData(const TArray<UDataTable*>& WeightedMaps, float DifficultyBias, TArray<FPcConfidentHitObject>& OutObjects);

	TArray<FPcRhythmSectionProfile> RhythmSections;
	TArray<FPcRuntimeEvent> FlattenedRuntimeEvents;
	TMap<int32, FPcImportedMusicData> MasterUninheritedTimingPoints;
};