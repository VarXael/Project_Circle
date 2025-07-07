// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MetasoundGeneratorHandle.h"
#include "GameFramework/Actor.h"
#include "MetasoundOutput.h" // Needed for FOnMetasoundOutputValueChanged
#include "PcMusicManager.generated.h"

class UMetaSoundSource;
class USoundWave;
class UDataTable;
class UAudioComponent;

UCLASS()
class PROJECT_CIRCLE_API APcMusicManager : public AActor
{
	GENERATED_BODY()

public:
	APcMusicManager();

	/** The Metasound Source to use for playback. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music")
	TObjectPtr<UMetaSoundSource> MainMusicMetaSound;

	/** The song's audio file, to be fed into the Metasound. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music")
	TObjectPtr<USoundWave> SongWaveAsset;
     
	/** The DataTable containing the comprehensive data parsed from the .osu file. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music")
	UDataTable* PrimaryDataTableMusicInfo;
	/** The DataTable containing the comprehensive data parsed from the .osu file. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music")
	TArray<UDataTable*> DataTableMusicInfo;
     
	/** Begins the analysis in the subsystem and starts music playback. */
	UFUNCTION(BlueprintCallable, Category = "Music")
	void StartMusicPlayback(float DifficultyBias);

	UPROPERTY(VisibleAnywhere,BlueprintReadOnly, Category = "Music")
	float CurrentSongProgressInSeconds;
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly, Category = "Music")
	float CurrentSongProgressInMs;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaTime) override;

	/** The audio component that will play the Metasound. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UAudioComponent> MusicAudioComponent;
	
private:
	/** 
	 * The delegate that binds to the Metasound's 'CurrentTime' output. 
	 * This is the high-precision clock for our system.
	 */
	UPROPERTY()
	FOnMetasoundOutputValueChanged OnMetasoundOutputValueChanged;

	/**
	 * This UFUNCTION is called by the Metasound system whenever the time value changes.
	 * It reports the new time to the UMusicAnalysisSubsystem.
	 */
	UFUNCTION()
	void OnMetaSoundTimeChanged(FName OutputName, const FMetaSoundOutput& Output);
};