#pragma once

#include "CoreMinimal.h"
#include "MetasoundGeneratorHandle.h"
#include "GameFramework/Actor.h"
#include "MetasoundOutput.h"
#include "PcMusicManager.generated.h"

class UMetaSoundSource;
class UAudioComponent;
class UPcMusicConfigurationData;

UCLASS()
class PROJECT_CIRCLE_API APcMusicManager : public AActor
{
	GENERATED_BODY()

public:
	APcMusicManager();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music") TObjectPtr<UMetaSoundSource> MainMusicMetaSound;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music") TObjectPtr<UPcMusicConfigurationData> SongConfiguration;

	UFUNCTION(BlueprintCallable, Category = "Music") void StartMusicPlayback();

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UAudioComponent> MusicAudioComponent;
	
private:
	FOnMetasoundOutputValueChanged OnMetasoundOutputValueChanged;

	UFUNCTION()
	void OnMetaSoundTimeChanged(FName OutputName, const FMetaSoundOutput& Output);
};