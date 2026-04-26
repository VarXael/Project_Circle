#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Sound/SoundWave.h"
#include "PcMusicAnalysisTypes.h"
#include "PcMusicConfigurationData.generated.h"

class UDataTable;

UCLASS(BlueprintType)
class PROJECT_CIRCLE_API UPcMusicConfigurationData : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	TObjectPtr<USoundWave> SongWaveAsset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay")
	TObjectPtr<UDataTable> GameplayMap;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay|BPM", meta = (ClampMin = "1.0", ClampMax = "300.0"))
	float DefaultGameplayBPM = 110.f;

	// --- NEW: Pulse Physics Presets (The song dictates the bounce!) ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay|Pulse Settings")
	FPcMovementPreset PulseSlow = { 0.5f, 200.f, 800.f, 800.f, 0.7f };
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay|Pulse Settings")
	FPcMovementPreset PulseNormal = { 1.0f, 200.f, 900.f, 900.f, 0.6f };
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay|Pulse Settings")
	FPcMovementPreset PulseFast = { 2.0f, 175.f, 1100.f, 1100.f, 0.5f };
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay|Pulse Settings")
	FPcMovementPreset PulseVeryFast = { 4.0f, 150.f, 1400.f, 1400.f, 0.4f };

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