// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MusicAnalysisTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "MusicData.h"
#include "MusicAnalysisSubsystem.generated.h"

class USongConfigurationData;
class UAnalyzedSongData;
class UDataTable;

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
	void StartSongPlayback(USongConfigurationData* SongConfig);
	
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