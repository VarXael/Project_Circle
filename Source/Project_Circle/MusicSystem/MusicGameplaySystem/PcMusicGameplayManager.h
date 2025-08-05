#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AbilitySystemInterface.h" // NEW: Required for GAS
#include "MetasoundGeneratorHandle.h"
#include "MetasoundOutput.h"
#include "PcMusicGameplayManager.generated.h"

class UAbilitySystemComponent; // NEW: Forward declare
class UMetaSoundSource;
class USoundWave;
class UDataTable;
class UAudioComponent;
class UPcMusicConfigurationData;

UCLASS()
class PROJECT_CIRCLE_API APcMusicGameplayManager : public AActor, public IAbilitySystemInterface // NEW: Inherit from interface
{
	GENERATED_BODY()

public:
	APcMusicGameplayManager();

	// --- Gameplay Data ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music|Gameplay Data")
	TObjectPtr<UPcMusicConfigurationData> SongConfiguration;
     
	UFUNCTION(BlueprintCallable, Category = "Music")
	void StartMusicPlayback();

	// --- GAS Interface ---
	// NEW: Implement the IAbilitySystemInterface
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	// --- Song Progress (Unchanged) ---
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly, Category = "Music")
	float CurrentSongProgressInSeconds;
	
	UPROPERTY(VisibleAnywhere,BlueprintReadOnly, Category = "Music")
	float CurrentSongProgressInMs;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// --- Components ---
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UAudioComponent> MusicAudioComponent;

	// NEW: Ability System Component for this actor
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;
	
private:
	UPROPERTY()
	FOnMetasoundOutputValueChanged OnMetasoundOutputValueChanged;

	UFUNCTION()
	void OnMetaSoundTimeChanged(FName OutputName, const FMetaSoundOutput& Output);

public:
	virtual void Tick(float DeltaSeconds) override;
};