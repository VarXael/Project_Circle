// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "EncounterActionSet.generated.h"

class UEncounterAction;
/**
 * 
 */
UCLASS(EditInlineNew, Blueprintable, BlueprintType)
class PROJECT_CIRCLE_API UEncounterActionSet : public UObject
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	float TimeStamp;

	//todo: Event type Variable: Note? Music Drop?

	//todo: See if you can find a way to order/filter them by event!
	
	UPROPERTY(EditAnywhere, Instanced)
	TArray<UEncounterAction*> Actions;
};
