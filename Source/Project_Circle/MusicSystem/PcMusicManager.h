// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MetasoundGeneratorHandle.h"
#include "MetasoundSource.h"
#include "GameFramework/Info.h"
#include "Engine/DataTable.h"
#include "Containers/Set.h"
#include "MusicData.h"
#include "PcMusicManager.generated.h"

class UAudioComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBeatTriggered, float, BeatTimestamp);

// The enum to classify the musical context of a section.
UENUM(BlueprintType)
enum class EGameplaySectionType : uint8
{
    Normal      UMETA(DisplayName = "Normal"),
    HighEnergy  UMETA(DisplayName = "High Energy"),
    Buildup     UMETA(DisplayName = "Buildup"),
    Cooldown    UMETA(DisplayName = "Cooldown"),
    Break       UMETA(DisplayName = "Break")
};

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

    UPROPERTY()
    int32 AnchorTimestampMS = 0;

    UPROPERTY()
    EGameplaySectionType SectionType = EGameplaySectionType::Normal;
};


UCLASS(Blueprintable)
class PROJECT_CIRCLE_API APcMusicManager : public AActor
{
    GENERATED_BODY()

public:
    APcMusicManager();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music")
    TObjectPtr<UMetaSoundSource> MainMusicMetaSound;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music")
    TObjectPtr<USoundWave> SongWaveAsset;
     
    UPROPERTY(EditAnywhere,BlueprintReadWrite, Category = "Music")
    UDataTable* DataTableMusicInfo;
     
    UFUNCTION(BlueprintCallable, Category = "Music")
    void StartMusicPlayback();

    UPROPERTY(BlueprintReadOnly, Category = "MusicState")
    float CurrentMusicProgress;

    UPROPERTY(BlueprintReadOnly, Category = "MusicState")
    float CurrentBPM;

    UPROPERTY(BlueprintReadOnly, Category = "MusicState")
    float CurrentBeatLengthMs;

    UPROPERTY(BlueprintReadOnly, Category = "MusicState", meta=(ToolTip="The musical context of the current section (Normal, High Energy, Buildup, or Break)."))
    EGameplaySectionType CurrentSectionType;

    UPROPERTY(BlueprintReadOnly, Category = "MusicState")
    int32 CurrentMeter;

    UPROPERTY(BlueprintReadOnly, Category = "MusicState|Break")
    bool bInBreakPeriod;

    UPROPERTY(BlueprintReadOnly, Category = "MusicState|Break")
    int32 CurrentBreakEndTimeMS;

    UPROPERTY(BlueprintAssignable, Category = "MusicEvents|GameplayActions")
    FOnBeatTriggered OnBeatTriggered;

    UFUNCTION(BlueprintImplementableEvent, Category = "MusicEvents|GameplayActions", meta=(DisplayName="OnBeatTriggered"))
    void K2_OnBeatTriggered(float BeatTimestamp);

    UFUNCTION(BlueprintImplementableEvent, Category = "MusicEvents|GameplayActions")
    void OnHitObjectTriggered(int32 HitTimestampMS, int32 HitObjectType);
    
    UFUNCTION(BlueprintImplementableEvent, Category = "MusicEvents|StateChanges", meta = (DisplayName = "OnSectionTypeChanged"))
    void K2_OnSectionTypeChanged(EGameplaySectionType NewSectionType);
     
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

    void ProcessMusicEvents(int32 InCurrentTimeMS);
    int32 NextEventIndex;

    UPROPERTY()
    FOnMetasoundOutputValueChanged OnMetasoundOutputValueChanged;
    
    UFUNCTION()
    void OnMetaSoundCurrentTimeChanged(FName OutputName, const FMetaSoundOutput& Output);
     
    void UpdateMeter(const FMusicData& TimingPointData);
    void CheckForBreakEnd(int32 InCurrentTimeMS);
    void ProcessBreakStartEvent(const FMusicData& BreakData);
    
    int32 NextBeatTimestampMS;
    void ProcessBeatTicks(int32 InCurrentTimeMS);
    int32 LastProcessedMusicProgressMs;

    UPROPERTY()
    TArray<FGameplayRhythmSection> RhythmSections;
    void AnalyzeAndGenerateRhythmSections();
    void UpdateRhythmSection(int32 InCurrentTimeMS);
    int32 CurrentRhythmSectionIndex;
    
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
    virtual void Tick(float DeltaTime) override;
};