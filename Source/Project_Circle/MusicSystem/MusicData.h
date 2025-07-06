#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "MusicData.generated.h"

// Enum to identify the type of entry in the CSV
UENUM(BlueprintType)
enum class EGameplayEntryType : uint8
{
    TimingPoint UMETA(DisplayName = "TimingPoint"),
    HitObject UMETA(DisplayName = "HitObject"),
    Break UMETA(DisplayName = "Break")
};

/**
 * @brief Structure for minimal gameplay data parsed from .osu files.
 * Intended for use with a UDataTable for CSV import.
 */
USTRUCT(BlueprintType,Blueprintable)
struct PROJECT_CIRCLE_API FMusicData : public FTableRowBase
{
    GENERATED_BODY()

public:
    // --- Common Fields for all Entry Types ---
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameplayData")
    EGameplayEntryType EntryType;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameplayData")
    int32 TimestampMS; // Start time for TimingPoint and Break, Hit time for HitObject

    // --- Timing Point Specific Fields (only populated for EntryType::TimingPoint) ---
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameplayData|TimingPoint")
    float BeatLength; // Milliseconds per beat or speed multiplier

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameplayData|TimingPoint")
    int32 Meter; // Time signature numerator (e.g., 4 for 4/4)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameplayData|TimingPoint")
    int32 Uninherited; // 1 for uninherited (new BPM), 0 for inherited (multiplier)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameplayData|TimingPoint")
    int32 Effects; // Bit flags, where bit 1 (value 1) indicates Kiai Time (beat drop)

    // --- Hit Object Specific Fields (only populated for EntryType::HitObject) ---
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameplayData|HitObject")
    int32 HitObjectType; // Raw type flags (1=Circle, 2=Slider, 8=Spinner)

    // --- Break Specific Fields (only populated for EntryType::Break) ---
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameplayData|Break")
    int32 BreakEndTimeMS; // End time of the break period

    // Constructor to set default values
    FMusicData()
        : EntryType(EGameplayEntryType::HitObject) // Default to HitObject type
        , TimestampMS(0)
        , BeatLength(0.0f)
        , Meter(0)
        , Uninherited(0)
        , Effects(0)
        , HitObjectType(0)
        , BreakEndTimeMS(0)
    {}
};
