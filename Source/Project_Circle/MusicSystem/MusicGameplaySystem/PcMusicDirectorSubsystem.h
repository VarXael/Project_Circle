#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Project_Circle/MusicSystem/MusicImportSystem/PcMusicAnalysisTypes.h"
#include "PcMusicDirectorSubsystem.generated.h"

class AMusicProxy;
class UDataTable;
class UPcMusicConfigurationData;

// --- DELEGATE DEFINITIONS ---

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBeatTriggered, float, BeatTimestamp);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSongProgress, float, CurrentSongProgress);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNoteHit, int32, TimestampMS);
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
	UFUNCTION(BlueprintCallable, Category = "Music Analysis")
	void InitializePlayback(UPcMusicConfigurationData* SongConfig);

	UFUNCTION(BlueprintCallable, Category = "Music Analysis")
	void UpdateMusicTime(float CurrentTimeSeconds);

	UFUNCTION(BlueprintPure, Category = "Music Analysis")
	bool IsReadyForPlayback() const { return bIsReadyForPlayback; }

	UFUNCTION(BlueprintPure, Category = "Music Analysis")
	float GetCurrentBPM() const { return CurrentBPM; }
	
	// +++ RE-INTRODUCED HELPER FUNCTION +++
	UFUNCTION(BlueprintPure, Category = "Music Analysis")
	bool IsInBreakPeriod() const { return CurrentBreakEndTimeMS != -1; }

	// --- DELEGATES ---
	UPROPERTY(BlueprintAssignable, Category = "Music Events")
	FOnBeatTriggered OnBeatTriggered;
	UPROPERTY(BlueprintAssignable, Category = "Music Events")
	FOnSongProgress OnSongProgress;
	UPROPERTY(BlueprintAssignable, Category = "Music Events")
	FOnNoteHit OnNoteHit;
	UPROPERTY(BlueprintAssignable, Category = "Music Events")
	FOnBPMChanged OnBPMChanged;
	UPROPERTY(BlueprintAssignable, Category = "Music Events")
	FOnSongEnd OnSongEnd;
	
	// +++ RE-INTRODUCED DELEGATES +++
	UPROPERTY(BlueprintAssignable, Category = "Music Events")
	FOnMeterChanged OnMeterChanged;
	UPROPERTY(BlueprintAssignable, Category = "Music Events")
	FOnBreakPeriod OnBreakStart;
	UPROPERTY(BlueprintAssignable, Category = "Music Events")
	FOnBreakPeriod OnBreakEnd;

private:
	// --- PRIVATE FUNCTIONS ---
	void ProcessMusicEvents();
	void UpdateRhythmSection(int32 InCurrentTimeMS);
	void ProcessBeatTicks(int32 InCurrentTimeMS);
	void ResetState();
	
	// --- MEMBER VARIABLES ---
	TArray<FPcMusicGameplayEvents> RhythmProfileRows;
	TArray<FPcMusicGameplayNotes> NoteEventRows;

	bool bIsReadyForPlayback = false;
	int32 NextNoteIndex = 0;
	int32 LastProcessedMusicProgressMs = -1;
	int32 AbsoluteSongEndTimeMS = -1;
	int32 CurrentSectionIndex = 0;
	int32 NextBeatTimestampMS = 0;
	int32 CurrentBeatInSession = 0;
	float CurrentBPM = 0.f;
	int32 CurrentMeter = 4;
	int32 CurrentBreakEndTimeMS = -1;



	//todo TEMPORARY, MAY REQUIRE RE-ARCHITECTURALIZATION
	UFUNCTION(BlueprintCallable)
	AMusicProxy* SpawnMusicProxy(TSubclassOf<AMusicProxy> ProxyClass, const FTransform& SpawnTransform);

};