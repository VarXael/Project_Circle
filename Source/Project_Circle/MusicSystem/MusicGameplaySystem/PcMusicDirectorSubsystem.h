#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Project_Circle/MusicSystem/MusicImportSystem/PcMusicAnalysisTypes.h"
#include "PcMusicDirectorSubsystem.generated.h"

class UPcMusicConfigurationData;
class APcMusicGameplayManager;
class UMusicActionInstance;

// Delegate definitions for external systems to listen to.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBeatTriggered, float, BeatTimestamp);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMusicTick, float, CurrentTimestampMS);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBPMChanged, float, NewBPM);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSongEnd, float, EndTimeSeconds);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMeterChanged, int32, NewMeter);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBreakPeriod, int32, StartTimeMS, int32, EndTimeMS);

/**
 * The core 'Clock' and 'Factory' for the music system. This subsystem reads song data,
 * keeps track of the precise song time, and spawns UMusicActionInstance objects for
 * notes that are about to occur. It also ticks these active instances.
 */
UCLASS()
class PROJECT_CIRCLE_API UPcMusicDirectorSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// --- PUBLIC API ---

	/**
	 * Initializes the subsystem with song data and a reference to the manager actor.
	 * @param SongConfig The configuration data for the song to be played.
	 * @param InMusicManager The gameplay manager actor, required for world context.
	 */
	void InitializePlayback(UPcMusicConfigurationData* SongConfig, APcMusicGameplayManager* InMusicManager);

	/**
	 * The main update function, called continuously by the PcMusicGameplayManager to drive the system.
	 * @param CurrentTimeSeconds The current playback time of the song.
	 */
	void UpdateMusicTime(float CurrentTimeSeconds);
	
	// --- GETTERS & DELEGATES ---
	UFUNCTION(BlueprintPure, Category = "Music Director")
	bool IsReadyForPlayback() const { return bIsReadyForPlayback; }

	UFUNCTION(BlueprintPure, Category = "Music Director")
	float GetCurrentBPM() const { return CurrentBPM; }

	UFUNCTION(BlueprintPure, Category = "Music Director")
	bool IsInBreakPeriod() const { return CurrentBreakEndTimeMS != -1; }

	UPROPERTY(BlueprintAssignable, Category = "Music Events")
	FOnBeatTriggered OnBeatTriggered;
	
	UPROPERTY(BlueprintAssignable, Category = "Music Events")
	FOnMusicTick OnMusicTick;
	
	UPROPERTY(BlueprintAssignable, Category = "Music Events")
	FOnBPMChanged OnBPMChanged;
	
	UPROPERTY(BlueprintAssignable, Category = "Music Events")
	FOnSongEnd OnSongEnd;
	
	UPROPERTY(BlueprintAssignable, Category = "Music Events")
	FOnMeterChanged OnMeterChanged;
	
	UPROPERTY(BlueprintAssignable, Category = "Music Events")
	FOnBreakPeriod OnBreakStart;
	
	UPROPERTY(BlueprintAssignable, Category = "Music Events")
	FOnBreakPeriod OnBreakEnd;

private:
	// --- PRIVATE LOGIC FUNCTIONS ---

	/** Creates UMusicActionInstance objects for notes that enter the lookahead window. */
	void ProcessNoteSpawning(int32 InCurrentTimeMS);
	
	/** Updates the current rhythm section data (BPM, Meter, Breaks) based on the song time. */
	void UpdateRhythmSection(int32 InCurrentTimeMS);

	/** Calculates and broadcasts beat events based on the current rhythm section. */
	void ProcessBeatTicks(int32 InCurrentTimeMS);

	/** Resets all state variables to prepare for a new song. */
	void ResetState();
	
	// --- MEMBER VARIABLES ---

	/** A direct pointer to the manager actor, which provides world context and is a stable owner. */
	UPROPERTY()
	TObjectPtr<APcMusicGameplayManager> MusicManager;

	/** The list of all currently active note instances that need to be ticked each frame. */
	UPROPERTY()
	TArray<TObjectPtr<UMusicActionInstance>> ActiveNoteInstances;

	/** The lookahead time in milliseconds. Notes within this window will have an instance created. */
	UPROPERTY(EditAnywhere, Category = "Music Director")
	int32 LookaheadTimeMS = 4000;

	// --- DATA STORAGE & STATE ---
	TArray<FPcMusicGameplayEvents> RhythmProfileRows;
	TArray<FPcMusicGameplayNotes> NoteEventRows;
	bool bIsReadyForPlayback = false;
	int32 NextNoteToSpawnIndex = 0;
	int32 LastProcessedMusicProgressMs = -1;
	int32 AbsoluteSongEndTimeMS = -1;
	int32 CurrentSectionIndex = 0;
	int32 NextBeatTimestampMS = 0;
	int32 CurrentBeatInSession = 0;
	float CurrentBPM = 0.f;
	int32 CurrentMeter = 4;
	int32 CurrentBreakEndTimeMS = -1;
};