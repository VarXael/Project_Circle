#pragma once

#include "CoreMinimal.h"
#include "PcMusicAnalysisTypes.generated.h"

class UDataTable;
class UPcMusicConfigurationData;

// -----------------------------------------------------------------------------
// ANALYSIS PARAMETERS
// -----------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct FPcSongAnalysisParameters
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Analysis Parameters")
	TObjectPtr<UDataTable> GameplayMap = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Analysis Parameters")
	TObjectPtr<UDataTable> StructuralBaseMap = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Analysis Parameters")
	TArray<TObjectPtr<UDataTable>> WeightedAnalysisMaps;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Analysis Parameters")
	float DifficultyBias = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Analysis Parameters")
	TObjectPtr<UPcMusicConfigurationData> SourceConfig = nullptr;
};

// -----------------------------------------------------------------------------
// RHYTHM SECTION
// -----------------------------------------------------------------------------

UENUM(BlueprintType)
enum class EPcGameplaySectionType : uint8
{
	Normal      UMETA(DisplayName = "Normal"),
	HighEnergy  UMETA(DisplayName = "High Energy"),
	Buildup     UMETA(DisplayName = "Buildup"),
	Cooldown    UMETA(DisplayName = "Cooldown"),
	Break       UMETA(DisplayName = "Break")
};

USTRUCT(BlueprintType)
struct FPcRhythmSectionProfile : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rhythm Section")
	int32 StartTimeMS = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rhythm Section")
	float BPM = 120.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rhythm Section")
	float BeatLengthMS = 500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rhythm Section")
	int32 AnchorTimestampMS = 0;

	/**
	 * The gameplay BPM for this section — what tempo the player's jump should feel like.
	 * The subsystem uses this to calculate subdivision (how many raw beats between jumps)
	 * and jump height (airtime = one gameplay beat interval).
	 *
	 * Set this directly in the generated DataTable after generating rhythm assets.
	 *
	 * Rules of thumb:
	 *   - Keep it between ~80 and ~180 for comfortable jump feel.
	 *   - Subdivision = round(BPM / GameplayBPM). e.g. BPM=400, GameplayBPM=100 → jumps every 4 beats.
	 *   - If left at 0, the subsystem falls back to using raw BPM (no subdivision).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rhythm Section|Gameplay")
	float GameplayBPM = 0.f;
};

// -----------------------------------------------------------------------------
// ANALYSIS RESULT
// -----------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct FPcSongAnalysisResult
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Song Analysis") TArray<FPcRhythmSectionProfile> RhythmSections;
};

// -----------------------------------------------------------------------------
// NOTE / QUEUE TYPES (unchanged)
// -----------------------------------------------------------------------------

struct FPcQueuedNoteEvent
{
	int32 TimestampMS;
	int32 NoteType;
	int32 OriginalHitSound;
	bool operator<(const FPcQueuedNoteEvent& Other) const { return TimestampMS < Other.TimestampMS; }
};

namespace EQueuedNoteType
{
	constexpr int32 SliderTick = 128;
	constexpr int32 SliderTail = 256;
}

USTRUCT(BlueprintType)
struct FPcConfidentHitObject
{
	GENERATED_BODY()
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Analysis") int32 TimestampMS = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Analysis") float Confidence = 0.f;
	uint32 CombinedHitSound = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Analysis") int32 HitObjectType = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Analysis") int32 Repeats = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Analysis") int32 SliderEndTimeMS = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Analysis") float SliderTickRate = 1.f;
};

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

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music Data") int32 TimestampMS = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music Data") EPcGameplayEntryType EntryType = EPcGameplayEntryType::HitObject;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music Data|Difficulty") float HPDrainRate = 5.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music Data|Difficulty") float CircleSize = 5.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music Data|Difficulty") float OverallDifficulty = 5.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music Data|Difficulty") float ApproachRate = 5.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music Data|Hit Object") int32 HitObjectType = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music Data|Hit Object") int32 HitSound = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music Data|Hit Object") int32 SliderEndTimeMS = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music Data|Hit Object") int32 Repeats = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music Data|Hit Object") float SliderTickRate = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music Data|Break") int32 BreakEndTimeMS = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music Data|Timing Point") int32 Uninherited = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music Data|Timing Point") float BeatLength = 500.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music Data|Timing Point") int32 Meter = 4;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music Data|Timing Point") int32 Effects = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music Data|Audio") float AudioBeatStrength = 0.f;
};