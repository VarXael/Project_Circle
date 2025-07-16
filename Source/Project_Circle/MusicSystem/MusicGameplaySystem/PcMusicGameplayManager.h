// --- START OF FILE PcMusicManager.h ---

#pragma once

#include "CoreMinimal.h"
#include "MetasoundGeneratorHandle.h"
#include "GameFramework/Actor.h"
#include "MetasoundOutput.h"
#include "PcMusicGameplayManager.generated.h"

class UMetaSoundSource;
class USoundWave;
class UDataTable;
class UAudioComponent;
class UPcMusicConfigurationData;

UCLASS()
class PROJECT_CIRCLE_API APcMusicGameplayManager : public AActor
{
	GENERATED_BODY()

public:
	APcMusicGameplayManager();

	// --- Gameplay Data ---
	/** The master configuration asset for the song to be played. This contains references to the generated data tables. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music|Gameplay Data")
	TObjectPtr<UPcMusicConfigurationData> SongConfiguration;
     
	/** Begins music playback using the assigned Song Configuration asset. */
	UFUNCTION(BlueprintCallable, Category = "Music")
	void StartMusicPlayback();

	UPROPERTY(VisibleAnywhere,BlueprintReadOnly, Category = "Music")
	float CurrentSongProgressInSeconds;
	
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly, Category = "Music")
	float CurrentSongProgressInMs;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UAudioComponent> MusicAudioComponent;
	
private:
	UPROPERTY()
	FOnMetasoundOutputValueChanged OnMetasoundOutputValueChanged;

	UFUNCTION()
	void OnMetaSoundTimeChanged(FName OutputName, const FMetaSoundOutput& Output);

public:
	virtual void Tick(float DeltaSeconds) override;
};