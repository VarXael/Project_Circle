#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "MusicData.generated.h"

// Enum to identify the type of entry in the CSV, now including AudioBeat.
UENUM(BlueprintType)
enum class EGameplayEntryType : uint8
{
    TimingPoint UMETA(DisplayName = "TimingPoint"),
    HitObject   UMETA(DisplayName = "HitObject"),
    Break       UMETA(DisplayName = "Break"),
    AudioBeat   UMETA(DisplayName = "AudioBeat") // <-- NEW ENTRY TYPE
};

/**
 * @brief Structure for comprehensive gameplay and audio data parsed from .osu files.
 * Intended for use with a UDataTable for CSV import.
 * This struct matches the output of the OsuSongConverter_With_Audio_Analysis.py script.
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
    int32 TimestampMS;

    // --- Timing Point Specific Fields ---
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameplayData|TimingPoint")
    float BeatLength;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameplayData|TimingPoint")
    int32 Meter;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameplayData|TimingPoint")
    int32 Uninherited;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameplayData|TimingPoint")
    int32 Effects;

    // --- Hit Object Specific Fields ---
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameplayData|HitObject")
    int32 HitObjectType;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameplayData|HitObject")
    int32 SliderEndTimeMS;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameplayData|HitObject")
    int32 HitSound;

    // --- Break Specific Fields ---
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameplayData|Break")
    int32 BreakEndTimeMS;

    // --- NEW: Audio Beat Specific Fields ---
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameplayData|AudioBeat", meta=(ToolTip="The 'impact' or 'strength' of a beat detected in the audio waveform by Librosa."))
    float AudioBeatStrength;

    // Constructor to set default values
    FMusicData()
        : EntryType(EGameplayEntryType::HitObject)
        , TimestampMS(0)
        , BeatLength(0.0f)
        , Meter(0)
        , Uninherited(0)
        , Effects(0)
        , HitObjectType(0)
        , SliderEndTimeMS(0)
        , HitSound(0)
        , BreakEndTimeMS(0)
        , AudioBeatStrength(0.0f) // Initialize new field
    {}
};
