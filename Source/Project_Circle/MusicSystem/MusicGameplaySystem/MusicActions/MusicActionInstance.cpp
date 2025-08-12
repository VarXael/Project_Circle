// Fill out your copyright notice in the Description page of Project Settings.

#include "MusicActionInstance.h"
#include "MusicActionSet.h"
#include "MusicAction.h"
#include "Project_Circle/MusicSystem/MusicGameplaySystem/PcMusicDirectorSubsystem.h"
#include "Project_Circle/MusicSystem/MusicGameplaySystem/PcMusicGameplayManager.h"



void UMusicActionInstance::Initialize(const FPcMusicGameplayNotes& InNoteData, APcMusicGameplayManager* InOwnerManager,UPcMusicDirectorSubsystem* InDirector)
{
	check(IsValid(InOwnerManager));
	check(InNoteData.MusicGameplayEventDefinition != nullptr);
	check(IsValid(InDirector));

	NoteData = InNoteData;
	OwnerManager = InOwnerManager;

	DirectorSubsystem = InDirector;
	DirectorSubsystem->OnMusicTick.AddDynamic(this, &UMusicActionInstance::MusicTick);

	K2_OnBeginInitialization();
}

void UMusicActionInstance::MusicTick(float CurrentTimeMs)
{
	// The C++ one-shot event check
	if (!bHasFiredExecution && CurrentTimeMs >= NoteData.StartTimeMS)
	{
		bHasFiredExecution = true;
		K2_OnExecution();
		K2_OnExecutionCompleted_Implementation(); // Default behavior is to deactivate after execution
	}

	// The optional Blueprint tick
	if (bShouldPerformMusicTick)
	{
		K2_OnMusicTick(CurrentTimeMs);
	}
}


void UMusicActionInstance::K2_OnExecutionCompleted_Implementation()
{
	Deactivate();
}

void UMusicActionInstance::Deactivate()
{
	if (DirectorSubsystem.IsValid())
	{
		DirectorSubsystem->OnMusicTick.RemoveDynamic(this, &UMusicActionInstance::MusicTick);
	}
	MarkAsGarbage();
}

void UMusicActionInstance::ExecuteMusicActionByTag(FGameplayTag ActionTag)
{
	// Ensure we have a valid manager to own the new action and a valid ActionSet to read from.
	APcMusicGameplayManager* Manager = OwnerManager.Get();
	const UMusicActionSet* ActionSet = NoteData.MusicGameplayEventDefinition;
	if (!Manager || !ActionSet || !ActionTag.IsValid())
	{
		return;
	}

	// Find the action class associated with the provided tag in our library.
	const FTaggedMusicAction* FoundAction = ActionSet->ActionLibrary.FindByPredicate(
		[&](const FTaggedMusicAction& Action)
		{
			return Action.ActionTag == ActionTag;
		});

	if (FoundAction && FoundAction->ActionClass)
	{
		// Create a new instance of the UMusicAction, owned by the stable Manager actor.
		UMusicAction* ActionObject = NewObject<UMusicAction>(Manager, FoundAction->ActionClass);

		// Run the action's Blueprint logic.
		ActionObject->Execute(this);
	}
}

UWorld* UMusicActionInstance::GetWorld() const
{
	// IsValid() checks if the pointer is not null and the object is not pending kill.
	return IsValid(OwnerManager) ? OwnerManager->GetWorld() : nullptr;
}

void UMusicActionInstance::BeginDestroy()
{
	UObject::BeginDestroy();
}
