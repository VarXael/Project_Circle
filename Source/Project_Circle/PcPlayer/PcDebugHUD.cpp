// ==========================================
// FILE: PcDebugHUD.cpp
// PATH: Source/Project_Circle/PcPlayer/PcDebugHUD.cpp
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

	float RightEdge = Canvas->ClipX - 50.0f;
	float BottomAnchorY = Canvas->ClipY * 0.85f; 

	// =========================================================
	// 1. CHARGE BARS (Bottom Right)
	// =========================================================
	float BarW = 80.0f; 
	float Gap = 10.0f;
	float BarH = 20.0f;
	float TotalW = (BarW * 3) + (Gap * 2);
	float StartX = RightEdge - TotalW;

	// Draw Background Box
	DrawRect(FLinearColor(0.1f, 0.1f, 0.1f, 0.8f), StartX - 5, BottomAnchorY - 5, TotalW + 10, BarH + 10);

	// Draw 3 Segments
	for (int32 i = 0; i < 3; i++)
	{
		// Calculate fill for this specific segment (i=0 is 0-1, i=1 is 1-2, etc)
		float SegmentValue = Flow->CurrentCharge - i; 
		float FillPct = FMath::Clamp(SegmentValue, 0.0f, 1.0f);

		float MyX = StartX + (i * (BarW + Gap));
	
		// Empty Slot Background
		DrawRect(FLinearColor(0.2f, 0.2f, 0.2f, 1.0f), MyX, BottomAnchorY, BarW, BarH);
	
		// Fill
		if (FillPct > 0.0f)
		{
			// Cyan if full, Yellow if filling
			FLinearColor Color = (FillPct >= 0.99f) ? FColor::Cyan : FLinearColor::Yellow;
			DrawRect(Color, MyX, BottomAnchorY, BarW * FillPct, BarH);
		}
	}

	// Draw Overdrive Line (Thin Red Bar on top)
	float OD_Y = BottomAnchorY - 10.0f;
	DrawRect(FLinearColor(0.1f, 0.1f, 0.1f, 0.8f), StartX, OD_Y, TotalW, 4.0f); // BG
	
	if (Flow->IsOverdrive())
	{
		DrawRect(FLinearColor::Red, StartX, OD_Y, TotalW, 4.0f);
	}

	// =========================================================
	// 2. SCORE & MULTIPLIER (Above Charge Bars)
	// =========================================================
	FString ScoreStr = FString::Printf(TEXT("SCORE: %0.0f"), Flow->CurrentScore);
	FString MultStr  = FString::Printf(TEXT("x%0.1f"), Flow->CurrentMultiplier);

	// Multiplier Color logic
	FLinearColor MultColor = FLinearColor::White;
	if (Flow->CurrentMultiplier > 4.0f) MultColor = FLinearColor::Red;
	else if (Flow->CurrentMultiplier > 2.0f) MultColor = FLinearColor::Yellow;
	else if (Flow->CurrentMultiplier > 0.0f) MultColor = FLinearColor::Green;

	DrawText(ScoreStr, FLinearColor::White, StartX, BottomAnchorY - 30.0f, nullptr, 1.2f);
	DrawText(MultStr, MultColor, RightEdge - 60.0f, BottomAnchorY - 30.0f, nullptr, 1.5f);

	// =========================================================
	// 3. DRIFT STAMINA (Vertical Left)
	// =========================================================
	float StaminaW = 15.0f;
	float StaminaH = 200.0f;
	float StaminaX = 50.0f;
	float StaminaY = Canvas->ClipY * 0.5f - (StaminaH * 0.5f);

	// Background
	DrawRect(FLinearColor(0.1f, 0.1f, 0.1f, 0.5f), StaminaX, StaminaY, StaminaW, StaminaH);

	// Calculate Fill (From Bottom)
	float CurrentStamina = Player->GetDriftStamina();
	float MaxStamina = Player->MaxDriftStamina;
	
	if (MaxStamina > 0.0f)
	{
		float StaminaPct = FMath::Clamp(CurrentStamina / MaxStamina, 0.0f, 1.0f);
		float FillH = StaminaH * StaminaPct;
		float FillY = StaminaY + (StaminaH - FillH); 

		if (StaminaPct > 0.0f)
		{
			FLinearColor FuseColor = FLinearColor::Green;
			if (StaminaPct < 0.25f) FuseColor = FLinearColor::Red; // Low warning
			DrawRect(FuseColor, StaminaX, FillY, StaminaW, FillH);
		}
	}
	
	// Label
	DrawText(TEXT("FUSE"), FLinearColor::White, StaminaX, StaminaY + StaminaH + 5.0f, nullptr, 1.0f);

	// =========================================================
	// 4. ACTION LOG (Above Score)
	// =========================================================
	DrawActionLog(BottomAnchorY - 60.0f, RightEdge);
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
			i--; 
			continue;
		}

		// Fade out
		float Alpha = FMath::Clamp(Msg.TimeRemaining / 0.5f, 0.0f, 1.0f);
		
		FLinearColor TextColor = FLinearColor::White;
		FString Prefix = "";
		
		if (Msg.Type == EStyleEventType::Good) { TextColor = FLinearColor::Green; Prefix = "+ "; }
		else if (Msg.Type == EStyleEventType::Bad) { TextColor = FLinearColor::Red; Prefix = "- "; }
		else { TextColor = FLinearColor(0.8f, 0.8f, 0.8f); } 
		
		TextColor.A = Alpha;

		FString FullText = Prefix + Msg.Text;
		
		// Align Right
		float XL, YL;
		Canvas->StrLen(GEngine->GetSmallFont(), FullText, XL, YL);
		XL *= 1.2f; YL *= 1.2f; // Scale

		DrawText(FullText, TextColor, RightAnchorX - XL, CurrentY - YL, nullptr, 1.2f);
		
		// Move cursor up
		CurrentY -= (YL + 5.0f); 
	}
}