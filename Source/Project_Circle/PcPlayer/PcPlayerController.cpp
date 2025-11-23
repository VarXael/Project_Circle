#include "PcPlayerController.h"
#include "EnhancedInputSubsystems.h"

void APcPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		// Enable the Input Mapping Context
		if (DefaultMappingContext)
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}

	bShowMouseCursor = false;
	SetInputMode(FInputModeGameOnly());
}