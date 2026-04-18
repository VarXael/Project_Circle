#include "PcQDebugHUD.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Character.h"
#include "Engine/Canvas.h"
#include "Project_Circle/MusicSystem/MusicGameplaySystem/PcMusicAnalysisSubsystem.h"

void APcQDebugHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas) return;

	DrawDotCrosshair();

	if (APlayerController* PC = GetOwningPlayerController())
	{
		if (ACharacter* Char = Cast<ACharacter>(PC->GetPawn()))
		{
			if (UPcQPlayerMovementComponent* MC = Cast<UPcQPlayerMovementComponent>(Char->GetCharacterMovement()))
			{
				DrawBhopDebug(MC);
			}
		}
	}

	if (UPcMusicAnalysisSubsystem* MusicSub = GetWorld()->GetSubsystem<UPcMusicAnalysisSubsystem>())
	{
		DrawRhythmUI(MusicSub);
	}
}

void APcQDebugHUD::DrawRhythmUI(UPcMusicAnalysisSubsystem* MusicSub)
{
	if (!MusicSub || !MusicSub->IsReadyForPlayback()) return;

	float IntervalMS = MusicSub->GetGameplayBeatIntervalMS();
	if (IntervalMS <= 0.f) return;

	int32 CurrentTimeMS = MusicSub->GetCurrentPlaybackTimeMS();
	int32 NextBeatMS = MusicSub->GetNextGameplayBeatTimeMS();

	const float CX = Canvas->SizeX * 0.5f;
	const float CY = Canvas->SizeY * 0.5f + RhythmUI_YOffset;

	const float TrackLeft = CX - 100.f;
	const float TrackRight = CX + (RhythmUI_BeatsToShow * RhythmUI_PixelsPerBeat);
	DrawLine(TrackLeft, CY, TrackRight, CY, FLinearColor(1.f, 1.f, 1.f, 0.15f), 3.f);

	float PrevBeatMS = NextBeatMS - IntervalMS;
	float DistToNext = FMath::Abs((float)(NextBeatMS - CurrentTimeMS));
	float DistToPrev = FMath::Abs((float)(CurrentTimeMS - PrevBeatMS));
	float MinDist = FMath::Min(DistToNext, DistToPrev);
	
	float FlashAlpha = FMath::Max(0.f, 1.f - (MinDist / 120.f)); 
	FLinearColor StrikeColor = FLinearColor::LerpUsingHSV(FLinearColor(1.f, 1.f, 1.f, 0.3f), FLinearColor(0.f, 1.f, 1.f, 1.f), FlashAlpha);
	
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.6f), CX - 2.f, CY - 20.f, 4.f, 40.f); 
	DrawRect(StrikeColor, CX - 1.f, CY - 18.f, 2.f, 36.f);
	DrawCircleHUD(CX, CY, 16.f + (6.f * FlashAlpha), StrikeColor, 2.f + (2.f * FlashAlpha), 32);

	float LookaheadSec = (IntervalMS * RhythmUI_BeatsToShow) / 1000.f;
	TArray<FPcRuntimeEvent> Notes = MusicSub->GetUpcomingNotes(LookaheadSec);
	
	for (const FPcRuntimeEvent& Note : Notes)
	{
		float TimeDiffMS = Note.TimestampMS - CurrentTimeMS;
		if (TimeDiffMS < -100.f) continue; 

		float XOffset = (TimeDiffMS / IntervalMS) * RhythmUI_PixelsPerBeat;
		float Alpha = FMath::Clamp(1.0f - (TimeDiffMS / (IntervalMS * RhythmUI_BeatsToShow)), 0.f, 1.f);
		if (TimeDiffMS < 0.f) Alpha = FMath::Clamp(1.0f - (FMath::Abs(TimeDiffMS) / 100.f), 0.f, 1.f); 

		DrawCircleHUD(CX + XOffset, CY, 6.f + (2.f * Alpha), FLinearColor(1.f, 0.1f, 0.1f, Alpha), 3.f, 16);
	}

	for (int32 i = -1; i <= RhythmUI_BeatsToShow; ++i)
	{
		float BeatTimeMS = NextBeatMS + (i * IntervalMS);
		float TimeDiffMS = BeatTimeMS - CurrentTimeMS;
		
		if (TimeDiffMS < -100.f) continue; 
		
		float XOffset = (TimeDiffMS / IntervalMS) * RhythmUI_PixelsPerBeat;
		float Alpha = FMath::Clamp(1.0f - (TimeDiffMS / (IntervalMS * RhythmUI_BeatsToShow)), 0.f, 1.f); 
		if (TimeDiffMS < 0.f) Alpha = FMath::Clamp(1.0f - (FMath::Abs(TimeDiffMS) / 100.f), 0.f, 1.f); 
		
		DrawRect(FLinearColor(0.f, 1.f, 1.f, Alpha), CX + XOffset - 7.f, CY - 7.f, 14.f, 14.f);
	}
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
		DrawLine(CX + Radius * FMath::Cos(i * Step), CY + Radius * FMath::Sin(i * Step),
				 CX + Radius * FMath::Cos((i + 1) * Step), CY + Radius * FMath::Sin((i + 1) * Step), Color, Thickness);
}

