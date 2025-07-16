// Fill out your copyright notice in the Description page of Project Settings.


#include "MusicGameplayEventProxy.h"


// Sets default values
AMusicGameplayEventProxy::AMusicGameplayEventProxy()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void AMusicGameplayEventProxy::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AMusicGameplayEventProxy::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

