// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "EncounterProxy.generated.h"

class UMusicAction;

UCLASS()
class PROJECT_CIRCLE_API AEncounterProxy : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AEncounterProxy();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;
};
