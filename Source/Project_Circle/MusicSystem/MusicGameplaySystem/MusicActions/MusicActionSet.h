// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Templates/SubclassOf.h"
#include "MusicActionSet.generated.h"

class UGameplayAbility;

/**
 * A simple, reliable enum to define the event phases in a note's lifecycle.
 * This is used as the key in the ActionMap.
 */
UENUM(BlueprintType)
enum class EMusicEventPhase : uint8
{
	OnPrepare	UMETA(DisplayName = "On Prepare (Enters Lookahead)"),
	OnHit		UMETA(DisplayName = "On Hit (Player Success)"),
	OnMiss		UMETA(DisplayName = "On Miss (Time Expired)")
};

/**
 * A wrapper struct to allow an array of abilities as a TMap value in the editor.
 */
USTRUCT(BlueprintType)
struct FMusicAbilityArray
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TArray<TSubclassOf<UGameplayAbility>> Abilities;
};

/**
 * Maps event phases (Enums) to a list of abilities to execute. This is a reusable
 * behavior template that can be assigned to any note.
 */
UCLASS()
class PROJECT_CIRCLE_API UMusicActionSet : public UDataAsset
{
	GENERATED_BODY()

public:
	/** The core mapping of an event phase to the list of abilities that should fire. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Note Abilities")
	TMap<EMusicEventPhase, FMusicAbilityArray> ActionMap;
};