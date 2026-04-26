#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "PcMusicAnalysisTypes.generated.h"

class UDataTable;
class UPcMusicConfigurationData;

// -----------------------------------------------------------------------------
// PURE RUNTIME DATA (Agnostic - Game only cares about this)
// -----------------------------------------------------------------------------

UENUM(BlueprintType)
enum class EPcMovementPresetOverride : uint8
{
	Auto        UMETA(DisplayName = "Auto (Use Section Tag)"),
	Normal      UMETA(DisplayName = "Normal"),
	Enhanced    UMETA(DisplayName = "Enhanced (Drop)")
};

// Movement feel parameters for a gameplay tier.
// BeatsPerJump is gone — jump interval is derived from BPM auto-subdivision now.
USTRUCT(BlueprintType)
struct FPcMovementPreset
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Jump")  float PeakHeightCM   = 200.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Speed") float MaxGroundSpeed = 900.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Speed") float MaxAirSpeed    = 900.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bhop")  float ChargeTime     = 0.6f;
};

USTRUCT(BlueprintType)
struct FPcRhythmSectionProfile : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly,  Category = "Rhythm Section")            int32 StartTimeMS     = 0;
	UPROPERTY(EditAnywhere,   BlueprintReadWrite,  Category = "Rhythm Section")            float BPM             = 120.f;
	UPROPERTY(EditAnywhere,   BlueprintReadWrite,  Category = "Rhythm Section")            float BeatLengthMS    = 500.f;
	UPROPERTY(EditAnywhere,   BlueprintReadWrite,  Category = "Rhythm Section")            int32 AnchorTimestampMS = 0;

	// Optional: authored gameplay BPM override (skips auto-subdivision when > 0)
	UPROPERTY(EditAnywhere,   BlueprintReadWrite,  Category = "Rhythm Section|Gameplay")   float GameplayBPM     = 0.f;

	// Normal = regular gameplay feel.  Enhanced = the drop.
	UPROPERTY(EditAnywhere,   BlueprintReadWrite,  Category = "Rhythm Section|Gameplay")
	EPcMovementPresetOverride MovementPreset = EPcMovementPresetOverride::Auto;
};

UENUM(BlueprintType)
enum class EPcRuntimeEventType : uint8
{
	NoteHit     UMETA(DisplayName = "Note Hit"),
	MeterChange UMETA(DisplayName = "Meter Change"),
	BreakStart  UMETA(DisplayName = "Break Start"),
	BreakEnd    UMETA(DisplayName = "Break End"),
	SongEnd     UMETA(DisplayName = "Song End")
};

USTRUCT(BlueprintType)
struct FPcRuntimeEvent : public FTableRowBase
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Event") int32                TimestampMS = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Event") EPcRuntimeEventType EventType   = EPcRuntimeEventType::NoteHit;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Event") int32                Value1      = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Event") int32                Value2      = 0;
};

// -----------------------------------------------------------------------------
// IMPORT & EDITOR DATA (Osu! Specific - Stripped out during analysis)
// -----------------------------------------------------------------------------

UENUM(BlueprintType)
enum class EPcGameplayEntryType : uint8
{
	HitObject   UMETA(DisplayName = "Hit Object"),
	TimingPoint UMETA(DisplayName = "Timing Point"),
	Break       UMETA(DisplayName = "Break"),
	AudioBeat   UMETA(DisplayName = "Audio Beat")
};

USTRUCT(BlueprintType)
struct FPcImportedMusicData : public FTableRowBase
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32                  TimestampMS       = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) EPcGameplayEntryType   EntryType         = EPcGameplayEntryType::HitObject;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float                  HPDrainRate       = 5.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float                  CircleSize        = 5.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float                  OverallDifficulty = 5.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float                  ApproachRate      = 5.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32                  HitObjectType     = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32                  HitSound          = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32                  SliderEndTimeMS   = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32                  Repeats           = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float                  SliderTickRate    = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32                  BreakEndTimeMS    = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32                  Uninherited       = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float                  BeatLength        = 500.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32                  Meter             = 4;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32                  Effects           = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float                  AudioBeatStrength = 0.f;
};

USTRUCT(BlueprintType)
struct FPcSongAnalysisParameters
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<UDataTable>              GameplayMap           = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<UDataTable>              StructuralBaseMap     = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<TObjectPtr<UDataTable>>      WeightedAnalysisMaps;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float                               DifficultyBias        = 0.0f;
};

USTRUCT()
struct FPcConfidentHitObject
{
	GENERATED_BODY()
	int32    TimestampMS     = 0;
	float    Confidence      = 0.f;
	uint32   CombinedHitSound = 0;
	int32    HitObjectType   = 0;
	int32    Repeats         = 0;
	int32    SliderEndTimeMS = 0;
	float    SliderTickRate  = 1.f;
};