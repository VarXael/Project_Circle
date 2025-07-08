#pragma once

#include "CoreMinimal.h"
#include "MetasoundGeneratorHandle.h"
#include "GameFramework/Actor.h"
#include "MetasoundOutput.h"
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

	// --- Sound Assets ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music")
	TObjectPtr<UMetaSoundSource> MainMusicMetaSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music")
	TObjectPtr<USoundWave> SongWaveAsset;

	// --- Gameplay Data ---
	/** The DataTable containing the hand-tuned rhythm sections (FRhythmSectionProfile). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music|Gameplay Data")
	TObjectPtr<UDataTable> TunedRhythmProfile;
	
	/** The DataTable containing the notes for the difficulty being played (FMusicData). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music|Gameplay Data")
	TObjectPtr<UDataTable> NoteEventMap;
     
	/** Begins music playback using the assigned DataTables. */
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