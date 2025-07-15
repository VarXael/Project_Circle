// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "EncounterManager.generated.h"

class UEncounterDefinition;

UCLASS()
class PROJECT_CIRCLE_API AEncounterManager : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AEncounterManager();

	UPROPERTY(EditAnywhere, Category = "Encounter")
	UEncounterDefinition* UEncounterDefinition;

	//todo you will need to change the song data, right now you have RhythmDefinition, and ImportedMusicData, you will need a third, something called NoteData, which will be used by this actor
	UPROPERTY(EditAnywhere, Category = "Encounter")
	UDataTable* MusicConfigurationData;
	
	UFUNCTION(CallInEditor, Category = "Encounter")
	void GenerateNewEncounterDefinitionFromConfigurationData();
	
protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;
};
