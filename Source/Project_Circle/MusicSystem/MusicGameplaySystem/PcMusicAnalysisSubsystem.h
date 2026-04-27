#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Project_Circle/MusicSystem/MusicImportSystem/PcMusicAnalysisTypes.h"
#include "PcMusicAnalysisSubsystem.generated.h"

class UDataTable;
class UPcMusicConfigurationData;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBeatTriggered,        float, BeatTimestamp);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSongProgress,         float, CurrentSongProgress);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnNoteHit,           int32, TimestampMS, int32, NoteType, int32, HitSound);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBPMChanged,           float, NewBPM);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMeterChanged,         int32, NewMeter);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBreakPeriod,         int32, StartTimeMS, int32, EndTimeMS);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSongEnd,              float, EndTimeSeconds);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGameplayBeatTriggered, float, BeatTimestamp);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGameplayBPMChanged,   float, NewGameplayBPM);

// Fired when the section switches between Normal and Enhanced (the drop).
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPresetChanged, bool, bIsEnhanced);

UCLASS()
class PROJECT_CIRCLE_API UPcMusicAnalysisSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Music Analysis") void InitializePlayback(UPcMusicConfigurationData* SongConfig);
	UFUNCTION(BlueprintCallable, Category = "Music Analysis") void UpdateMusicTime(float CurrentTimeSeconds);

	// ── State ─────────────────────────────────────────────────────────────────
	UFUNCTION(BlueprintPure, Category = "Music Analysis") bool  IsReadyForPlayback()  const { return bIsReadyForPlayback; }
	UFUNCTION(BlueprintPure, Category = "Music Analysis") float GetCurrentBPM()       const { return CurrentBPM; }
	UFUNCTION(BlueprintPure, Category = "Music Analysis") float GetCurrentGameplayBPM() const { return CurrentGameplayBPM; }
	UFUNCTION(BlueprintPure, Category = "Music Analysis") bool  IsEnhanced()          const { return bCurrentSectionIsEnhanced; }

	// ── Preset ────────────────────────────────────────────────────────────────
	UFUNCTION(BlueprintPure, Category = "Music Analysis") const FPcMovementPreset& GetCurrentPulsePreset() const;
	UFUNCTION(BlueprintPure, Category = "Music Analysis") FString                  GetActivePresetName()   const;

	// ── Gameplay metronome ────────────────────────────────────────────────────
	UFUNCTION(BlueprintPure, Category = "Music Analysis|UI") int32                    GetCurrentPlaybackTimeMS()    const { return LastProcessedMusicProgressMs; }
	UFUNCTION(BlueprintPure, Category = "Music Analysis|UI") float                    GetGameplayBeatIntervalMS()   const;
	UFUNCTION(BlueprintPure, Category = "Music Analysis|UI") int32                    GetNextGameplayBeatTimeMS()   const { return NextGameplayBeatTimestampMS; }
	UFUNCTION(BlueprintPure, Category = "Music Analysis|UI") float                    GetTimeUntilNextGameplayBeat() const;
	UFUNCTION(BlueprintPure, Category = "Music Analysis|UI") TArray<FPcRuntimeEvent>  GetUpcomingNotes(float LookaheadWindowSec) const;

	// ── Delegates ─────────────────────────────────────────────────────────────
	UPROPERTY(BlueprintAssignable, Category = "Music Events") FOnBeatTriggered         OnBeatTriggered;       // raw beat, internal use
	UPROPERTY(BlueprintAssignable, Category = "Music Events") FOnSongProgress          OnSongProgress;
	UPROPERTY(BlueprintAssignable, Category = "Music Events") FOnNoteHit               OnNoteHit;
	UPROPERTY(BlueprintAssignable, Category = "Music Events") FOnBPMChanged            OnBPMChanged;
	UPROPERTY(BlueprintAssignable, Category = "Music Events") FOnMeterChanged          OnMeterChanged;
	UPROPERTY(BlueprintAssignable, Category = "Music Events") FOnBreakPeriod           OnBreakStart;
	UPROPERTY(BlueprintAssignable, Category = "Music Events") FOnBreakPeriod           OnBreakEnd;
	UPROPERTY(BlueprintAssignable, Category = "Music Events") FOnSongEnd               OnSongEnd;

	// The one everything cares about:
	UPROPERTY(BlueprintAssignable, Category = "Music Events|Gameplay") FOnGameplayBeatTriggered OnGameplayBeatTriggered;
	UPROPERTY(BlueprintAssignable, Category = "Music Events|Gameplay") FOnGameplayBPMChanged    OnGameplayBPMChanged;
	UPROPERTY(BlueprintAssignable, Category = "Music Events|Gameplay") FOnPresetChanged         OnPresetChanged;

private:
	void ProcessMusicEvents();
	void ProcessBeatTicks(int32 InCurrentTimeMS);
	void UpdateRhythmSection(int32 InCurrentTimeMS);
	void ResetState();

	// Takes an explicit target so Enhanced sections can pass TargetGameplayBPM * 2
	float ComputeGameplayBPM(float RawBPM, float Target) const;

	TArray<FPcRhythmSectionProfile> RhythmSections;
	TArray<FPcRuntimeEvent>         RuntimeEvents;

	FPcMovementPreset PulseNormal;
	FPcMovementPreset PulseEnhanced;

	float TargetGameplayBPM             = 55.f;

	bool  bIsReadyForPlayback           = false;
	int32 NextEventIndex                = 0;
	int32 LastProcessedMusicProgressMs  = -1;

	int32 CurrentSectionIndex           = 0;

	int32 NextBeatTimestampMS           = 0;   // raw beat tracker
	int32 CurrentBeatInSession          = 0;

	int32 NextGameplayBeatTimestampMS   = 0;   // the gameplay metronome

	float CurrentBPM                    = 0.f;
	float CurrentGameplayBPM            = 0.f;
	bool  bCurrentSectionIsEnhanced     = false;
};