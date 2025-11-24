#include "PcDebugHUD.h"
#include "PcPlayerCharacter.h"
#include "Engine/Canvas.h"

void APcDebugHUD::DrawHUD()
{
	Super::DrawHUD();

	if (APcPlayerCharacter* Player = Cast<APcPlayerCharacter>(GetOwningPawn()))
	{
		FString DebugText = Player->GetDebugInfo();
		
		// Draw Text at (50, 250) with Yellow Color
		DrawText(DebugText, FLinearColor::Yellow, 50.0f, 250.0f, nullptr, 1.5f);
	}
}