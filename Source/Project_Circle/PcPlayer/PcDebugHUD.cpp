#include "PcDebugHUD.h"
#include "PcPlayerCharacter.h"
#include "FlowSystem/PcFlowMechanicComponent.h" // Corrected Path
#include "Engine/Canvas.h"
#include "Engine/World.h" 

void APcDebugHUD::AddStyleMessage(FString Message, EStyleEventType Type)
{
	FStyleLogMessage NewMsg;
	NewMsg.Text = Message;
	NewMsg.Type = Type;
	NewMsg.TimeRemaining = MessageLifetime;
	
	// Add to start (Newest messages at index 0)
	MessageLog.Insert(NewMsg, 0);
	
	// Cap log size so it doesn't span the whole screen
	if (MessageLog.Num() > 10) MessageLog.Pop();
}

void APcDebugHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas) return;

	APawn* OwningPawn = GetOwningPawn();
	APcPlayerCharacter* Player = Cast<APcPlayerCharacter>(OwningPawn);
	if (!Player || !Player->FlowComp) return;

	// 1. Draw Center Vectors (Physics Tuning)
	DrawPhysicsDebug(Player);

	// 2. Draw Right-Side Dashboard (Bar + Stats + Log)
	DrawFlowDashboard(Player);
}

void APcDebugHUD::DrawPhysicsDebug(APcPlayerCharacter* Player)
{
	float CX = Canvas->ClipX * 0.5f;
	float CY = Canvas->ClipY * 0.5f;

	// Yellow = Input (Look), Cyan = Velocity (Slide)
	FVector InputDir = Player->DebugLastInputDir;
	FVector VelDir = Player->DebugLastVelocityDir;

	// Scale lines for visibility
	float LineLen = 80.0f;

	// Draw Input (Aim)
	DrawLine(CX, CY, CX + (InputDir.Y * LineLen), CY - (InputDir.X * LineLen), FLinearColor::Yellow, 2.0f);
	
	// Draw Velocity (Movement) - FColor::Cyan converts implicitly to FLinearColor
	DrawLine(CX, CY, CX + (VelDir.Y * LineLen), CY - (VelDir.X * LineLen), FColor::Cyan, 2.0f);

	// Slip Angle Text
	FString AngleText = FString::Printf(TEXT("%.0f"), Player->DebugSlipAngle);
	FLinearColor AngleColor = FLinearColor::White;
	
	// Color Code the Angle
	if (Player->DebugSlipAngle > 20 && Player->DebugSlipAngle < 90) AngleColor = FLinearColor::Green; // Sweet Spot
	else if (Player->DebugSlipAngle > 90) AngleColor = FLinearColor::Red; // Spinout
	else if (Player->DebugSlipAngle > 0) AngleColor = FLinearColor(0.5f, 0.5f, 0.5f); // Boring straight

	DrawText(AngleText, AngleColor, CX + 10, CY - 40, nullptr, 1.2f);
}

