#include "PcQDebugHUD.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Character.h"
#include "Engine/Canvas.h"

void APcQDebugHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas) return;

	DrawDotCrosshair();

	APlayerController* PC = GetOwningPlayerController();
	if (!PC) return;

	ACharacter* Char = Cast<ACharacter>(PC->GetPawn());
	if (!Char) return;

	UPcQPlayerMovementComponent* MC = Cast<UPcQPlayerMovementComponent>(Char->GetCharacterMovement());
	if (!MC) return;

	DrawBhopDebug(MC);
}

void APcQDebugHUD::DrawDotCrosshair()
{
	const float CX = Canvas->SizeX * 0.5f;
	const float CY = Canvas->SizeY * 0.5f;

	DrawCircleHUD(CX, CY, DotRingRadius + 1.f, FLinearColor(0.f, 0.f, 0.f, 0.45f), DotRingThickness + 1.f, 32);
	DrawCircleHUD(CX, CY, DotRingRadius, CrosshairColor, DotRingThickness, 32);
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.5f), CX - DotSize - 0.5f, CY - DotSize - 0.5f, (DotSize + 0.5f) * 2.f, (DotSize + 0.5f) * 2.f);
	DrawRect(CrosshairColor, CX - DotSize, CY - DotSize, DotSize * 2.f, DotSize * 2.f);
}

void APcQDebugHUD::DrawCircleHUD(float CX, float CY, float Radius, FLinearColor Color, float Thickness, int32 Segments)
{
	if (Segments <= 0) return;
	const float Step = 2.f * PI / Segments;
	for (int32 i = 0; i < Segments; ++i)
	{
		const float A0 = i * Step, A1 = (i + 1) * Step;
		DrawLine(CX + Radius * FMath::Cos(A0), CY + Radius * FMath::Sin(A0),
				 CX + Radius * FMath::Cos(A1), CY + Radius * FMath::Sin(A1),
				 Color, Thickness);
	}
}

