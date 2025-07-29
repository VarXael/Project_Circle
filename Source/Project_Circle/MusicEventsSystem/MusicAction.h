// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MusicAction.generated.h"

class UPcMusicDirectorSubsystem;
/**
 * 
 */
UCLASS(EditInlineNew, Blueprintable, BlueprintType)
class PROJECT_CIRCLE_API UMusicAction : public UObject
{
	GENERATED_BODY()
	
public:
	UFUNCTION(BlueprintImplementableEvent)
	void OnExecute(UPcMusicDirectorSubsystem* WorldContext);
};
