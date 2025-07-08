#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "MusicAnalysisTypes.h"
#include "MusicData.h"
#include "MusicAnalysisSubsystem.generated.h"

// Forward Declarations
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
	/**
	 * Initializes the subsystem for playback using pre-processed, hand-tuned data from DataTables.
	 * @param RhythmProfileData The DataTable containing the FRhythmSectionProfile rows.
	 * @param NoteEventData The DataTable containing the FMusicData for the specific difficulty being played.
	 */
	UFUNCTION(BlueprintCallable, Category = "Music Analysis")
	void InitializePlayback(UDataTable* RhythmProfileData, UDataTable* NoteEventData);
	
	/** Updates the subsystem with the current music time, triggering events. */
	UFUNCTION(BlueprintCallable, Category = "Music Analysis")
	void UpdateMusicTime(float CurrentTimeSeconds);
	
	UFUNCTION(BlueprintPure, Category = "Music Analysis") bool IsReadyForPlayback() const { return bIsReadyForPlayback; }
	UFUNCTION(BlueprintPure, Category = "Music Analysis") float GetCurrentBPM() const { return CurrentBPM; }
	UFUNCTION(BlueprintPure, Category = "Music Analysis") bool IsInBreakPeriod() const { return LastProcessedMusicProgressMs < CurrentBreakEndTimeMS; }

	// --- Delegates ---
	UPROPERTY(BlueprintAssignable, Category = "Music Events") FOnBeatTriggered OnBeatTriggered;
	UPROPERTY(BlueprintAssignable, Category = "Music Events") FOnNoteHit OnNoteHit;
	UPROPERTY(BlueprintAssignable, Category = "Music Events") FOnBPMChanged OnBPMChanged;
	UPROPERTY(BlueprintAssignable, Category = "Music Events") FOnMeterChanged OnMeterChanged;
	UPROPERTY(BlueprintAssignable, Category = "Music Events") FOnBreakPeriod OnBreakStart;
	UPROPERTY(BlueprintAssignable, Category = "Music Events") FOnBreakPeriod OnBreakEnd;
	UPROPERTY(BlueprintAssignable, Category = "Music Events") FOnSongEnd OnSongEnd;

private:
	void ProcessMusicEvents();
	void UpdateRhythmSection(int32 InCurrentTimeMS);
	void ProcessBeatTicks(int32 InCurrentTimeMS);
	void GenerateSliderSubEvents(const FMusicData& SliderData);
	void ResetState();
	
	// --- Core Playback Data ---
	// In MusicAnalysisSubsystem.h
	TArray<FRhythmSectionProfile> RhythmProfileRows;
	TArray<FMusicData> RuntimeEventRows;
	TMap<int32, float> MasterBeatLengths; // For slider tick calculations
	
	// --- Playback State Tracking ---
	bool bIsReadyForPlayback = false;
	int32 NextEventIndex = 0;
	int32 LastProcessedMusicProgressMs = -1;
	int32 AbsoluteSongEndTimeMS = -1;
	int32 NextBeatTimestampMS = 0;
	int32 CurrentSectionIndex = 0;
	int32 CurrentBeatInSession = 0;
	float CurrentBPM = 0.f;
	int32 CurrentMeter = 4;
	int32 CurrentBreakEndTimeMS = -1;
	TArray<FQueuedNoteEvent> NoteEventQueue;
};