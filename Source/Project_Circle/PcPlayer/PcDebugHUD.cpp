#include "PcDebugHUD.h"
#include "PcPlayerCharacter.h"
#include "Engine/Canvas.h"

void APcDebugHUD::DrawHUD()
{
	Super::DrawHUD();

	// --- 1. DRAW EXISTING DEBUG TEXT ---
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

	// --- 2. DRAW CROSSHAIR (New) ---
	if (Canvas)
	{
		// Find the exact center of the screen
		const float CenterX = Canvas->ClipX * 0.5f;
		const float CenterY = Canvas->ClipY * 0.5f;

		const float CrosshairSize = 10.0f; // Length of the lines
		const float Thickness = 2.0f;      // Thickness of the lines
		FLinearColor CrosshairColor = FLinearColor::Red; // High visibility

		// Draw Horizontal Line (-)
		DrawLine(
			CenterX - CrosshairSize, CenterY, 
			CenterX + CrosshairSize, CenterY, 
			CrosshairColor, Thickness
		);

		// Draw Vertical Line (|)
		DrawLine(
			CenterX, CenterY - CrosshairSize, 
			CenterX, CenterY + CrosshairSize, 
			CrosshairColor, Thickness
		);
	}
}