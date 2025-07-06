// Fill out your copyright notice in the Description page of Project Settings.


#include "PcMaestroMusicSubsystem.h"

UPcMaestroMusicSubsystem::UPcMaestroMusicSubsystem()
{
	
}

void UPcMaestroMusicSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UPcMaestroMusicSubsystem::Deinitialize()
{
	Super::Deinitialize();
}

bool UPcMaestroMusicSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return true;
}