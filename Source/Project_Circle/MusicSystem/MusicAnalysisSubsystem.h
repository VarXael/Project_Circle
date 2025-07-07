// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MusicData.h"
#include "MusicAnalysisSubsystem.generated.h"

// The enum to classify the musical context of a section.
UENUM(BlueprintType)
enum class EGameplaySectionType : uint8
{
    Normal      UMETA(DisplayName = "Normal"),
    HighEnergy  UMETA(DisplayName = "High Energy"),
    Buildup     UMETA(DisplayName = "Buildup"),
    Cooldown    UMETA(DisplayName = "Cooldown"),
    Break       UMETA(DisplayName = "Break")
};

// This struct holds the final, pre-calculated gameplay data for a single, stable rhythmic section.
USTRUCT(BlueprintType)
struct FGameplayRhythmSection
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Rhythm Section")
    int32 StartTimeMS = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Rhythm Section")
    float BPM = 120.f;

    UPROPERTY(BlueprintReadOnly, Category = "Rhythm Section")
    float BeatLengthMS = 500.f;

    UPROPERTY(BlueprintReadOnly, Category = "Rhythm Section")
    int32 AnchorTimestampMS = 0;
};

// The primary output of our analysis. It contains everything a consumer needs to know.
USTRUCT(BlueprintType)
struct FSongAnalysisResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Song Analysis")
	TArray<FGameplayRhythmSection> RhythmSections;
};

// A temporary struct to hold the full profile of a section for analysis.
struct FSectionProfileData
{
	int32 StartTime;
	int32 EndTime;
	float BaseBeatLength;
	int32 AnchorTimestamp;
	float HybridApsScore;
};

// Represents a dynamically generated note event like a slider tick or tail.
struct FQueuedNoteEvent
{
	int32 TimestampMS = 0;
	int32 NoteType = 0;
	int32 OriginalHitSound = 0;

	bool operator<(const FQueuedNoteEvent& Other) const
	{
		return TimestampMS < Other.TimestampMS;
	}
};

// We can use the HitObjectType flags from osu! and add our own custom ones.
namespace EQueuedNoteType
{
	constexpr int32 SliderTick = 128; // Custom flag not used by osu!
	constexpr int32 SliderTail = 256;  // Custom flag for repeats/tail
}

// Delegate for the rhythmic beat of the song
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBeatTriggered, float, BeatTimestamp);

// Delegate for discrete note events (circles, slider heads, ticks, tails)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnNoteHit, int32, TimestampMS, int32, NoteType, int32, HitSound);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBPMChanged, float, NewBPM);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMeterChanged, int32, NewMeter);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBreakPeriod, int32, StartTimeMS, int32, EndTimeMS);

// Forward declare internal struct
struct FQueuedNoteEvent;

UCLASS()
class PROJECT_CIRCLE_API UMusicAnalysisSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// --- Public API ---

	UFUNCTION(BlueprintCallable, Category = "Music Analysis")
	void StartSongAnalysis(UDataTable* MusicDataTable);
	
	UFUNCTION(BlueprintCallable, Category = "Music Analysis")
	void UpdateMusicTime(float CurrentTimeSeconds);
	
	UFUNCTION(BlueprintPure, Category = "Music Analysis")
	bool IsAnalysisComplete() const { return bAnalysisComplete; }

	UFUNCTION(BlueprintPure, Category = "Music Analysis")
	float GetCurrentBPM() const { return CurrentBPM; }
	
	UFUNCTION(BlueprintPure, Category = "Music Analysis")
	bool IsInBreakPeriod() const { return bInBreakPeriod; }

	// --- Delegates for gameplay systems to subscribe to ---
	UPROPERTY(BlueprintAssignable, Category = "Music Events")
	FOnBeatTriggered OnBeatTriggered;

	UPROPERTY(BlueprintAssignable, Category = "Music Events")
	FOnNoteHit OnNoteHit;

	UPROPERTY(BlueprintAssignable, Category = "Music Events")
	FOnBPMChanged OnBPMChanged;

	UPROPERTY(BlueprintAssignable, Category = "Music Events")
	FOnMeterChanged OnMeterChanged;
	
	UPROPERTY(BlueprintAssignable, Category = "Music Events")
	FOnBreakPeriod OnBreakStart;

	UPROPERTY(BlueprintAssignable, Category = "Music Events")
	FOnBreakPeriod OnBreakEnd;

private:
	// --- Internal State & Data ---
	bool bAnalysisComplete = false;
	TArray<FMusicData> LoadedMusicData;
	FSongAnalysisResult CurrentSongAnalysis;
	
	// --- Analysis Logic (moved from PcMusicManager) ---
	FSongAnalysisResult AnalyzeRhythmSections();
	bool LoadMusicDataFromTable(UDataTable* MusicDataTable);

	// --- Playback State Tracking ---
	int32 NextEventIndex = 0;
	int32 LastProcessedMusicProgressMs = -1;
	int32 NextBeatTimestampMS = 0;
	int32 CurrentRhythmSectionIndex = 0;
	
	float CurrentBPM = 0.f;
	int32 CurrentMeter = 4;
	bool bInBreakPeriod = false;
	
	TArray<FQueuedNoteEvent> NoteEventQueue;

	// --- Playback Processing Functions ---
	void ProcessMusicEvents(int32 InCurrentTimeMS);
	void UpdateRhythmSection(int32 InCurrentTimeMS);
	void ProcessBeatTicks(int32 InCurrentTimeMS);
	void GenerateSliderSubEvents(const FMusicData& SliderData);
};