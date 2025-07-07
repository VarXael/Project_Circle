// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MetasoundGeneratorHandle.h"
#include "MetasoundSource.h"
#include "GameFramework/Info.h"
#include "Engine/DataTable.h"
#include "Containers/Set.h" // Required for TSet
#include "MusicData.h"
#include "PcMusicManager.generated.h"

class UAudioComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBeatTriggered, float, BeatTimestamp);

// This struct holds the final, pre-calculated gameplay data for a single, stable rhythmic section.
USTRUCT(BlueprintType)
struct FGameplayRhythmSection
{
    GENERATED_BODY()

    UPROPERTY()
    int32 StartTimeMS = 0;

    UPROPERTY()
    float BPM = 120.f;

    UPROPERTY()
    float BeatLengthMS = 500.f;

    // The timestamp of the first note in this section, used to anchor the beat.
    UPROPERTY()
    int32 AnchorTimestampMS = 0;

    // True if this section is a high-intensity part of the song (e.g., a beat drop).
    UPROPERTY()
    bool bIsHighIntensity = false;
};


UCLASS(Blueprintable)
class PROJECT_CIRCLE_API APcMusicManager : public AActor
{
    GENERATED_BODY()

public:
    APcMusicManager();

    /** Music stuff set in the editor */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music")
    TObjectPtr<UMetaSoundSource> MainMusicMetaSound;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music")
    TObjectPtr<USoundWave> SongWaveAsset;
     
    UPROPERTY(EditAnywhere,BlueprintReadWrite, Category = "Music")
    UDataTable* DataTableMusicInfo;
     
    /** Music stuff used in gameplay and BP */
    UFUNCTION(BlueprintCallable, Category = "Music")
    void StartMusicPlayback();

    /** Core Variables and Events that drive gameplay */
    UPROPERTY(BlueprintReadOnly, Category = "MusicState")
    float CurrentMusicProgress;

    UPROPERTY(BlueprintReadOnly, Category = "MusicState")
    float CurrentBPM;

    UPROPERTY(BlueprintReadOnly, Category = "MusicState")
    float CurrentBeatLengthMs;

    UPROPERTY(BlueprintReadOnly, Category = "MusicState", meta=(ToolTip="Represents if the song is currently in a high-intensity section, like a beat drop."))
    bool bIsInHighIntensity;

    UPROPERTY(BlueprintReadOnly, Category = "MusicState")
    int32 CurrentMeter;

    UPROPERTY(BlueprintReadOnly, Category = "MusicState|Break")
    bool bInBreakPeriod;

    UPROPERTY(BlueprintReadOnly, Category = "MusicState|Break")
    int32 CurrentBreakEndTimeMS;

    /** This is the core beat event, triggered precisely on every beat of the song. */
    UPROPERTY(BlueprintAssignable, Category = "MusicEvents|GameplayActions")
    FOnBeatTriggered OnBeatTriggered;

    /** Blueprint-implementable version of the beat event for convenience. */
    UFUNCTION(BlueprintImplementableEvent, Category = "MusicEvents|GameplayActions", meta=(DisplayName="OnBeatTriggered"))
    void K2_OnBeatTriggered(float BeatTimestamp);

    UFUNCTION(BlueprintImplementableEvent, Category = "MusicEvents|GameplayActions")
    void OnHitObjectTriggered(int32 HitTimestampMS, int32 HitObjectType);
     
    UFUNCTION(BlueprintImplementableEvent, Category = "MusicEvents|StateChanges")
    void OnIntensityChanged(bool bNewIsInHighIntensity);
     
    UFUNCTION(BlueprintImplementableEvent, Category = "MusicEvents|StateChanges")
    void OnBPMChanged(float NewBPM);
     
    UFUNCTION(BlueprintImplementableEvent, Category = "MusicEvents|StateChanges")
    void OnMeterChanged(int32 NewMeter);
     
    UFUNCTION(BlueprintImplementableEvent, Category = "MusicEvents|GameplayActions")
    void OnBreakStart(int32 StartTimeMS, int32 EndTimeMS);
     
    UFUNCTION(BlueprintImplementableEvent, Category = "MusicEvents|GameplayActions")
    void OnBreakEnd(int32 EndTimeMS);
     
private:
    UPROPERTY()
    TObjectPtr<UAudioComponent> MusicAudioComponent;
    
    UPROPERTY()
    TArray<FMusicData> LoadedMusicData;
    
    UFUNCTION()
    bool LoadMusicDataFromTable();

    /** Main function used to Process Core Variables and trigger Core Gameplay events */
    void ProcessMusicEvents(int32 InCurrentTimeMS);
    int32 NextEventIndex;

    UPROPERTY()
    FOnMetasoundOutputValueChanged OnMetasoundOutputValueChanged;
    
    UFUNCTION()
    void OnMetaSoundCurrentTimeChanged(FName OutputName, const FMetaSoundOutput& Output);
     
    /** Functions used to update core variables, used inside process music events when specified. */
    void UpdateMeter(const FMusicData& TimingPointData);
    void CheckForBreakEnd(int32 InCurrentTimeMS);
    void ProcessBreakStartEvent(const FMusicData& BreakData);

    /** This is the heart of the beat trigger system. It holds the exact millisecond timestamp for the next beat. */
    int32 NextBeatTimestampMS;

    /** Main function for processing the beat tick events. */
    void ProcessBeatTicks(int32 InCurrentTimeMS);
    
    /** Used in OnMetaSoundCurrentTimeChanged to prevent redundant processing. */
    int32 LastProcessedMusicProgressMs;

    // ~~~ DEFINITIVE ANALYSIS SYSTEM ~~~
    
    // The pre-calculated list of stable rhythm sections for gameplay.
    UPROPERTY()
    TArray<FGameplayRhythmSection> RhythmSections;

    // Performs a one-time, deep analysis of the song to generate the RhythmSections.
    void AnalyzeAndGenerateRhythmSections();

    // During playback, this function updates the state based on the pre-calculated sections.
    void UpdateRhythmSection(int32 InCurrentTimeMS);
    int32 CurrentRhythmSectionIndex;
    
    // ~~~ END DEFINITIVE ANALYSIS SYSTEM ~~~
     
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
    virtual void Tick(float DeltaTime) override;
};