// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MusicData.h"
#include "MusicAnalysisSubsystem.generated.h"

// --- Forward Declarations ---
class UAnalyzedSongData;
class UDataTable;

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
struct FGameplayRhythmSection
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly, Category = "Rhythm Section") int32 StartTimeMS = 0;
    UPROPERTY(BlueprintReadOnly, Category = "Rhythm Section") float BPM = 120.f;
    UPROPERTY(BlueprintReadOnly, Category = "Rhythm Section") float BeatLengthMS = 500.f;
    UPROPERTY(BlueprintReadOnly, Category = "Rhythm Section") int32 AnchorTimestampMS = 0;
};

USTRUCT(BlueprintType)
struct FSongAnalysisResult // Kept for now as it's used inside UAnalyzedSongData
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Song Analysis") TArray<FGameplayRhythmSection> RhythmSections;
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

// --- Delegates ---
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBeatTriggered, float, BeatTimestamp);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnNoteHit, int32, TimestampMS, int32, NoteType, int32, HitSound);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBPMChanged, float, NewBPM);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMeterChanged, int32, NewMeter);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBreakPeriod, int32, StartTimeMS, int32, EndTimeMS);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSongEnd, float, EndTimeSeconds);


UCLASS()
class PROJECT_CIRCLE_API UMusicAnalysisSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// --- Public API ---
	UFUNCTION(BlueprintCallable, Category = "Music Analysis")
	void StartSongPlayback(UDataTable* PrimaryDataTable, const TArray<UDataTable*>& AllSongDataTables, float DifficultyBias = 0.0f);
	
	UFUNCTION(BlueprintCallable, Category = "Music Analysis")
	void UpdateMusicTime(float CurrentTimeSeconds);
	
	UFUNCTION(BlueprintPure, Category = "Music Analysis") bool IsReadyForPlayback() const { return bIsReadyForPlayback; }
	UFUNCTION(BlueprintPure, Category = "Music Analysis") float GetCurrentBPM() const { return CurrentBPM; }
	UFUNCTION(BlueprintPure, Category = "Music Analysis") bool IsInBreakPeriod() const { return LastProcessedMusicProgressMs < CurrentBreakEndTimeMS; }

	// --- Delegates for gameplay systems to subscribe to ---
	UPROPERTY(BlueprintAssignable, Category = "Music Events") FOnBeatTriggered OnBeatTriggered;
	UPROPERTY(BlueprintAssignable, Category = "Music Events") FOnNoteHit OnNoteHit;
	UPROPERTY(BlueprintAssignable, Category = "Music Events") FOnBPMChanged OnBPMChanged;
	UPROPERTY(BlueprintAssignable, Category = "Music Events") FOnMeterChanged OnMeterChanged;
	UPROPERTY(BlueprintAssignable, Category = "Music Events") FOnBreakPeriod OnBreakStart;
	UPROPERTY(BlueprintAssignable, Category = "Music Events") FOnBreakPeriod OnBreakEnd;
	UPROPERTY(BlueprintAssignable, Category = "Music Events") FOnSongEnd OnSongEnd;

private:
	// --- Playback Processing Functions ---
	void ProcessMusicEvents();
	void UpdateRhythmSection(int32 InCurrentTimeMS);
	void ProcessBeatTicks(int32 InCurrentTimeMS);
	void GenerateSliderSubEvents(const FMusicData& SliderData);
	void ResetPlaybackState();
	
	// --- Core Data Reference ---
	UPROPERTY()
	TObjectPtr<UAnalyzedSongData> CurrentAnalyzedSong;
	
	// --- Playback State Tracking ---
	bool bIsReadyForPlayback = false;
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