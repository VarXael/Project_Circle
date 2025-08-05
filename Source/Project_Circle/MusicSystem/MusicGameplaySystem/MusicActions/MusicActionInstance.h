// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayAbilitySpec.h" // Required for FGameplayAbilitySpecHandle
#include "MusicActionSet.h"
#include "Project_Circle/MusicSystem/MusicImportSystem/PcMusicAnalysisTypes.h"
#include "MusicActionInstance.generated.h"

class UAbilitySystemComponent;
class APcMusicGameplayManager;
class UPcMusicDirectorSubsystem;

/** An enum to represent the internal state of an active note. */
UENUM()
enum class EMusicNoteState : uint8
{
	None,
	Approaching,
	Hit,
	Missed
};

/**
 * The autonomous "brain" for a single musical note. This lightweight UObject is created
 * for each note that enters the gameplay window. It manages its own state, listens
 * for time updates from the PcMusicDirectorSubsystem, and executes abilities on the
 * central PcMusicGameplayManager in response to events (Prepare, Hit, Miss).
 */
UCLASS(BlueprintType)
class PROJECT_CIRCLE_API UMusicActionInstance : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * Initializes the instance with all the data it needs to manage its lifecycle.
	 * This grants the note's abilities to the central manager's ASC and subscribes to the music tick.
	 * @param InNoteData The specific data for this note (timing, etc.) from the master DataTable.
	 * @param InOwnerManager The gameplay manager actor, which owns the central Ability System Component.
	 * @param InDirector The music director subsystem, used to subscribe to the music tick for time updates.
	 */
	void Initialize(const FPcMusicGameplayNotes& InNoteData, APcMusicGameplayManager* InOwnerManager, UPcMusicDirectorSubsystem* InDirector);

	/**
	 * Called by an external source (e.g., a visual proxy actor) to report a successful player interaction with this note.
	 */
	void ReportHitSuccess();

	/**
	 * Gets the core gameplay data associated with this note instance.
	 * @return A const reference to the note data.
	 */
	const FPcMusicGameplayNotes& GetNoteData() const { return NoteData; }
	
	/**
	 * Gets the central Ability System Component from the Gameplay Manager.
	 * This is the ASC that all abilities for all notes are run on.
	 */
	UAbilitySystemComponent* GetAbilitySystemComponent() const;

private:
	/**
	 * The main update function, subscribed to the Director's OnMusicTick delegate.
	 * This function is responsible for detecting when a note has been missed.
	 * @param CurrentTimeMs The current song time broadcast by the director.
	 */
	void HandleMusicTick(float CurrentTimeMs);

	/**
	 * Looks up and triggers the abilities associated with a given event phase (e.g., OnHit).
	 * @param TargetASC The Ability System Component on which to activate the abilities.
	 * @param Phase The enum representing the event phase.
	 */
	void TryActivateAbilityForPhase(UAbilitySystemComponent* TargetASC, EMusicEventPhase Phase);

	/**
	 * Handles all cleanup for this instance. It revokes the abilities it granted to the manager's
	 * ASC, unsubscribes from delegates, and marks itself for garbage collection.
	 */
	void Deactivate();

	// --- Core Data & State ---

	/** A copy of the specific data for this note, taken from the master DataTable during initialization. */
	UPROPERTY(VisibleInstanceOnly, Category = "State")
	FPcMusicGameplayNotes NoteData;
	
	/** The current state of this note instance's lifecycle. */
	UPROPERTY(VisibleInstanceOnly, Category = "State")
	EMusicNoteState CurrentState = EMusicNoteState::None;

	// --- References & Handles ---

	/** A weak pointer to the central gameplay manager, which owns the Ability System Component. */
	UPROPERTY()
	TWeakObjectPtr<APcMusicGameplayManager> OwnerManager;

	/** A weak pointer to the music director, used to subscribe/unsubscribe to its time updates. */
	UPROPERTY()
	TWeakObjectPtr<UPcMusicDirectorSubsystem> OwnerDirector;

	/**
	 * Stores the handles to the abilities that this instance granted to the manager's ASC.
	 * This is crucial for cleaning up and revoking the correct abilities upon deactivation.
	 */
	UPROPERTY()
	TArray<FGameplayAbilitySpecHandle> GrantedAbilityHandles;
};