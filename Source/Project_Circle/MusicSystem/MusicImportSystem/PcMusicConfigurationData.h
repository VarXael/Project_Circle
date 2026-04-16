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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay")
	TObjectPtr<UDataTable> GameplayMap;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay|BPM", meta = (ClampMin = "1.0", ClampMax = "300.0"))
	float DefaultGameplayBPM = 110.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Analysis")
	TArray<TObjectPtr<UDataTable>> AdditionalAnalysisMaps;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Analysis", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	float DifficultyBias = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Analysis", meta=(DisplayName="Structural Base Map (Optional Override)"))
	TObjectPtr<UDataTable> StructuralAnalysisBaseMapOverride;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Generated Assets")
	TObjectPtr<UDataTable> GeneratedRhythmProfile;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Generated Assets")
	TObjectPtr<UDataTable> GeneratedNoteData;

#if WITH_EDITOR
	UFUNCTION(CallInEditor, Category = "Generated Assets")
	void GenerateRhythmAssets();
#endif
};