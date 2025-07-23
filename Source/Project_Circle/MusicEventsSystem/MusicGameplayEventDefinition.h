// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "MusicGameplayEventDefinition.generated.h"
class UMusicGameplayEventAbility;

/**
 * 
 */
UCLASS()
class PROJECT_CIRCLE_API UMusicGameplayEventDefinition : public UDataAsset
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Instanced,Category = "Music Gameplay Event Actions")
	TArray<UMusicGameplayEventAbility*> MusicGameplayEventActions;
};
