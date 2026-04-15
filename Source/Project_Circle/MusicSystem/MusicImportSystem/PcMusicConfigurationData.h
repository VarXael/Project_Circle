#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PcMusicConfigurationData.generated.h"

class UDataTable;

UCLASS(BlueprintType)
class PROJECT_CIRCLE_API UPcMusicConfigurationData : public UDataAsset
{
	GENERATED_BODY()

public:
	// --- CORE GAMEPLAY DATA ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay")
	TObjectPtr<UDataTable> GameplayMap;

	// --- GAMEPLAY BPM ---
	/**
	 * The target gameplay BPM. 
	 * The system automatically calculates the correct subdivision based on this.
	 * (e.g. Song is 440 BPM, Target is 110 BPM -> System automatically jumps every 4 beats).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay|BPM", meta = (ClampMin = "1.0", ClampMax = "300.0"))
	float DefaultGameplayBPM = 110.f;

	// --- RHYTHM ANALYSIS DATA ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Analysis")
	TArray<TObjectPtr<UDataTable>> AdditionalAnalysisMaps;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Analysis", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	float DifficultyBias = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Analysis", meta=(DisplayName="Structural Base Map (Optional Override)"))
	TObjectPtr<UDataTable> StructuralAnalysisBaseMapOverride;

	// --- GENERATED GAMEPLAY ASSETS ---
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Generated Assets")
	TObjectPtr<UDataTable> GeneratedRhythmProfile;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Generated Assets")
	TObjectPtr<UDataTable> GeneratedNoteData;

#if WITH_EDITOR
	UFUNCTION(CallInEditor, Category = "Generated Assets")
	void GenerateRhythmAssets();
#endif
};