void APcDebugHUD::DrawFlowDashboard(APcPlayerCharacter* Player)
{
	UPcFlowMechanicComponent* Flow = Player->FlowComp;
	if (!Flow) return;

	float RightEdge = Canvas->ClipX - 50.0f;
	float BarAnchorY = Canvas->ClipY * 0.75f; // 75% down the screen
	float BarLeft = RightEdge - BarWidth;

	// --- 1. THE ACTION LOG (Above the Bar) ---
	DrawActionLog(BarAnchorY - 10.0f, RightEdge);

	// --- 2. THE BAR BACKGROUND ---
	FLinearColor BgColor = FLinearColor(0.1f, 0.1f, 0.1f, 0.8f);
	DrawRect(BgColor, BarLeft, BarAnchorY, BarWidth, BarHeight);

	// --- 3. THE BAR FILL ---
	float FillPercent = FMath::Clamp(Flow->FlowPercent / 100.0f, 0.0f, 1.0f);
	// Invert the fill direction so it fills Right-to-Left (decays Left-to-Right)
	// Or standard Left-to-Right. Standard is simpler. 
	// To Decay Right-to-Left (visualize losing ground):
	float FillWidth = BarWidth * FillPercent;

	FLinearColor FillColor = FLinearColor::Yellow; // Default

	// State-Based Colors
	switch (Flow->CurrentState)
	{
	case EFlowState::Stable:   FillColor = FLinearColor::Yellow; break;
	case EFlowState::Draining: FillColor = FLinearColor(1.0f, 0.5f, 0.0f); break; // Orange
	case EFlowState::Charging: FillColor = FLinearColor::Green; break;
	case EFlowState::Frozen:   FillColor = FColor::Cyan; break; // Use FColor::Cyan
	case EFlowState::Panic:    
		// Flash Red/White
		float Pulse = FMath::Sin(GetWorld()->GetTimeSeconds() * 20.0f); 
		FillColor = (Pulse > 0) ? FLinearColor::Red : FLinearColor::White;
		break;
	}

	if (FillWidth > 1.0f)
	{
		DrawRect(FillColor, BarLeft, BarAnchorY, FillWidth, BarHeight);
	}

	// --- 4. TEXT INFO (Under the Bar) ---
	float TextY = BarAnchorY + BarHeight + 8.0f;
	
	// Left side: TIER
	FString TierStr = FString::Printf(TEXT("TIER %d"), Flow->CurrentTier);
	DrawText(TierStr, FLinearColor::White, BarLeft, TextY, nullptr, 1.5f);

	// Right side: SPEED / CAP
	// We need to calculate the Cap based on Tier to show context
	float SpeedCap = 800.0f + (Flow->CurrentTier * 600.0f); // Hardcoded visual ref based on Player.cpp
	FString SpdStr = FString::Printf(TEXT("%.0f / %.0f"), Player->GetCurrentSpeed(), SpeedCap);
	
	// Measure string to right-align it
	float SpdLen, SpdH;
	Canvas->StrLen(GEngine->GetSmallFont(), SpdStr, SpdLen, SpdH); // FIX: Use Canvas directly
	DrawText(SpdStr, FLinearColor::White, RightEdge - (SpdLen * 1.5f), TextY, nullptr, 1.5f);
}

void APcDebugHUD::DrawActionLog(float BottomAnchorY, float RightAnchorX)
{
	float CurrentY = BottomAnchorY;
	float DeltaTime = GetWorld()->GetDeltaSeconds();

	// Iterate backwards (Draw newest at bottom, pushing older ones up)
	for (int32 i = 0; i < MessageLog.Num(); ++i)
	{
		FStyleLogMessage& Msg = MessageLog[i];
		
		// Update Timer
		Msg.TimeRemaining -= DeltaTime;
		if (Msg.TimeRemaining <= 0.0f)
		{
			MessageLog.RemoveAt(i);
			i--; // Adjust index
			continue;
		}

		// Calculate Alpha (Fade out last 0.5s)
		float Alpha = FMath::Clamp(Msg.TimeRemaining / 0.5f, 0.0f, 1.0f);
		
		FLinearColor TextColor = FLinearColor::White;
		FString Prefix = "";
		
		if (Msg.Type == EStyleEventType::Good) { TextColor = FLinearColor::Green; Prefix = "+ "; }
		else if (Msg.Type == EStyleEventType::Bad) { TextColor = FLinearColor::Red; Prefix = "- "; }
		
		TextColor.A = Alpha;

		FString FullText = Prefix + Msg.Text;
		
		// Align Right
		float XL, YL;
		Canvas->StrLen(GEngine->GetSmallFont(), FullText, XL, YL); // FIX: Use Canvas directly
		// Scale 1.2
		XL *= 1.2f; YL *= 1.2f;

		DrawText(FullText, TextColor, RightAnchorX - XL, CurrentY - YL, nullptr, 1.2f);
		
		CurrentY -= (YL + 5.0f); // Move Up
	}
}