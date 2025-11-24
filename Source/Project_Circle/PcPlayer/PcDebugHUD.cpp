#include "PcDebugHUD.h"
#include "PcPlayerCharacter.h"
#include "Engine/Canvas.h"

void APcDebugHUD::DrawHUD()
{
	Super::DrawHUD();

	if (APcPlayerCharacter* Player = Cast<APcPlayerCharacter>(GetOwningPawn()))
	{
		FString DebugText = Player->GetDebugInfo();
		bool bCanJump = Player->IsInRhythmWindow();

		FLinearColor TextColor = bCanJump ? FLinearColor::Green : FLinearColor::White;
		float TextScale = 1.5f;

		// Draw Stats
		DrawText(DebugText, TextColor, 50.0f, 250.0f, nullptr, TextScale);

		// Draw Visual Prompt
		if (bCanJump)
		{
			DrawText(TEXT(">>> PRESS SPACE <<<"), FLinearColor::Green, 300.0f, 250.0f, nullptr, 3.0f);
		}
	}
}