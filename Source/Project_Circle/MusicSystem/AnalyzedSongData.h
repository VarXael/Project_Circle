// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "MusicData.h" // For FMusicData
#include "MusicAnalysisSubsystem.h" // For FGameplayRhythmSection, FConfidentHitObject, etc.
#include "AnalyzedSongData.generated.h"

class UDataTable;

/**
 * A data-only UObject that holds the complete analysis result of a song.
 * This object is created by the MusicAnalysisSubsystem and then used by it for
 * real-time event playback. It encapsulates all heavy analysis logic.
 */
UCLASS(BlueprintType)
class PROJECT_CIRCLE_API UAnalyzedSongData : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * Factory method to create and populate an analysis object.
	 * This performs the entire analysis process.
	 * @param Outer The object to own this new UObject (typically the UMusicAnalysisSubsystem).
	 * @param PrimaryDataTable The main difficulty used for structural analysis.
	 * @param AllSongDataTables All difficulties used for council voting.
	 * @param DifficultyBias A -1 to 1 value to weigh council votes towards easier or harder difficulties.
	 * @return A fully analyzed song data object, or nullptr if analysis fails.
	 */
	static UAnalyzedSongData* CreateAnalyzedSongData(UObject* Outer, UDataTable* PrimaryDataTable, const TArray<UDataTable*>& AllSongDataTables, float DifficultyBias);

	// --- Public Data Accessors ---

	UFUNCTION(BlueprintPure, Category = "Analyzed Song Data")
	const TArray<FGameplayRhythmSection>& GetRhythmSections() const { return Result.RhythmSections; }
	
	UFUNCTION(BlueprintPure, Category = "Analyzed Song Data")
	const TArray<FMusicData>& GetRuntimeEventTimeline() const { return RuntimeEventTimeline; }

	UFUNCTION(BlueprintPure, Category = "Analyzed Song Data")
	const TMap<int32, FMusicData>& GetMasterUninheritedTimingPoints() const { return MasterUninheritedTimingPoints; }
	
	UFUNCTION(BlueprintPure, Category = "Analyzed Song Data")
	int32 GetAbsoluteSongEndTimeMS() const { return AbsoluteSongEndTimeMS; }

private:
	// --- Analysis Logic (moved from Subsystem) ---
	void AnalyzeRhythmSections(UDataTable* PrimaryDataTable, const TArray<UDataTable*>& AllSongDataTables, float DifficultyBias);
	
	// UPDATED SIGNATURE
	bool GatherCouncilData(const TArray<UDataTable*>& AllSongDataTables, float DifficultyBias, TArray<FConfidentHitObject>& OutConfidentHitObjects);

	// --- Stored Analysis Results ---

	UPROPERTY()
	FSongAnalysisResult Result;

	UPROPERTY()
	TArray<FMusicData> RuntimeEventTimeline;

	UPROPERTY()
	TMap<int32, FMusicData> MasterUninheritedTimingPoints;

	UPROPERTY()
	int32 AbsoluteSongEndTimeMS = -1;
};