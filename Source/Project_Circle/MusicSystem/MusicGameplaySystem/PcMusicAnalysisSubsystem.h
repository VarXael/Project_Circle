#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Project_Circle/MusicSystem/MusicImportSystem/PcMusicAnalysisTypes.h"
#include "PcMusicAnalysisSubsystem.generated.h"

class UDataTable;
class UPcMusicConfigurationData;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBeatTriggered, float, BeatTimestamp);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSongProgress, float, CurrentSongProgress);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnNoteHit, int32, TimestampMS, int32, NoteType, int32, HitSound);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBPMChanged, float, NewBPM);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMeterChanged, int32, NewMeter);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBreakPeriod, int32, StartTimeMS, int32, EndTimeMS);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSongEnd, float, EndTimeSeconds);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGameplayBeatTriggered, float, BeatTimestamp);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGameplayBPMChanged, float, NewGameplayBPM);

UCLASS()
class PROJECT_CIRCLE_API UPcMusicAnalysisSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Music Analysis") void InitializePlayback(UPcMusicConfigurationData* SongConfig);
	UFUNCTION(BlueprintCallable, Category = "Music Analysis") void UpdateMusicTime(float CurrentTimeSeconds);

	UFUNCTION(BlueprintPure, Category = "Music Analysis") bool IsReadyForPlayback() const { return bIsReadyForPlayback; }
	UFUNCTION(BlueprintPure, Category = "Music Analysis") float GetCurrentBPM() const { return CurrentBPM; }
	UFUNCTION(BlueprintPure, Category = "Music Analysis") float GetCurrentGameplayBPM() const { return CurrentGameplayBPM; }
	UFUNCTION(BlueprintPure, Category = "Music Analysis") float GetCurrentSubdivision() const { return BeatSubdivision; }
	UFUNCTION(BlueprintPure, Category = "Music Analysis") EPcMovementPresetOverride GetCurrentPresetOverride() const { return CurrentPresetOverride; }

	// --- PRESET ACCESSORS ---
	UFUNCTION(BlueprintPure, Category = "Music Analysis") const FPcMovementPreset& GetCurrentPulsePreset() const;
	UFUNCTION(BlueprintPure, Category = "Music Analysis") FString GetActivePresetName() const;

	// --- HUD / GAMEPLAY ACCESSORS (The TRUE Gameplay Metronome) ---
	UFUNCTION(BlueprintPure, Category = "Music Analysis|UI") int32 GetCurrentPlaybackTimeMS() const { return LastProcessedMusicProgressMs; }
	UFUNCTION(BlueprintPure, Category = "Music Analysis|UI") float GetGameplayBeatIntervalMS() const;
	UFUNCTION(BlueprintPure, Category = "Music Analysis|UI") int32 GetNextGameplayBeatTimeMS() const { return NextGameplayBeatTimestampMS; }
	UFUNCTION(BlueprintPure, Category = "Music Analysis|UI") float GetTimeUntilNextGameplayBeat() const;
	UFUNCTION(BlueprintPure, Category = "Music Analysis|UI") TArray<FPcRuntimeEvent> GetUpcomingNotes(float LookaheadWindowSec) const;

	UPROPERTY(BlueprintAssignable, Category = "Music Events") FOnBeatTriggered OnBeatTriggered; // (Raw Beat - Under the hood)
	UPROPERTY(BlueprintAssignable, Category = "Music Events") FOnSongProgress OnSongProgress;
	UPROPERTY(BlueprintAssignable, Category = "Music Events") FOnNoteHit OnNoteHit;
	UPROPERTY(BlueprintAssignable, Category = "Music Events") FOnBPMChanged OnBPMChanged;
	UPROPERTY(BlueprintAssignable, Category = "Music Events") FOnMeterChanged OnMeterChanged;
	UPROPERTY(BlueprintAssignable, Category = "Music Events") FOnBreakPeriod OnBreakStart;
	UPROPERTY(BlueprintAssignable, Category = "Music Events") FOnBreakPeriod OnBreakEnd;
	UPROPERTY(BlueprintAssignable, Category = "Music Events") FOnSongEnd OnSongEnd;
	
	// THIS IS THE ONE EVERYTHING CARES ABOUT NOW!
	UPROPERTY(BlueprintAssignable, Category = "Music Events|Gameplay") FOnGameplayBeatTriggered OnGameplayBeatTriggered;
	UPROPERTY(BlueprintAssignable, Category = "Music Events|Gameplay") FOnGameplayBPMChanged OnGameplayBPMChanged;

private:
	void ProcessMusicEvents();
	void ProcessBeatTicks(int32 InCurrentTimeMS);
	void UpdateRhythmSection(int32 InCurrentTimeMS);
	void ResetState();

	TArray<FPcRhythmSectionProfile> RhythmSections;
	TArray<FPcRuntimeEvent>         RuntimeEvents;

	FPcMovementPreset PulseSlow;
	FPcMovementPreset PulseNormal;
	FPcMovementPreset PulseFast;
	FPcMovementPreset PulseVeryFast;

	bool  bIsReadyForPlayback          = false;
	int32 NextEventIndex               = 0;
	int32 LastProcessedMusicProgressMs = -1;
	
	int32 CurrentSectionIndex          = 0;
	
	int32 NextBeatTimestampMS          = 0; // Raw beat tracker
	int32 CurrentBeatInSession         = 0;
	
	int32 NextGameplayBeatTimestampMS  = 0; // NEW: The unified Gameplay Tracker
	
	float CurrentBPM                   = 0.f;
	float CurrentGameplayBPM           = 0.f;
	float BeatSubdivision              = 1.0f;
	float DefaultGameplayBPM           = 110.f;
	EPcMovementPresetOverride CurrentPresetOverride = EPcMovementPresetOverride::Auto;
};