#pragma once

#include "CoreMinimal.h"
#include "PcMusicAnalysisTypes.generated.h"

class UMusicGameplayEventDefinition;
class UMusicGameplayEventActionSet;
class UDataTable;
class UPcMusicConfigurationData;

USTRUCT(BlueprintType)
struct FPcSongAnalysisParameters
{
	GENERATED_BODY()

	/** The specific DataTable whose notes will be used for gameplay events. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Analysis Parameters")
	TObjectPtr<UDataTable> GameplayMap = nullptr;

	/** The DataTable used as the base for finding rhythm sections. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Analysis Parameters")
	TObjectPtr<UDataTable> StructuralBaseMap = nullptr;

	/** The pool of all maps used for calculating weighted difficulty scores. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Analysis Parameters")
	TArray<TObjectPtr<UDataTable>> WeightedAnalysisMaps;

	/** A bias from -1 (easier) to 1 (harder) to weigh the analysis maps. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Analysis Parameters")
	float DifficultyBias = 0.0f;

	/** The Song Configuration asset itself, to access overrides. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Analysis Parameters")
	TObjectPtr<UPcMusicConfigurationData> SourceConfig = nullptr;
};


// --- Enums and Structs (These are unchanged but needed for compilation) ---
UENUM(BlueprintType)
enum class EPcGameplaySectionType : uint8
{
	Normal UMETA(DisplayName = "Normal"),
	HighEnergy UMETA(DisplayName = "High Energy"),
	Buildup UMETA(DisplayName = "Buildup"),
	Cooldown UMETA(DisplayName = "Cooldown"),
	Break UMETA(DisplayName = "Break")
};

USTRUCT(BlueprintType)
struct FPcMusicGameplayEvents : public FTableRowBase
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
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music Gameplay Events")
	TObjectPtr<UMusicGameplayEventDefinition> MusicGameplayEventDefinition;
	
};

USTRUCT(BlueprintType)
struct FPcMusicGameplayNotes : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music Gameplay Notes")
	int32 StartTimeMS = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music Gameplay Notes")
	float ApproachRate = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music Gameplay Events")
	TObjectPtr<UMusicGameplayEventDefinition> MusicGameplayEventDefinition;
	
};

USTRUCT(BlueprintType)
struct FPcSongAnalysisResult
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Song Analysis")
	TArray<FPcMusicGameplayEvents> RhythmSections;
};

struct FPcQueuedNoteEvent
{
	int32 TimestampMS;
	int32 NoteType;
	int32 OriginalHitSound;

	bool operator<(const FPcQueuedNoteEvent& Other) const
	{
		return TimestampMS < Other.TimestampMS;
	}
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
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Analysis")
	int32 TimestampMS = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Analysis")
	float Confidence = 0.f;
	uint32 CombinedHitSound = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Analysis")
	int32 HitObjectType = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Analysis")
	int32 Repeats = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Analysis")
	int32 SliderEndTimeMS = 0;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Analysis")
	float SliderTickRate = 1.f;
};


UENUM(BlueprintType)
enum class EPcGameplayEntryType : uint8
{
	HitObject UMETA(DisplayName = "Hit Object"),
	TimingPoint UMETA(DisplayName = "Timing Point"),
	Break UMETA(DisplayName = "Break"),
	AudioBeat UMETA(DisplayName = "Audio Beat")
};


USTRUCT(BlueprintType)
struct FPcImportedMusicData : public FTableRowBase
{
	GENERATED_BODY()

public:
	// --- Common Data (All Types) ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music Data")
	int32 TimestampMS = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music Data")
	EPcGameplayEntryType EntryType = EPcGameplayEntryType::HitObject;

	// --- Global Map Difficulty (Applies to all entries) ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music Data|Difficulty")
	float HPDrainRate = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music Data|Difficulty")
	float CircleSize = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music Data|Difficulty")
	float OverallDifficulty = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music Data|Difficulty")
	float ApproachRate = 5.0f;

	// --- Hit Object Data ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music Data|Hit Object",
		meta=(EditCondition="EntryType == EGameplayEntryType::HitObject"))
	int32 HitObjectType = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music Data|Hit Object",
		meta=(EditCondition="EntryType == EGameplayEntryType::HitObject"))
	int32 HitSound = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music Data|Hit Object",
		meta=(EditCondition="EntryType == EGameplayEntryType::HitObject"))
	int32 SliderEndTimeMS = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music Data|Hit Object",
		meta=(EditCondition="EntryType == EGameplayEntryType::HitObject"))
	int32 Repeats = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music Data|Hit Object",
		meta=(EditCondition="EntryType == EGameplayEntryType::HitObject"))
	float SliderTickRate = 1.f;

	// --- Break Period Data ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music Data|Break",
		meta=(EditCondition="EntryType == EGameplayEntryType::Break"))
	int32 BreakEndTimeMS = 0;

	// --- Timing Point Data ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music Data|Timing Point",
		meta=(EditCondition="EntryType == EGameplayEntryType::TimingPoint"))
	int32 Uninherited = 0; // 1 for uninherited (red), 0 for inherited (green)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music Data|Timing Point",
		meta=(EditCondition="EntryType == EGameplayEntryType::TimingPoint"))
	float BeatLength = 500.f; // For uninherited, this is ms per beat. For inherited, it's a -100/velocity multiplier.

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music Data|Timing Point",
		meta=(EditCondition="EntryType == EGameplayEntryType::TimingPoint"))
	int32 Meter = 4; // e.g., 4 for 4/4 time signature

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music Data|Timing Point",
		meta=(EditCondition="EntryType == EGameplayEntryType::TimingPoint"))
	int32 Effects = 0; // Bitmask for kiai time, etc.

	// --- Audio Beat Data (if you do separate audio analysis) ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music Data|Audio",
		meta=(EditCondition="EntryType == EGameplayEntryType::AudioBeat"))
	float AudioBeatStrength = 0.f;
};
