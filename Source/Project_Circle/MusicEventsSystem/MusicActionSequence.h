// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "MusicActionSequence.generated.h"
class UMusicAction;

/**
 * 
 */
UCLASS()
class PROJECT_CIRCLE_API UMusicActionSequence : public UDataAsset
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, Instanced,Category = "Music Gameplay Event Actions")
	TArray<UMusicAction*> MusicActions;
};
