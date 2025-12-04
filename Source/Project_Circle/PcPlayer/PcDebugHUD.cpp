#include "PcDebugHUD.h"
#include "Project_Circle/PcPlayer/PcPlayerCharacter.h"
#include "Engine/Canvas.h"
#include "Engine/World.h" 

void APcDebugHUD::AddStyleMessage(FString Message, EStyleEventType Type)
{
	FStyleLogMessage NewMsg;
	NewMsg.Text = Message;
	NewMsg.Type = Type;
	NewMsg.TimeRemaining = MessageLifetime;
	MessageLog.Insert(NewMsg, 0);
	if (MessageLog.Num() > 8) MessageLog.Pop();
}

void APcDebugHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!Canvas) return;

	APawn* OwningPawn = GetOwningPawn();
	APcPlayerCharacter* Player = Cast<APcPlayerCharacter>(OwningPawn);
	if (!Player) return;

	float DeltaTime = GetWorld()->GetDeltaSeconds();
	
	// --- LOG (Top Right) ---
	float RightEdge = Canvas->ClipX - MarginRight;
	float LogY = MarginTop;
	float LineHeight = 25.0f;

	for (int32 i = MessageLog.Num() - 1; i >= 0; --i)
	{
		MessageLog[i].TimeRemaining -= DeltaTime; 
		if (MessageLog[i].TimeRemaining <= 0.0f) MessageLog.RemoveAt(i);
	}

	for (const FStyleLogMessage& Msg : MessageLog)
	{
		FLinearColor Color = FLinearColor::White;
		FString Prefix = "";
		if (Msg.Type == EStyleEventType::Good) { Color = FLinearColor::Green; Prefix = "+ "; }
		else if (Msg.Type == EStyleEventType::Bad) { Color = FLinearColor::Red; Prefix = "- "; }

		Color.A = 1.0f; // Force Visible
		DrawText(Prefix + Msg.Text, Color, RightEdge, LogY, nullptr, 1.2f);
		LogY += LineHeight;
	}

	// --- FUSE BAR (Bottom Right - Fixed Position) ---
	float BarY = Canvas->ClipY * 0.8f; // 80% down the screen
	
	float FusePercent = Player->GetFuseFraction();
	int32 Stacks = Player->GetFlowStacks();

	// 1. Background (Solid Dark Gray)
	DrawRect(FLinearColor(0.1f, 0.1f, 0.1f, 1.0f), RightEdge, BarY, BarWidth, BarHeight);

	// 2. Foreground (Solid Color)
	FLinearColor BarColor = FLinearColor::Blue;
	if (Stacks == 1) BarColor = FLinearColor::Yellow;
	if (Stacks == 2) BarColor = FLinearColor(1.0f, 0.5f, 0.0f); // Orange
	if (Stacks >= 3) BarColor = FLinearColor::Red;

	if (FusePercent > 0.0f)
	{
		DrawRect(BarColor, RightEdge, BarY, BarWidth * FusePercent, BarHeight);
	}

	// 3. Text
	DrawText(FString::Printf(TEXT("FLOW x%d"), Stacks), FLinearColor::White, RightEdge, BarY - 25.0f, nullptr, 1.5f);

	// --- STATS (Debug Speed) ---
	float StatsY = BarY + BarHeight + 10.0f;
	
	FString SpdInfo = FString::Printf(TEXT("SPD: %.0f / LIMIT: %.0f (CAP: %.0f)"), 
		Player->GetCurrentSpeed(), 
		Player->GetTargetMaxSpeed(),
		Player->MaxSkimSpeed);
		
	DrawText(SpdInfo, FLinearColor::White, RightEdge, StatsY, nullptr, 1.2f);

	// --- RHYTHM PROMPT (Center) ---
	if (Player->IsInRhythmWindow())
	{
		float CX = Canvas->ClipX * 0.5f;
		float CY = Canvas->ClipY * 0.5f;
		DrawText(">> JUMP <<", FLinearColor::Green, CX - 50.0f, CY + 50.0f, nullptr, 2.0f);
	}
	
	// --- CROSSHAIR (New) ---
	float CX = Canvas->ClipX * 0.5f;
	float CY = Canvas->ClipY * 0.5f;
	float Size = 10.0f;
	FLinearColor CrossColor = FLinearColor::White;

	// Horizontal Line
	DrawLine(CX - Size, CY, CX + Size, CY, CrossColor, 2.0f);
	// Vertical Line
	DrawLine(CX, CY - Size, CX, CY + Size, CrossColor, 2.0f);
}