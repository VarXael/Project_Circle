#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Project_Circle/MusicSystem/MusicImportSystem/PcMusicAnalysisTypes.h"
#include "PcMusicAnalysisSubsystem.generated.h"

class UDataTable;

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
	UFUNCTION(BlueprintCallable, Category = "Music Analysis")
	void InitializePlayback(UPcMusicConfigurationData* SongConfig);

	UFUNCTION(BlueprintCallable, Category = "Music Analysis")
	void UpdateMusicTime(float CurrentTimeSeconds);

	UFUNCTION(BlueprintPure, Category = "Music Analysis")
	bool IsReadyForPlayback() const { return bIsReadyForPlayback; }

	UFUNCTION(BlueprintPure, Category = "Music Analysis")
	float GetCurrentBPM() const { return CurrentBPM; }

	UFUNCTION(BlueprintPure, Category = "Music Analysis")
	float GetCurrentGameplayBPM() const { return CurrentGameplayBPM; }

	UFUNCTION(BlueprintPure, Category = "Music Analysis")
	int32 GetCurrentSubdivision() const { return BeatSubdivision; }

	UFUNCTION(BlueprintPure, Category = "Music Analysis")
	bool IsInBreakPeriod() const { return LastProcessedMusicProgressMs < CurrentBreakEndTimeMS; }

	UPROPERTY(BlueprintAssignable, Category = "Music Events")
	FOnBeatTriggered OnBeatTriggered;
	UPROPERTY(BlueprintAssignable, Category = "Music Events")
	FOnSongProgress OnSongProgress;
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
	UPROPERTY(BlueprintAssignable, Category = "Music Events")
	FOnSongEnd OnSongEnd;
	UPROPERTY(BlueprintAssignable, Category = "Music Events|Gameplay")
	FOnGameplayBeatTriggered OnGameplayBeatTriggered;
	UPROPERTY(BlueprintAssignable, Category = "Music Events|Gameplay")
	FOnGameplayBPMChanged OnGameplayBPMChanged;

private:
	void ProcessMusicEvents();
	void UpdateRhythmSection(int32 InCurrentTimeMS);
	void ProcessBeatTicks(int32 InCurrentTimeMS);
	void GenerateSliderSubEvents(const FPcImportedMusicData& SliderData);
	void ResetState();
	void UpdateGameplayBPM(const FPcRhythmSectionProfile& Section);

	TArray<FPcRhythmSectionProfile> RhythmProfileRows;
	TArray<FPcImportedMusicData>    RuntimeEventRows;
	TMap<int32, float>              MasterBeatLengths;

	bool  bIsReadyForPlayback          = false;
	int32 NextEventIndex               = 0;
	int32 LastProcessedMusicProgressMs = -1;
	int32 AbsoluteSongEndTimeMS        = -1;
	int32 NextBeatTimestampMS          = 0;
	int32 CurrentSectionIndex          = 0;
	int32 CurrentBeatInSession         = 0;
	float CurrentBPM                   = 0.f;
	int32 CurrentMeter                 = 4;
	int32 CurrentBreakEndTimeMS        = -1;
	TArray<FPcQueuedNoteEvent> NoteEventQueue;

	// Gameplay beat state
	float CurrentGameplayBPM  = 0.f;
	int32 BeatSubdivision     = 1;
	int32 RawBeatCounter      = 0;

	// Loaded from SongConfig on InitializePlayback.
	// Used as fallback when a section's GameplayBPM is left at 0 in the DataTable.
	float DefaultGameplayBPM  = 110.f;
};