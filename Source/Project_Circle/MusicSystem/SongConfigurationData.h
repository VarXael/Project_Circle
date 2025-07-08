// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "MusicData.h"
#include "SongConfigurationData.generated.h"

class UDataTable;

USTRUCT(BlueprintType)
struct FSectionOverride
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Override")
	float OverriddenBeatLengthMS = 500.f;
};

UCLASS(BlueprintType)
class PROJECT_CIRCLE_API USongConfigurationData : public UDataAsset
{
	GENERATED_BODY()

public:
	// --- CORE GAMEPLAY DATA ---
	/** The specific DataTable whose notes will be used for gameplay events. This is the map the player "plays". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay")
	TObjectPtr<UDataTable> GameplayMap;
	
	/** A map of Section Start Times to their new, overridden properties. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay Overrides")
	TMap<int32, FSectionOverride> SectionOverrides;

	// --- RHYTHM ANALYSIS DATA ---
	/** All other DataTables to be used for rhythm analysis. The GameplayMap should NOT be included in this list. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Analysis")
	TArray<TObjectPtr<UDataTable>> AdditionalAnalysisMaps;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Analysis", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	float DifficultyBias = 0.0f;
	
	/**
	 * OPTIONAL: A specific map from the 'AdditionalAnalysisMaps' list to use as the base for structural analysis.
	 * If left empty, the system will automatically choose the one with the highest OverallDifficulty from the analysis list.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Analysis", meta=(DisplayName="Structural Base Map (Optional Override)"))
	TObjectPtr<UDataTable> StructuralAnalysisBaseMapOverride;

#if WITH_EDITOR
	/**
	 * Runs the full song analysis and generates the Rhythm Profile and Note Data DataTables.
	 * This will prompt to save two new assets.
	 */
	UFUNCTION(CallInEditor, Category = "Rhythm Generation")
	void GenerateRhythmAssets();
#endif
};
