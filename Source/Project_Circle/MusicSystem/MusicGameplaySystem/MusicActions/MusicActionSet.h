// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "MusicActionSet.generated.h"

class UMusicActionInstance;
class UMusicAction;

/**
 * A struct that pairs a Blueprintable UMusicAction class with a Gameplay Tag for designers to reference.
 */
USTRUCT(BlueprintType)
struct FTaggedMusicAction
{
	GENERATED_BODY()

	/** The unique tag that the MusicActionInstance Blueprint will use to call this action. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTag ActionTag;

	/** The UMusicAction Blueprint class that contains the logic for this action. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSubclassOf<UMusicAction> ActionClass;
};

/**
 * A Data Asset that serves as a "Note Type" template. It defines which Blueprint 'Brain'
 * to use for a note and provides a library of actions that the 'Brain' can execute by tag.
 * This asset can be reused across many different notes in a DataTable.
 */
UCLASS()
class PROJECT_CIRCLE_API UMusicActionSet : public UDataAsset
{
	GENERATED_BODY()

public:
	/** The Blueprint class of the UMusicActionInstance to spawn for notes using this set. This is the 'Brain'. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Note Instance")
	TSubclassOf<UMusicActionInstance> MusicActionInstanceClass;

	/** The library of actions this 'Brain' is allowed to look up and execute by tag. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Note Actions")
	TArray<FTaggedMusicAction> ActionLibrary;
};