void APcQDebugHUD::DrawBhopDebug(UPcQPlayerMovementComponent* MC)
{
	const float PanelX = 30.f; float PanelY = 30.f; const float LineH  = 22.f;

	auto Row = [&](const FString& Label, const FString& Value, FLinearColor Color = FLinearColor::White) {
		DrawText(Label + TEXT("  ") + Value, Color, PanelX, PanelY, GEngine->GetSmallFont(), 1.f);
		PanelY += LineH;
	};

	EBhopState State = MC->GetBhopState();
	FString StateStr = State == EBhopState::Idle ? "IDLE" : (State == EBhopState::Charging ? "CHARGING" : "BEAT-SYNCED");
	if (State == EBhopState::GroundPounding) StateStr = "GROUND POUND";
	Row(TEXT("STATE:"), StateStr, GetStateColor(State));

	if (State == EBhopState::Charging) {
		const float Alpha = MC->GetChargeAlpha();
		DrawRect(FLinearColor(0.1f, 0.1f, 0.1f, 0.8f), PanelX, PanelY, 160.f, 10.f);
		DrawRect(FLinearColor::LerpUsingHSV(FLinearColor::Yellow, FLinearColor::Green, Alpha), PanelX, PanelY, 160.f * Alpha, 10.f);
		PanelY += 16.f;
	}

	if (UPcMusicAnalysisSubsystem* Sub = GetWorld()->GetSubsystem<UPcMusicAnalysisSubsystem>()) {
		Row(TEXT("PRESET:"), Sub->GetActivePresetName(), FLinearColor::Yellow);
		Row(TEXT("GAME BPM:"), FString::Printf(TEXT("%.1f"), Sub->GetCurrentGameplayBPM()));
	}
	Row(TEXT("COYOTE:"), MC->HasQueuedJump() ? TEXT("ACTIVE") : TEXT("--"), MC->HasQueuedJump() ? FLinearColor::Yellow : FLinearColor(0.5f, 0.5f, 0.5f));

	const float HSpeed = MC->GetHorizontalSpeed();
	FLinearColor SpeedCol = MC->IsInBhopChain() ? FLinearColor(1.f, 0.45f, 0.f) : FLinearColor::White;
	Row(TEXT("SPEED:"), FString::Printf(TEXT("%.0f u/s"), HSpeed), SpeedCol);

	DrawRect(FLinearColor(0.1f, 0.1f, 0.1f, 0.8f), PanelX, PanelY, 160.f, 6.f);
	DrawRect(SpeedCol, PanelX, PanelY, 160.f * FMath::Clamp(HSpeed / (MC->MaxWalkSpeed * 3.f), 0.f, 1.f), 6.f);
	PanelY += 14.f;

	Row(TEXT("V SPEED:"), FString::Printf(TEXT("%.0f u/s"), MC->Velocity.Z), MC->Velocity.Z < -10.f ? FLinearColor(0.6f, 0.6f, 1.f) : FLinearColor::White);
	Row(TEXT("GROUNDED:"), MC->IsMovingOnGround() ? TEXT("YES") : TEXT("NO"), MC->IsMovingOnGround() ? FLinearColor::Green : FLinearColor(0.6f, 0.6f, 1.f));
}

FLinearColor APcQDebugHUD::GetStateColor(EBhopState State) const {
	if (State == EBhopState::GroundPounding) return FLinearColor::Red;
	return State == EBhopState::Idle ? FLinearColor(0.5f, 0.5f, 0.5f) : (State == EBhopState::Charging ? FLinearColor::Yellow : FLinearColor::Green);
}