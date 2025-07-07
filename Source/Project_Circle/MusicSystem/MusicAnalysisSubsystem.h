// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MusicData.h"
#include "MusicAnalysisSubsystem.generated.h"

// ... (enums and structs are unchanged) ...
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

USTRUCT(BlueprintType)
struct FSongAnalysisResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Song Analysis")
	TArray<FGameplayRhythmSection> RhythmSections;
};

struct FSectionProfileData
{
	int32 StartTime;
	int32 EndTime;
	float BaseBeatLength;
	int32 AnchorTimestamp;
	float HybridApsScore;
};

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

namespace EQueuedNoteType
{
	constexpr int32 SliderTick = 128;
	constexpr int32 SliderTail = 256;
}

USTRUCT(BlueprintType)
struct FConfidentHitObject
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

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBeatTriggered, float, BeatTimestamp);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnNoteHit, int32, TimestampMS, int32, NoteType, int32, HitSound);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBPMChanged, float, NewBPM);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMeterChanged, int32, NewMeter);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBreakPeriod, int32, StartTimeMS, int32, EndTimeMS);


UCLASS()
class PROJECT_CIRCLE_API UMusicAnalysisSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// --- Public API ---

	/**
	 * Starts a comprehensive analysis using the "King and Council" model.
	 * @param PrimaryDataTable The "King" - the difficulty that defines the song's structure and timing.
	 * @param AllSongDataTables The "Council" - ALL difficulties, including the primary one, used for intensity consensus.
	 * @param DifficultyBias A -1.0 to 1.0 value to give more weight to easier or harder difficulties in the council.
	 */
	UFUNCTION(BlueprintCallable, Category = "Music Analysis")
	void StartSongAnalysis(UDataTable* PrimaryDataTable, const TArray<UDataTable*>& AllSongDataTables, float DifficultyBias = 0.0f);
	
	/** Updates the subsystem with the current music playback time to process events. */
	UFUNCTION(BlueprintCallable, Category = "Music Analysis")
	void UpdateMusicTime(float CurrentTimeSeconds);
	
	UFUNCTION(BlueprintPure, Category = "Music Analysis")
	bool IsAnalysisComplete() const { return bAnalysisComplete; }

	UFUNCTION(BlueprintPure, Category = "Music Analysis")
	float GetCurrentBPM() const { return CurrentBPM; }
	
	UFUNCTION(BlueprintPure, Category = "Music Analysis")
	bool IsInBreakPeriod() const { return LastProcessedMusicProgressMs < CurrentBreakEndTimeMS; }

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
	// --- Analysis Logic ---
	FSongAnalysisResult AnalyzeRhythmSections(UDataTable* PrimaryDataTable, const TArray<UDataTable*>& AllSongDataTables, float DifficultyBias);
	bool LoadCouncilData(const TArray<UDataTable*>& AllSongDataTables, float DifficultyBias);

	// --- Playback Processing Functions ---
	void ProcessMusicEvents();
	void UpdateRhythmSection(int32 InCurrentTimeMS);
	void ProcessBeatTicks(int32 InCurrentTimeMS);
	void GenerateSliderSubEvents(const FMusicData& SliderData);
	
	// --- Core Analysis Data ---
	FSongAnalysisResult CurrentSongAnalysis;
	TArray<FConfidentHitObject> ConfidentHitObjects;
	TArray<FMusicData> MasterAudioBeats;
	TMap<int32, FMusicData> MasterUninheritedTimingPoints;

	UPROPERTY()
	TArray<FMusicData> RuntimeEventTimeline;
	
	// --- Playback State Tracking ---
	bool bAnalysisComplete = false;
	int32 NextEventIndex = 0;
	int32 LastProcessedMusicProgressMs = -1;
	int32 NextBeatTimestampMS = 0;
	int32 CurrentRhythmSectionIndex = 0;
	int32 CurrentBeatInSession = 0;
	
	float CurrentBPM = 0.f;
	int32 CurrentMeter = 4;
	int32 CurrentBreakEndTimeMS = -1;
	
	TArray<FQueuedNoteEvent> NoteEventQueue;
};