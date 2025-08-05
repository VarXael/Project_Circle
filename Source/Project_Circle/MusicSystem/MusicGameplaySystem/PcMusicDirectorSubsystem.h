#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Project_Circle/MusicSystem/MusicImportSystem/PcMusicAnalysisTypes.h"
#include "PcMusicDirectorSubsystem.generated.h"

class UPcMusicConfigurationData;
class APcMusicGameplayManager;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnMusicTick, float /** CurrentTimeMs */);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBeatTriggered, float, BeatTimestamp);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSongProgress, float, CurrentSongProgress);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBPMChanged, float, NewBPM);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSongEnd, float, EndTimeSeconds);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMeterChanged, int32, NewMeter);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBreakPeriod, int32, StartTimeMS, int32, EndTimeMS);


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
	UFUNCTION(BlueprintCallable, Category = "Music Director")
	void InitializePlayback(UPcMusicConfigurationData* SongConfig, APcMusicGameplayManager* InMusicManager);

	/** The main update function, driven by the PcMusicGameplayManager. */
	UFUNCTION(BlueprintCallable, Category = "Music Director")
	void UpdateMusicTime(float CurrentTimeSeconds);
	
	/** The "heartbeat" delegate that autonomous objects can subscribe to for per-frame updates. */
	FOnMusicTick OnMusicTick;

	// --- GETTERS AND DELEGATES ---
	UFUNCTION(BlueprintPure, Category = "Music Director")
	bool IsReadyForPlayback() const { return bIsReadyForPlayback; }

	UFUNCTION(BlueprintPure, Category = "Music Director")
	float GetCurrentBPM() const { return CurrentBPM; }

	UFUNCTION(BlueprintPure, Category = "Music Director")
	bool IsInBreakPeriod() const { return CurrentBreakEndTimeMS != -1; }

	UPROPERTY(BlueprintAssignable, Category = "Music Events")
	FOnBeatTriggered OnBeatTriggered;
	UPROPERTY(BlueprintAssignable, Category = "Music Events")
	FOnSongProgress OnSongProgress;
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
	// --- PRIVATE FUNCTIONS ---

	/** Creates UMusicActionInstance objects for notes that enter the lookahead window. */
	void ProcessNoteSpawning(int32 InCurrentTimeMS);
	void UpdateRhythmSection(int32 InCurrentTimeMS);
	void ProcessBeatTicks(int32 InCurrentTimeMS);
	void ResetState();
	
	// --- MEMBER VARIABLES ---

	/** A weak pointer to the manager actor, passed to new UMusicActionInstances for their ASC ActorInfo. */
	UPROPERTY()
	TObjectPtr<APcMusicGameplayManager> MusicManager;

	/** The lookahead time in milliseconds. Notes within this window will have an instance created. */
	UPROPERTY(EditAnywhere, Category = "Music Director", meta = (AllowPrivateAccess = "true"))
	int32 LookaheadTimeMS = 4000;

	// --- DATA STORAGE ---
	TArray<FPcMusicGameplayEvents> RhythmProfileRows;
	TArray<FPcMusicGameplayNotes> NoteEventRows;

	// --- STATE VARIABLES ---
	bool bIsReadyForPlayback = false;
	
	/** Tracks the next note to consider for spawning an instance. */
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