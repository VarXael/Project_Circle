#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Sound/SoundWave.h"
#include "Project_Circle/MusicSystem/MusicImportSystem/PcMusicAnalysisTypes.h"
// THIS MUST ALWAYS BE THE LAST INCLUDE
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

	// ── BPM Anchoring ─────────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay|BPM",
	          meta = (ClampMin = "20.0", ClampMax = "200.0",
	                  ToolTip = "Target gameplay BPM. Auto-subdivision snaps each section as close to this as possible."))
	float TargetGameplayBPM = 55.f;

	// ── Movement Presets ──────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay|Presets")
	FPcMovementPreset PulseNormal = { 200.f, 900.f, 900.f, 0.6f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gameplay|Presets")
	FPcMovementPreset PulseEnhanced = { 280.f, 1100.f, 1100.f, 0.5f };

	// ── Analysis ──────────────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Analysis")
	TArray<TObjectPtr<UDataTable>> AdditionalAnalysisMaps;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Analysis",
	          meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	float DifficultyBias = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Analysis",
	          meta = (DisplayName = "Structural Base Map (Optional Override)"))
	TObjectPtr<UDataTable> StructuralAnalysisBaseMapOverride;

	// ── Generated Assets ──────────────────────────────────────────────────────
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Generated Assets")
	TObjectPtr<UDataTable> GeneratedRhythmProfile;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Generated Assets")
	TObjectPtr<UDataTable> GeneratedNoteData;

#if WITH_EDITOR
	UFUNCTION(CallInEditor, Category = "Generated Assets")
	void GenerateRhythmAssets();
#endif
};