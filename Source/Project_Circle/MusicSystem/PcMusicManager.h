// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MetasoundGeneratorHandle.h"
#include "MetasoundSource.h"
#include "GameFramework/Info.h"
#include "Engine/DataTable.h"
#include "MusicData.h"

#include "PcMusicManager.generated.h"

class UAudioComponent;


//DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMusicCueTrigger,const FMusicData&, OnMusicCueTrigger);

UCLASS(Blueprintable)
class PROJECT_CIRCLE_API APcMusicManager : public AActor
{
    GENERATED_BODY()

public:
    // Sets default values for this actor's properties
    APcMusicManager();

    /**
     * Music stuff set in the editor
     */
    UPROPERTY(EditAnywhere,BlueprintReadWrite, Category = "Music")
    UDataTable* DataTableMusicInfo;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music")
    TObjectPtr<UMetaSoundSource> MainMusicMetaSound;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music")
    TObjectPtr<USoundWave> SongWaveAsset;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Music")
    TObjectPtr<UAudioComponent> MusicAudioComponent;

    /**
     * Music Mapping Data
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Music")
    TArray<FMusicData> LoadedMusicData;

    UFUNCTION(BlueprintCallable, Category = "Music")
    bool LoadMusicDataFromTable();
    
    /**
     * Music stuff used in gameplay and BP
     */
    UFUNCTION(BlueprintCallable, Category = "Music")
    void StartMusicPlayback();

    UFUNCTION(BlueprintImplementableEvent, Category = "MusicEvents")
    void OnMusicEventTriggered(const FMusicData& MusicData); 

    UFUNCTION(BlueprintImplementableEvent, Category = "Music")
    void MusicTick(int32 CurrentSongProgress);
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Music")
    float CurrentMusicProgress;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Music")
    int32 CurrentMusicProgressMS;

    /**
     * Internal stuff 
     */
    UPROPERTY()
    int32 NextEventIndex;

    UPROPERTY()
	FOnMetasoundOutputValueChanged OnMetasoundOutputValueChanged;

    UFUNCTION()
    void OnMetaSoundCurrentTimeChanged(FName OutputName, const FMetaSoundOutput& Output);

    UFUNCTION(BlueprintCallable, Category = "Music")
    bool GetMusicDataAt(const TArray<FMusicData>& SortedArray, int32 SongProgress, FMusicData& OutMusicData);
private:
	void ProcessMusicEvents(int32 InCurrentTimeMS);
protected:
    // Called when the game starts or when spawned
    virtual void BeginPlay() override;

public:
    // Called every frame to drive the MetaSound's playback time
    virtual void Tick(float DeltaTime) override;

protected:
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
};