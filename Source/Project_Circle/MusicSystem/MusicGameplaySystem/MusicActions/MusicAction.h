// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "MusicAction.generated.h"

class UPcMusicDirectorSubsystem;
class APcMusicGameplayManager;
class UMusicActionInstance;

/**
 * A self-contained, blueprintable action that can be executed by a UMusicActionInstance.
 * Its purpose is to perform a single, specific task in the world, such as spawning an actor,
 * have an enemy attack, or moving a platform. It overrides GetWorld() to allow for
 * world-context Blueprint nodes like SpawnActorFromClass.
 */
UCLASS(Blueprintable)
class PROJECT_CIRCLE_API UMusicAction : public UObject
{
	GENERATED_BODY()

public:
	// --- Core Functions ---

	/** Overrides the default UObject GetWorld to provide a valid world context. */
	virtual UWorld* GetWorld() const override;
	
	/** 
	 * [C++] Called by the UMusicActionInstance to start the action's lifecycle.
	 */
	void Execute(UMusicActionInstance* NoteContext);

	/** 
	 * The main execution function for this action. This is called immediately after the action is created.
	 * @param NoteContext The UMusicActionInstance that is executing this action, providing all necessary context.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Action", meta = (DisplayName = "ExecuteMusicAction"))
	void K2_OnExecuted(UMusicActionInstance* NoteContext);
	
	/**
	 * [BlueprintCallable] Call this from your Blueprint logic when the action is finished.
	 * This will stop it from receiving ticks and allow it to be garbage collected.
	 */
	UFUNCTION(BlueprintCallable, Category = "Action")
	void EndMusicAction();

	/** 
	 * [C++] Called by a delegate linked to the UPcMusicDirectorSubsystem "OnMusicTick"
	 * It's set inside the ExecuteMusicAction of this UObject.
	 */
	UFUNCTION()
	void MusicTick(float CurrentTimeMs);
	
	/**
	 * [Blueprint] The music-driven tick for this action. This will only fire if bShouldPerformMusicTick is true.
	 * It is called by MusicTick, which in turn is linked to the UPcMusicDirectorSubsystem "OnMusicTick".
	 * @param CurrentTimeMs The precise current time of the song in milliseconds.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Action", meta = (DisplayName = "On Music Tick"))
	void K2_OnMusicTick(float CurrentTimeMs);
	
	// --- Properties ---
	
	/**
	 * [Blueprint Read/Write] If true, this action will subscribe to and receive the Director's music tick.
	 * For fire-and-forget actions, keep this false to save performance.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Action|Ticking")
	bool bShouldPerformMusicTick = false;

private:
	/** Weak pointer to the director, used to unsubscribe from the tick when finished. */
	UPROPERTY()
	TWeakObjectPtr<UPcMusicDirectorSubsystem> DirectorSubsystem;
};