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

	DrawLine(CX, CY, CX + (InputDir.Y * LineLen), CY - (InputDir.X * LineLen), FLinearColor::Yellow, 2.0f);
	DrawLine(CX, CY, CX + (VelDir.Y * LineLen), CY - (VelDir.X * LineLen), FColor::Cyan, 2.0f);

	FString AngleText = FString::Printf(TEXT("%.0f"), Player->DebugSlipAngle);
	FLinearColor AngleColor = (Player->DebugSlipAngle > 20 && Player->DebugSlipAngle < 90) ? FLinearColor::Green : FLinearColor::White;
	DrawText(AngleText, AngleColor, CX + 10, CY - 40, nullptr, 1.2f);
}

void APcDebugHUD::DrawFlowDashboard(APcPlayerCharacter* Player)
{
	UPcFlowMechanicComponent* Flow = Player->FlowComp;
	if (!Flow) return;

	// === 1. CHARGE BARS (Bottom Center/Right) ===
	float RightEdge = Canvas->ClipX - 50.0f;
	float BottomAnchorY = Canvas->ClipY * 0.90f; 

	float BarW = 80.0f; 
	float Gap = 10.0f;
	float BarH = 20.0f;
	float TotalW = (BarW * 3) + (Gap * 2);
	// Center the bars horizontally
	float StartX = (Canvas->ClipX * 0.5f) - (TotalW * 0.5f);

	// Background for Bars
	DrawRect(FLinearColor(0.1f, 0.1f, 0.1f, 0.8f), StartX - 5, BottomAnchorY - 5, TotalW + 10, BarH + 10);

	for (int32 i = 0; i < 3; i++)
	{
		float SegmentValue = Flow->CurrentCharge - i; 
		float FillPct = FMath::Clamp(SegmentValue, 0.0f, 1.0f);
		float MyX = StartX + (i * (BarW + Gap));
	
		DrawRect(FLinearColor(0.2f, 0.2f, 0.2f, 1.0f), MyX, BottomAnchorY, BarW, BarH);
	
		if (FillPct > 0.0f)
		{
			FLinearColor Color = (FillPct >= 0.99f) ? FColor::Cyan : FLinearColor::Yellow;
			DrawRect(Color, MyX, BottomAnchorY, BarW * FillPct, BarH);
		}
	}

	// Overdrive Line
	float OD_Y = BottomAnchorY - 10.0f;
	DrawRect(FLinearColor(0.1f, 0.1f, 0.1f, 0.8f), StartX, OD_Y, TotalW, 4.0f); 
	if (Flow->IsOverdrive()) DrawRect(FLinearColor::Red, StartX, OD_Y, TotalW, 4.0f);

	// === 2. SCORE & MULTIPLIER (Top Right) ===
	
	// Format: Arcade Padding (e.g., 0005230)
	FString ScoreStr = FString::Printf(TEXT("%07.0f"), Flow->CurrentScore);
	FString MultStr  = FString::Printf(TEXT("x %0.1f"), Flow->CurrentMultiplier);

	// Multiplier Color
	FLinearColor MultColor = FLinearColor::White;
	if (Flow->CurrentMultiplier > 4.0f) MultColor = FLinearColor::Red;
	else if (Flow->CurrentMultiplier > 2.0f) MultColor = FLinearColor::Yellow;
	else if (Flow->CurrentMultiplier > 0.0f) MultColor = FLinearColor::Green;

	// Dimensions for the Box
	float BoxW = 300.0f;
	float BoxH = 120.0f;
	float BoxX = Canvas->ClipX - BoxW - 20.0f;
	float BoxY = 20.0f;

	// Draw Background Panel (Dark Translucent)
	DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.5f), BoxX, BoxY, BoxW, BoxH);

	// Draw "MULTIPLIER" (Scale 3.5)
	// We use FMath::Sin to pulse the size slightly if multiplier is high
	float Pulse = 1.0f;
	if (Flow->CurrentMultiplier > 2.0f)
	{
		Pulse = 1.0f + (FMath::Sin(GetWorld()->GetTimeSeconds() * 10.0f) * 0.05f);
	}
	float MultScale = 3.5f * Pulse;
	
	float MultTextW, MultTextH;
	Canvas->StrLen(GEngine->GetLargeFont(), MultStr, MultTextW, MultTextH);
	// Right Align logic
	float DrawMultX = (BoxX + BoxW) - (MultTextW * MultScale) - 20.0f;
	float DrawMultY = BoxY + 10.0f;
	
	DrawText(MultStr, MultColor, DrawMultX, DrawMultY, GEngine->GetLargeFont(), MultScale);

	// Draw "SCORE" (Scale 2.0) - Below Multiplier
	float ScoreScale = 2.0f;
	float ScoreTextW, ScoreTextH;
	Canvas->StrLen(GEngine->GetLargeFont(), ScoreStr, ScoreTextW, ScoreTextH);
	
	float DrawScoreX = (BoxX + BoxW) - (ScoreTextW * ScoreScale) - 20.0f;
	float DrawScoreY = DrawMultY + (MultTextH * MultScale) + 5.0f;

	DrawText(ScoreStr, FLinearColor::White, DrawScoreX, DrawScoreY, GEngine->GetLargeFont(), ScoreScale);


	// === 3. DRIFT STAMINA (Left Vertical) ===
	float StaminaW = 15.0f;
	float StaminaH = 200.0f;
	float StaminaX = 50.0f;
	float StaminaY = Canvas->ClipY * 0.5f - (StaminaH * 0.5f);

	DrawRect(FLinearColor(0.1f, 0.1f, 0.1f, 0.5f), StaminaX, StaminaY, StaminaW, StaminaH);

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
			if (StaminaPct < 0.25f) FuseColor = FLinearColor::Red;
			DrawRect(FuseColor, StaminaX, FillY, StaminaW, FillH);
		}
	}
	DrawText(TEXT("FUSE"), FLinearColor::White, StaminaX, StaminaY + StaminaH + 5.0f, nullptr, 1.0f);

	// 4. ACTION LOG (Right Side, below Score Box)
	DrawActionLog(BoxY + BoxH + 20.0f, Canvas->ClipX - 40.0f);
}

void APcDebugHUD::DrawActionLog(float StartY, float RightAnchorX)
{
	float CurrentY = StartY;
	float DeltaTime = GetWorld()->GetDeltaSeconds();

	for (int32 i = 0; i < MessageLog.Num(); ++i)
	{
		FStyleLogMessage& Msg = MessageLog[i];
		Msg.TimeRemaining -= DeltaTime;
		if (Msg.TimeRemaining <= 0.0f) { MessageLog.RemoveAt(i); i--; continue; }

		float Alpha = FMath::Clamp(Msg.TimeRemaining / 0.5f, 0.0f, 1.0f);
		FLinearColor TextColor = FLinearColor::White;
		FString Prefix = "";
		
		if (Msg.Type == EStyleEventType::Good) { TextColor = FLinearColor::Green; Prefix = "+ "; }
		else if (Msg.Type == EStyleEventType::Bad) { TextColor = FLinearColor::Red; Prefix = "- "; }
		else { TextColor = FLinearColor(0.8f, 0.8f, 0.8f); } 
		
		TextColor.A = Alpha;
		FString FullText = Prefix + Msg.Text;
		
		float XL, YL;
		Canvas->StrLen(GEngine->GetSmallFont(), FullText, XL, YL);
		XL *= 1.2f; YL *= 1.2f;

		DrawText(FullText, TextColor, RightAnchorX - XL, CurrentY, nullptr, 1.2f);
		CurrentY += (YL + 5.0f); // Move Down
	}
}