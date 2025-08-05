// Fill out your copyright notice in the Description page of Project Settings.

#include "MusicActionInstance.h"
#include "AbilitySystemComponent.h"
#include "MusicActionSet.h"
#include "Abilities/GameplayAbility.h"
#include "Project_Circle/MusicSystem/MusicGameplaySystem/PcMusicDirectorSubsystem.h"
#include "Project_Circle/MusicSystem/MusicGameplaySystem/PcMusicGameplayManager.h"

UAbilitySystemComponent* UMusicActionInstance::GetAbilitySystemComponent() const
{
	if (OwnerManager.IsValid())
	{
		return OwnerManager->GetAbilitySystemComponent();
	}
	return nullptr;
}

void UMusicActionInstance::Initialize(const FPcMusicGameplayNotes& InNoteData, APcMusicGameplayManager* InOwnerManager, UPcMusicDirectorSubsystem* InDirector)
{
	NoteData = InNoteData;
	OwnerManager = InOwnerManager;
	OwnerDirector = InDirector;

	if (!OwnerManager.IsValid() || !OwnerDirector.IsValid() || !NoteData.MusicGameplayEventDefinition)
	{
		UE_LOG(LogTemp, Error, TEXT("UMusicActionInstance::Initialize failed: A required dependency was null. Deactivating."));
		Deactivate();
		return;
	}

	UAbilitySystemComponent* ManagerASC = GetAbilitySystemComponent();
	if (!ManagerASC)
	{
		UE_LOG(LogTemp, Error, TEXT("UMusicActionInstance::Initialize failed: OwnerManager has no AbilitySystemComponent. Deactivating."));
		Deactivate();
		return;
	}

	const UMusicActionSet* ActionSet = NoteData.MusicGameplayEventDefinition;
	for (const auto& Kvp : ActionSet->ActionMap)
	{
		for (const TSubclassOf<UGameplayAbility>& AbilityClass : Kvp.Value.Abilities)
		{
			if (AbilityClass)
			{
				FGameplayAbilitySpec Spec(AbilityClass);
				// Set this UObject as the SourceObject. This allows the ability to know which note instance triggered it.
				Spec.SourceObject = this;
				const FGameplayAbilitySpecHandle NewHandle = ManagerASC->GiveAbility(Spec);
				GrantedAbilityHandles.Add(NewHandle);
			}
		}
	}
	
	// 4. Set the initial state and subscribe to the director's tick delegate to begin listening for time updates.
	CurrentState = EMusicNoteState::Approaching;
	OwnerDirector->OnMusicTick.AddUObject(this, &UMusicActionInstance::HandleMusicTick);

	// 5. Fire the initial "OnPrepare" abilities to signal the note's appearance.
	TryActivateAbilityForPhase(ManagerASC, EMusicEventPhase::OnPrepare);
}

void UMusicActionInstance::ReportHitSuccess()
{
	// A note can only be successfully hit if it's currently in the "Approaching" state.
	if (CurrentState != EMusicNoteState::Approaching)
	{
		return;
	}
	
	if (UAbilitySystemComponent* ManagerASC = GetAbilitySystemComponent())
	{
		CurrentState = EMusicNoteState::Hit;
		TryActivateAbilityForPhase(ManagerASC, EMusicEventPhase::OnHit);
	}
	
	// The note's lifecycle is complete.
	Deactivate();
}

void UMusicActionInstance::HandleMusicTick(float CurrentTimeMs)
{
	// Only process the tick if the note is in the "Approaching" state.
	if (CurrentState != EMusicNoteState::Approaching)
	{
		return;
	}

	// If the current song time has passed the note's designated hit time, it has been missed.
	if (CurrentTimeMs >= NoteData.StartTimeMS)
	{
		if (UAbilitySystemComponent* ManagerASC = GetAbilitySystemComponent())
		{
			CurrentState = EMusicNoteState::Missed;
			TryActivateAbilityForPhase(ManagerASC, EMusicEventPhase::OnMiss);
		}
		
		// The note's lifecycle is complete.
		Deactivate();
	}
}

void UMusicActionInstance::TryActivateAbilityForPhase(UAbilitySystemComponent* TargetASC, EMusicEventPhase Phase)
{
	// Ensure we have a valid ASC and ActionSet to read from.
	if (!TargetASC || !NoteData.MusicGameplayEventDefinition)
	{
		return;
	}

	const UMusicActionSet* ActionSet = NoteData.MusicGameplayEventDefinition;
	
	// Find the list of abilities associated with the specified event phase.
	if (const FMusicAbilityArray* FoundArray = ActionSet->ActionMap.Find(Phase))
	{
		// Execute every ability defined in that list.
		for (const TSubclassOf<UGameplayAbility>& AbilityClass : FoundArray->Abilities)
		{
			if (AbilityClass)
			{
				TargetASC->TryActivateAbilityByClass(AbilityClass);
			}
		}
	}
}

void UMusicActionInstance::Deactivate()
{
	// 1. Revoke the abilities this instance granted from the central manager's ASC.
	if (UAbilitySystemComponent* ManagerASC = GetAbilitySystemComponent())
	{
		for (const FGameplayAbilitySpecHandle& Handle : GrantedAbilityHandles)
		{
			// This removes the ability spec, ensuring it can't be activated again.
			ManagerASC->ClearAbility(Handle);
		}
		GrantedAbilityHandles.Empty();
	}

	// 2. Unsubscribe from the Director's tick to prevent this object from receiving updates after deactivation.
	if (OwnerDirector.IsValid())
	{
		OwnerDirector->OnMusicTick.RemoveAll(this);
	}

	// 3. Mark this UObject for garbage collection, as its job is done.
	MarkAsGarbage();
}