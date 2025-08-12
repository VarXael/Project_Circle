// Fill out your copyright notice in the Description page of Project Settings.

#include "MusicAction.h"
#include "Project_Circle/MusicSystem/MusicGameplaySystem/PcMusicDirectorSubsystem.h"

UWorld* UMusicAction::GetWorld() const
{
	if (AActor* OuterActor = GetTypedOuter<AActor>())
	{
		return OuterActor->GetWorld();
	}
	return nullptr;
}

void UMusicAction::Execute(UMusicActionInstance* NoteContext)
{
	check(NoteContext != nullptr);
	if (bShouldPerformMusicTick)
	{
		UWorld* World = GetWorld();
		if (World)
		{
			// Find the director subsystem.
			DirectorSubsystem = World->GetSubsystem<UPcMusicDirectorSubsystem>();
			if (DirectorSubsystem.IsValid())
			{
				// Subscribe our K2_OnMusicTick event to be called by the Director's C++ broadcast.
				DirectorSubsystem->OnMusicTick.AddDynamic(this, &UMusicAction::MusicTick);
			}
		}
	}
	K2_OnExecuted(NoteContext);
}

void UMusicAction::MusicTick(float CurrentTimeMs)
{
	K2_OnMusicTick(CurrentTimeMs);
}

void UMusicAction::EndMusicAction()
{
	// Unsubscribe from the Director's tick to stop receiving updates.
	// This is crucial for performance and to allow this object to be garbage collected.
	if (DirectorSubsystem.IsValid())
	{
		DirectorSubsystem->OnMusicTick.RemoveDynamic(this, &UMusicAction::K2_OnMusicTick);
	}
	
	// Mark ourself as pending kill. If anything is holding a strong UPROPERTY reference,
	// it will be nullified, preventing crashes.
	MarkAsGarbage();
}


