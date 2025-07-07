// MusicData.h

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "MusicData.generated.h"

UENUM(BlueprintType)
enum class EGameplayEntryType : uint8
{
	TimingPoint UMETA(DisplayName = "TimingPoint"),
	HitObject   UMETA(DisplayName = "HitObject"),
	Break       UMETA(DisplayName = "Break"),
	AudioBeat   UMETA(DisplayName = "AudioBeat")
};

USTRUCT(BlueprintType, Blueprintable)
struct PROJECT_CIRCLE_API FMusicData : public FTableRowBase
{
	GENERATED_BODY()

public:
	// --- Common Fields ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameplayData")
	EGameplayEntryType EntryType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameplayData")
	int32 TimestampMS;

	// --- Timing Point ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameplayData|TimingPoint")
	float BeatLength;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameplayData|TimingPoint")
	int32 Meter;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameplayData|TimingPoint")
	int32 Uninherited;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameplayData|TimingPoint")
	int32 Effects;

	// --- Hit Object ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameplayData|HitObject")
	int32 HitObjectType;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameplayData|HitObject")
	int32 HitSound;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameplayData|HitObject")
	int32 SliderEndTimeMS;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameplayData|HitObject")
	float PixelLength;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameplayData|HitObject")
	int32 Repeats;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameplayData|HitObject")
	float SliderVelocity;

	// --- Break ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameplayData|Break")
	int32 BreakEndTimeMS;

	// --- Audio Beat ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameplayData|AudioBeat")
	float AudioBeatStrength;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameplayData|AudioBeat")
	float AudioBeatCentroid;
		
	// --- NEW: Global Difficulty Setting ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameplayData|Difficulty")
	float SliderTickRate;

	// Constructor
	FMusicData()
		: EntryType(EGameplayEntryType::HitObject)
		, TimestampMS(0)
		, BeatLength(0.0f)
		, Meter(4)
		, Uninherited(0)
		, Effects(0)
		, HitObjectType(0)
		, HitSound(0)
		, SliderEndTimeMS(0)
		, PixelLength(0.0f)
		, Repeats(0)
		, SliderVelocity(0.0f)
		, BreakEndTimeMS(0)
		, AudioBeatStrength(0.0f)
		, AudioBeatCentroid(0.0f)
		, SliderTickRate(1.0f) // Default value
	{}
};