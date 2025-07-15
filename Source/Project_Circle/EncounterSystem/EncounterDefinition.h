// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EncounterDefinition.generated.h"

class UEncounterActionSet;
/**
 * 
 */
UCLASS()
class PROJECT_CIRCLE_API UEncounterDefinition : public UDataAsset
{
	GENERATED_BODY()

	//todo: potentially in here you could have different type of sets: One for notes, another for type of things (beat drops), and more!
	UPROPERTY(EditAnywhere,Instanced)
	TArray<UEncounterActionSet*> EncounterActionSets;
};
