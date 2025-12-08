// ==========================================
// FILE: PcDebugHUD.cpp
// PATH: E:\GameDev\Unreal Engine Projects\Project_Circle\Source\Project_Circle\PcPlayer\PcDebugHUD.cpp
// ==========================================
#include "PcDebugHUD.h"
#include "PcPlayerCharacter.h"
#include "FlowSystem/PcFlowMechanicComponent.h"
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
	
	if (MessageLog.Num() > 10) MessageLog.Pop();
}

void APcDebugHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas) return;

	APawn* OwningPawn = GetOwningPawn();
	APcPlayerCharacter* Player = Cast<APcPlayerCharacter>(OwningPawn);
	if (!Player || !Player->FlowComp) return;

	DrawPhysicsDebug(Player);
	DrawFlowDashboard(Player);
}

void APcDebugHUD::DrawPhysicsDebug(APcPlayerCharacter* Player)
{
	float CX = Canvas->ClipX * 0.5f;
	float CY = Canvas->ClipY * 0.5f;

	FVector InputDir = Player->DebugLastInputDir;
	FVector VelDir = Player->DebugLastVelocityDir;
	float LineLen = 80.0f;

	// Draw Input (Yellow)
	DrawLine(CX, CY, CX + (InputDir.Y * LineLen), CY - (InputDir.X * LineLen), FLinearColor::Yellow, 2.0f);
	
	// Draw Velocity (Cyan)
	DrawLine(CX, CY, CX + (VelDir.Y * LineLen), CY - (VelDir.X * LineLen), FColor::Cyan, 2.0f);

	// Angle Text
	FString AngleText = FString::Printf(TEXT("%.0f"), Player->DebugSlipAngle);
	FLinearColor AngleColor = (Player->DebugSlipAngle > 20 && Player->DebugSlipAngle < 90) ? FLinearColor::Green : FLinearColor::White;
	DrawText(AngleText, AngleColor, CX + 10, CY - 40, nullptr, 1.2f);
}

void APcDebugHUD::DrawFlowDashboard(APcPlayerCharacter* Player)
{
	UPcFlowMechanicComponent* Flow = Player->FlowComp;
	if (!Flow) return;

	// CONFIG
	float RightEdge = Canvas->ClipX - 50.0f;
	float BottomAnchorY = Canvas->ClipY * 0.85f; 
	float BarW = 300.0f;
	float BarH = 20.0f;
	float BarLeft = RightEdge - BarW;
	float Gap = 10.0f;

	// --- 1. FLOW BAR (Bottom) ---
	float FlowY = BottomAnchorY;
	
	// Bg
	DrawRect(FLinearColor(0.1f, 0.1f, 0.1f, 0.8f), BarLeft, FlowY, BarW, BarH);
	
	// Fill
	float FlowPct = FMath::Clamp(Flow->FlowPercent / 100.0f, 0.0f, 1.0f);
	FLinearColor FlowColor = FLinearColor::Yellow; 
	if (Flow->CurrentState == EFlowState::Charging) FlowColor = FLinearColor::Green;
	else if (Flow->CurrentState == EFlowState::Frozen) FlowColor = FColor::Cyan;
	else if (Flow->CurrentState == EFlowState::Panic) FlowColor = FLinearColor::Red;
	
	DrawRect(FlowColor, BarLeft, FlowY, BarW * FlowPct, BarH);

	// Stats Text (Under Flow)
	float TextY = FlowY + BarH + 5.0f;
	DrawText(FString::Printf(TEXT("TIER %d"), Flow->CurrentTier), FLinearColor::White, BarLeft, TextY, nullptr, 1.2f);
	
	FString SpdStr = FString::Printf(TEXT("%.0f"), Player->GetCurrentSpeed());
	float SpdW, SpdH; Canvas->StrLen(GEngine->GetSmallFont(), SpdStr, SpdW, SpdH);
	DrawText(SpdStr, FLinearColor::White, RightEdge - SpdW * 1.2f, TextY, nullptr, 1.2f);


	// --- 2. STAMINA BAR (Middle) ---
	float StaminaY = FlowY - BarH - Gap;
	
	// Bg
	DrawRect(FLinearColor(0.1f, 0.1f, 0.1f, 0.8f), BarLeft, StaminaY, BarW, BarH);

	// Fill
	float StaminaPct = Player->GetDriftStamina() / 100.0f;
	FLinearColor StaminaColor = FColor::Cyan;
	if (StaminaPct <= 0.0f) StaminaColor = FLinearColor::Red;       // Burnout
	else if (StaminaPct < 0.3f) StaminaColor = FColor::Orange;      // Low warning
	
	DrawRect(StaminaColor, BarLeft, StaminaY, BarW * StaminaPct, BarH);
	DrawText(TEXT("STAMINA"), FLinearColor::White, BarLeft - 70.0f, StaminaY + 2.0f, nullptr, 1.0f);


	// --- 3. LIVE SCORE (Floating above Stamina) ---
	// Only show if we are actively accumulating drift points
	/*
	if (Player->DriftScoreAccumulator > 10.0f)
	{
		FString LiveScore = FString::Printf(TEXT("+ %.0f"), Player->DriftScoreAccumulator);
		DrawText(LiveScore, FLinearColor::Yellow, BarLeft, StaminaY - 25.0f, nullptr, 1.5f);
	}
	*/

	// --- 4. ACTION LOG (Top Stack) ---
	// Anchored above the Stamina Bar
	DrawActionLog(StaminaY - 20.0f, RightEdge);
}

void APcDebugHUD::DrawActionLog(float BottomAnchorY, float RightAnchorX)
{
	float CurrentY = BottomAnchorY;
	float DeltaTime = GetWorld()->GetDeltaSeconds();

	for (int32 i = 0; i < MessageLog.Num(); ++i)
	{
		FStyleLogMessage& Msg = MessageLog[i];
		
		// Update Timer
		Msg.TimeRemaining -= DeltaTime;
		if (Msg.TimeRemaining <= 0.0f)
		{
			MessageLog.RemoveAt(i);
			i--; // Adjust index after removal
			continue;
		}

		// Fade out
		float Alpha = FMath::Clamp(Msg.TimeRemaining / 0.5f, 0.0f, 1.0f);
		
		FLinearColor TextColor = FLinearColor::White;
		FString Prefix = "";
		
		if (Msg.Type == EStyleEventType::Good) { TextColor = FLinearColor::Green; Prefix = "+ "; }
		else if (Msg.Type == EStyleEventType::Bad) { TextColor = FLinearColor::Red; Prefix = "- "; }
		else { TextColor = FLinearColor(0.8f, 0.8f, 0.8f); } // Neutral Grey
		
		TextColor.A = Alpha;

		FString FullText = Prefix + Msg.Text;
		
		// Align Right
		float XL, YL;
		Canvas->StrLen(GEngine->GetSmallFont(), FullText, XL, YL);
		XL *= 1.2f; YL *= 1.2f; // Scale

		DrawText(FullText, TextColor, RightAnchorX - XL, CurrentY - YL, nullptr, 1.2f);
		
		// Move cursor up for next message
		CurrentY -= (YL + 5.0f); 
	}
}