void APcQDebugHUD::DrawBhopDebug(UPcQPlayerMovementComponent* MC)
{
	const float PanelX = 30.f;
	float       PanelY = 30.f;
	const float LineH  = 22.f;

	auto Row = [&](const FString& Label, const FString& Value, FLinearColor Color = FLinearColor::White)
	{
		DrawText(Label + TEXT("  ") + Value, Color, PanelX, PanelY, GEngine->GetSmallFont(), 1.f);
		PanelY += LineH;
	};

	// --- STATE ---
	EBhopState State = MC->GetBhopState();
	FString StateStr;
	switch (State)
	{
	case EBhopState::Idle:     StateStr = TEXT("IDLE");        break;
	case EBhopState::Charging: StateStr = TEXT("CHARGING");    break;
	case EBhopState::Active:   StateStr = TEXT("BEAT-SYNCED"); break;
	}
	Row(TEXT("STATE:"), StateStr, GetStateColor(State));

	// --- CHARGE BAR ---
	if (State == EBhopState::Charging)
	{
		const float Alpha = MC->GetChargeAlpha();
		const float BW = 160.f, BH = 10.f;
		DrawRect(FLinearColor(0.1f, 0.1f, 0.1f, 0.8f), PanelX, PanelY, BW, BH);
		DrawRect(FLinearColor::LerpUsingHSV(FLinearColor::Yellow, FLinearColor::Green, Alpha), PanelX, PanelY, BW * Alpha, BH);
		DrawRect(FLinearColor::White, PanelX,      PanelY,      BW,  1.f);
		DrawRect(FLinearColor::White, PanelX,      PanelY + BH, BW,  1.f);
		DrawRect(FLinearColor::White, PanelX,      PanelY,      1.f, BH);
		DrawRect(FLinearColor::White, PanelX + BW, PanelY,      1.f, BH + 1.f);
		PanelY += BH + 6.f;
	}

	// --- PRESET ---
	const int32 Sub = MC->GetCurrentSubdivision();
	FString PresetName;
	FLinearColor PresetColor;
	if (Sub <= 1)      { PresetName = TEXT("SLOW");      PresetColor = FLinearColor(0.4f, 0.8f, 1.f); }
	else if (Sub <= 2) { PresetName = TEXT("NORMAL");    PresetColor = FLinearColor::Green; }
	else if (Sub <= 4) { PresetName = TEXT("FAST");      PresetColor = FLinearColor::Yellow; }
	else               { PresetName = TEXT("VERY FAST"); PresetColor = FLinearColor(1.f, 0.4f, 0.f); }

	Row(TEXT("PRESET:"),     FString::Printf(TEXT("%s  (sub %d)"), *PresetName, Sub), PresetColor);
	Row(TEXT("GAME BPM:"),   FString::Printf(TEXT("%.1f"), MC->GetCurrentBPM()));
	Row(TEXT("JUMP VEL:"),   FString::Printf(TEXT("%.0f u/s"), MC->JumpZVelocity));
	Row(TEXT("COYOTE:"),     MC->HasQueuedJump() ? TEXT("ACTIVE") : TEXT("--"),
		MC->HasQueuedJump() ? FLinearColor::Yellow : FLinearColor(0.5f, 0.5f, 0.5f));

	// --- SPEED ---
	const float HSpeed = MC->GetHorizontalSpeed();
	FLinearColor SpeedCol = MC->IsInBhopChain() ? FLinearColor(1.f, 0.45f, 0.f) : FLinearColor::White;
	Row(TEXT("SPEED:"), FString::Printf(TEXT("%.0f u/s"), HSpeed), SpeedCol);

	{
		const float MaxDisplay = MC->MaxWalkSpeed * 3.f;
		const float BW = 160.f, BH = 6.f;
		const float Fill = FMath::Clamp(HSpeed / MaxDisplay, 0.f, 1.f);
		DrawRect(FLinearColor(0.1f, 0.1f, 0.1f, 0.8f), PanelX, PanelY, BW, BH);
		DrawRect(SpeedCol, PanelX, PanelY, BW * Fill, BH);
		DrawRect(FLinearColor::White, PanelX,      PanelY,      BW,  1.f);
		DrawRect(FLinearColor::White, PanelX,      PanelY + BH, BW,  1.f);
		DrawRect(FLinearColor::White, PanelX,      PanelY,      1.f, BH);
		DrawRect(FLinearColor::White, PanelX + BW, PanelY,      1.f, BH + 1.f);
		const float WalkMark = FMath::Clamp(MC->MaxWalkSpeed / MaxDisplay, 0.f, 1.f);
		DrawRect(FLinearColor::White, PanelX + BW * WalkMark - 1.f, PanelY - 2.f, 2.f, BH + 4.f);
		PanelY += BH + 8.f;
	}

	Row(TEXT("V SPEED:"), FString::Printf(TEXT("%.0f u/s"), MC->Velocity.Z),
		MC->Velocity.Z < -10.f ? FLinearColor(0.6f, 0.6f, 1.f) : FLinearColor::White);

	Row(TEXT("GROUNDED:"), MC->IsMovingOnGround() ? TEXT("YES") : TEXT("NO"),
		MC->IsMovingOnGround() ? FLinearColor::Green : FLinearColor(0.6f, 0.6f, 1.f));

	if (MC->IsInBhopChain())
	{
		const float Pulse = FMath::Abs(FMath::Sin(GetWorld()->GetTimeSeconds() * 6.f));
		DrawText(TEXT("BHOP CHAIN"),
			FLinearColor::LerpUsingHSV(FLinearColor(1.f, 0.3f, 0.f), FLinearColor::Yellow, Pulse),
			Canvas->SizeX - 160.f, 30.f, GEngine->GetSmallFont(), 1.4f);
	}
}

FLinearColor APcQDebugHUD::GetStateColor(EBhopState State) const
{
	switch (State)
	{
	case EBhopState::Idle:     return FLinearColor(0.5f, 0.5f, 0.5f);
	case EBhopState::Charging: return FLinearColor::Yellow;
	case EBhopState::Active:   return FLinearColor::Green;
	default:                   return FLinearColor::White;
	}
}