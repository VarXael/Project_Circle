#pragma once

#include "CoreMinimal.h"
#include "MusicAnalysisTypes.generated.h"

class UDataTable;
class USongConfigurationData;

USTRUCT(BlueprintType)
struct FSongAnalysisParameters
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
	TObjectPtr<USongConfigurationData> SourceConfig = nullptr;
};


// --- Enums and Structs (These are unchanged but needed for compilation) ---
UENUM(BlueprintType)
enum class EGameplaySectionType : uint8
{
	Normal      UMETA(DisplayName = "Normal"),
	HighEnergy  UMETA(DisplayName = "High Energy"),
	Buildup     UMETA(DisplayName = "Buildup"),
	Cooldown    UMETA(DisplayName = "Cooldown"),
	Break       UMETA(DisplayName = "Break")
};

USTRUCT(BlueprintType)
struct FRhythmSectionProfile : public FTableRowBase
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
};

USTRUCT(BlueprintType)
struct FSongAnalysisResult // Kept for now as it's used inside UAnalyzedSongData
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Song Analysis") TArray<FRhythmSectionProfile> RhythmSections;
};

struct FQueuedNoteEvent { int32 TimestampMS; int32 NoteType; int32 OriginalHitSound; bool operator<(const FQueuedNoteEvent& Other) const { return TimestampMS < Other.TimestampMS; } };
namespace EQueuedNoteType { constexpr int32 SliderTick = 128; constexpr int32 SliderTail = 256; }

USTRUCT(BlueprintType)
struct FConfidentHitObject
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