// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MusicData.h"
#include "Subsystems/WorldSubsystem.h"
#include "PcMaestroMusicSubsystem.generated.h"

class APcMusicManager;
class UMetaSoundSource;
/**
 * 
 */
UCLASS()
class PROJECT_CIRCLE_API UPcMaestroMusicSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UPcMaestroMusicSubsystem();
	
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	
};
