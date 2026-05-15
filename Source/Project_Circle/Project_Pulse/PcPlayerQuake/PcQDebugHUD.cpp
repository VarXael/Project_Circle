#include "PcQDebugHUD.h"
#include "PcQPlayerCharacter.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Character.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Project_Circle/MusicSystem/MusicGameplaySystem/PcMusicAnalysisSubsystem.h"

void APcQDebugHUD::RegisterThreat(int32 TimestampMS, const FString& Label, FLinearColor Color) {
	FPcHudThreatEvent& Ev = ActiveThreats.AddDefaulted_GetRef();
	Ev.TimestampMS = TimestampMS; Ev.Label = Label; Ev.Color = Color;
}

void APcQDebugHUD::PurgeThreat(int32 TimestampMS) {
	ActiveThreats.RemoveAll([TimestampMS](const FPcHudThreatEvent& Ev) { return Ev.TimestampMS == TimestampMS; });
}

APcQDebugHUD::FArcGeom APcQDebugHUD::BuildArcGeom() const {
	FArcGeom G; G.CX = Canvas->SizeX * 0.5f; G.CY = Canvas->SizeY + Arc_CenterBelowScreen;
	const float SinLimb = -Arc_CenterBelowScreen / Arc_Radius;
	const float BaseLimb = FMath::Asin(FMath::Clamp(SinLimb, -1.f, 1.f));
	G.SpawnAngle = BaseLimb - 0.05f; G.BufferAngle = -(PI - FMath::Asin(-SinLimb)) + 0.05f;
	G.StrikeAngle = FMath::Lerp(G.SpawnAngle, G.BufferAngle, Arc_StrikeGatePercent);
	return G;
}

void APcQDebugHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas) return;

	float FlashHard = 0.f, FlashSoft = 0.f, BeatRemainingFraction = 1.f;

	if (UPcMusicAnalysisSubsystem* MusicSub = GetWorld()->GetSubsystem<UPcMusicAnalysisSubsystem>()) {
		if (MusicSub->IsReadyForPlayback()) {
			const float IntervalMS = MusicSub->GetGameplayBeatIntervalMS();
			const int32 CurrentTimeMS = MusicSub->GetCurrentPlaybackTimeMS();
			const int32 NextBeatMS = MusicSub->GetNextGameplayBeatTimeMS();
			if (IntervalMS > 0.f) {
				const float FlashWindow = IntervalMS * Arc_FlashWindowPct;
				const float DistToNext = FMath::Abs((float)(NextBeatMS - CurrentTimeMS));
				const float DistToPrev = FMath::Abs(CurrentTimeMS - ((float)NextBeatMS - IntervalMS));
				const float MinDist = FMath::Min(DistToNext, DistToPrev);
				FlashHard = FMath::Clamp(1.f - (MinDist / (FlashWindow * 0.5f)), 0.f, 1.f);
				FlashSoft = FMath::Clamp(1.f - (MinDist / FlashWindow), 0.f, 1.f);
				BeatRemainingFraction = FMath::Clamp(MusicSub->GetTimeUntilNextGameplayBeat() / (IntervalMS / 1000.f), 0.f, 1.f);

				const FArcGeom G = BuildArcGeom();
				if (bShowArcMetronome) DrawArcMetronome(MusicSub, G, CurrentTimeMS, NextBeatMS, IntervalMS, FlashHard, FlashSoft);
				if (bShowGlanceBoard)  DrawGlanceBoard(MusicSub, CurrentTimeMS, NextBeatMS, IntervalMS, FlashHard);
			}
		}
	}

	if (APlayerController* PC = GetOwningPlayerController()) {
		if (APcQPlayerCharacter* Char = Cast<APcQPlayerCharacter>(PC->GetPawn())) {
			if (UPcQPlayerMovementComponent* MC = Char->MoveComp) {
				
				if (!MC->OnComboEvent.IsAlreadyBound(this, &APcQDebugHUD::OnComboEvent))
					MC->OnComboEvent.AddDynamic(this, &APcQDebugHUD::OnComboEvent);

				float ActionFlash = MC->GetOnBeatFlash();
				if (bEnableOnBeatHitMarker && ActionFlash > 0.01f) {
					const float CX = Canvas->SizeX * 0.5f, CY = Canvas->SizeY * 0.5f;
					const float HitR = 30.f + (1.f - ActionFlash) * 50.f; 
					DrawCircleHUD(CX, CY, HitR, FLinearColor(1.f, 0.85f, 0.2f, ActionFlash * 0.4f), 2.5f, 32);
					const float Exp = (1.f - ActionFlash), Dist = 20.f + Exp * 25.f, Len = 8.f + ActionFlash * 8.f;
					const FLinearColor HitCol(1.f, 0.9f, 0.1f, ActionFlash * 0.8f);
					DrawLine(CX - Dist, CY - Dist, CX - Dist - Len, CY - Dist - Len, HitCol, 2.5f);
					DrawLine(CX + Dist, CY - Dist, CX + Dist + Len, CY - Dist - Len, HitCol, 2.5f);
					DrawLine(CX - Dist, CY + Dist, CX - Dist - Len, CY + Dist + Len, HitCol, 2.5f);
					DrawLine(CX + Dist, CY + Dist, CX + Dist + Len, CY + Dist + Len, HitCol, 2.5f);
				}

				DrawDotCrosshair(BeatRemainingFraction, MC, FlashSoft);
				DrawPlayerStatus(MC, Char, FlashSoft);
				DrawMovementDebug(MC);
			}
		}
	}
}

// =============================================================================
//  CLEAN PLAYER STATUS (Abilities & Movement Blocks)
// =============================================================================
void APcQDebugHUD::DrawPlayerStatus(UPcQPlayerMovementComponent* MC, APcQPlayerCharacter* PC, float FlashSoft)
{
	if (!Canvas || !GEngine || !MC || !PC) return;
	const float CX = Canvas->SizeX * 0.5f, BY = Canvas->SizeY * 0.85f; 
	const float BarW = 160.f, BarH = 8.f, Gap = 24.f;

	auto DrawStatusBlock = [&](float X, float Y, const FString& Label, float FillPct, FLinearColor Col, bool bIsActive) {
		DrawTextWithShadow(Label, Col * FLinearColor(1,1,1, 0.95f), X, Y - 20.f, GEngine->GetSmallFont(), 1.05f);
		const FLinearColor FrameCol = FLinearColor(1,1,1, 0.15f);
		DrawRect(FLinearColor(0.01f, 0.01f, 0.02f, 0.75f), X - 2.f, Y - 2.f, BarW + 4.f, BarH + 4.f);
		DrawLine(X - 4.f, Y - 4.f, X + 10.f, Y - 4.f, FrameCol, 1.5f); DrawLine(X - 4.f, Y - 4.f, X - 4.f, Y + 6.f, FrameCol, 1.5f);
		DrawLine(X + BarW + 4.f, Y + BarH + 4.f, X + BarW - 10.f, Y + BarH + 4.f, FrameCol, 1.5f); DrawLine(X + BarW + 4.f, Y + BarH + 4.f, X + BarW + 4.f, Y + BarH - 6.f, FrameCol, 1.5f);
		DrawRect(FLinearColor(0.05f, 0.05f, 0.05f, 1.f), X, Y, BarW, BarH);
		if (FillPct > 0.01f) {
			float Alpha = bIsActive ? (0.85f + 0.15f * FlashSoft) : 0.45f;
			DrawRect(Col * FLinearColor(1,1,1, Alpha), X, Y, BarW * FillPct, BarH);
		}
	};

	// ── LEFT: Movement/Dash Status ──
	float DashFill = MC->GetDashActiveAlpha();
	bool bDashing = MC->IsDashing();
	FString DashLabel = bDashing ? TEXT("DASH ACTIVE") : (DashFill > 0.f ? TEXT("DASH READY") : TEXT("DASH EMPTY"));
	FLinearColor DashCol = bDashing ? FLinearColor(1.f, 0.55f, 0.15f) : (DashFill > 0.f ? FLinearColor(0.8f, 0.4f, 0.1f) : FLinearColor(0.4f, 0.4f, 0.4f));
	DrawStatusBlock(CX - BarW - Gap, BY, DashLabel, bDashing ? 1.f : DashFill, DashCol, bDashing);

	// ── RIGHT: Double Jump Status ──
	float DJFill = MC->HasDoubleJump() ? 1.f : 0.f; 
	FString DJLabel = MC->HasDoubleJump() ? TEXT("DOUBLE JUMP READY") : TEXT("DOUBLE JUMP EMPTY");
	FLinearColor DJCol = MC->HasDoubleJump() ? FLinearColor(0.2f, 0.6f, 1.f) : FLinearColor(0.4f, 0.4f, 0.55f);
	DrawStatusBlock(CX + Gap, BY, DJLabel, DJFill, DJCol, MC->HasDoubleJump());

	// ── BOTTOM: Combat Status (Ammo) ──
	int32 Ammo = PC->GetCurrentAmmo();
	bool bReloading = PC->IsReloading();
	float AmmoFill = bReloading ? 0.f : (float)Ammo / 6.f; // MaxAmmo is 6
	FString CombatLabel = bReloading ? TEXT("RELOADING...") : FString::Printf(TEXT("AMMO: %d / 6"), Ammo);
	FLinearColor CombatCol = bReloading ? FLinearColor(1.f, 0.2f, 0.2f) : FLinearColor(0.f, 0.8f, 1.f);
	DrawStatusBlock(CX - (BarW * 0.5f), BY + 45.f, CombatLabel, AmmoFill, CombatCol, !bReloading);

	// ── COMBO FEED ──
	const float Now = GetWorld()->GetTimeSeconds();
	float ComboY = BY - 140.f;

	ComboFeed.RemoveAll([&](const FPcComboFeedEntry& E) { return (Now - E.BornAt) >= ComboFeed_FadeDuration; });
	for (int32 i = ComboFeed.Num() - 1; i >= 0; --i) {
		const FPcComboFeedEntry& E = ComboFeed[i];
		const float Age = Now - E.BornAt;
		const float HalfDur = ComboFeed_FadeDuration * 0.6f;
		const float Alpha = Age < HalfDur ? 1.f : FMath::Clamp(1.f - (Age - HalfDur) / (ComboFeed_FadeDuration - HalfDur), 0.f, 1.f);
		if (Alpha < 0.01f) continue;

		const float SlideInX = Age < 0.15f ? (1.f - Age / 0.15f) * -30.f : 0.f;
		const float TxtX = (CX - BarW) + 12.f + SlideInX;
		const float TxtY = ComboY;

		DrawRect(FLinearColor(0.01f, 0.01f, 0.02f, Alpha * 0.6f), (CX - BarW) + SlideInX, TxtY - 2.f, BarW * 2.f, 20.f);
		DrawRect(E.Color * FLinearColor(1,1,1, Alpha * 0.9f), (CX - BarW) + SlideInX, TxtY - 2.f, 4.f, 20.f);
		DrawTextWithShadow(E.Label, E.Color * FLinearColor(1, 1, 1, Alpha), TxtX, TxtY, GEngine->GetSmallFont(), 1.15f);
		ComboY += 26.f;
	}
}

void APcQDebugHUD::DrawMovementDebug(UPcQPlayerMovementComponent* MC)
{
	if (!MC || !GEngine || !Canvas) return;
	const float PanelX = 20.f; float PanelY = 20.f; const float LineH = 22.f;
	auto Row = [&](const FString& Label, const FString& Value, FLinearColor Color = FLinearColor::White) {
		DrawTextWithShadow(Label + TEXT("  ") + Value, Color, PanelX, PanelY, GEngine->GetSmallFont(), 1.f);
		PanelY += LineH;
	};

	EPlayerMovementState State = MC->GetMovementState();
	FString StateStr;
	if      (State == EPlayerMovementState::RecallLunging)  StateStr = TEXT("RECALL LUNGING");
	else if (State == EPlayerMovementState::Dashing)        StateStr = TEXT("DASHING");
	else if (State == EPlayerMovementState::GroundPounding) StateStr = TEXT("GROUND POUND");
	else if (State == EPlayerMovementState::InAir)          StateStr = TEXT("IN AIR");
	else                                                    StateStr = TEXT("GROUNDED");
	Row(TEXT("STATE:"), StateStr, GetStateColor(State));

	if (MC->GetJumpBufferAlpha() > 0.f) Row(TEXT("BUFFER:"), TEXT("JUMP READY"), FLinearColor(0.4f, 0.8f, 1.f));

	const float HSpeed = MC->GetHorizontalSpeed();
	FLinearColor SpeedCol = HSpeed > MC->MaxWalkSpeed * 1.05f ? FLinearColor(1.f,0.45f,0.f) : FLinearColor::White;
	Row(TEXT("SPEED:"), FString::Printf(TEXT("%.0f u/s"), HSpeed), SpeedCol);
	DrawRect(FLinearColor(0.05f,0.05f,0.05f,0.85f), PanelX, PanelY, 140.f, 4.f);
	DrawRect(SpeedCol, PanelX, PanelY, 140.f * FMath::Clamp(HSpeed/(MC->MaxWalkSpeed*3.f),0.f,1.f), 4.f);
}

// =============================================================================
//  METRONOME & CROSSHAIR
// =============================================================================

void APcQDebugHUD::DrawDotCrosshair(float BeatRemainingFraction, UPcQPlayerMovementComponent* MC, float FlashSoft)
{
	if (!Canvas || !MC) return;
	const float CX = Canvas->SizeX * 0.5f, CY = Canvas->SizeY * 0.5f;
	const float FlashA = FMath::Clamp(1.f - BeatRemainingFraction / 0.15f, 0.f, 1.f);
	
	const float AW = CrosshairChevronWidth, AH = CrosshairChevronHeight;
	const float EXTRA_H = 2.0f, THICK = 2.5f;
	auto DrawChev = [&](float TipX, bool bLeft, float Alpha, FLinearColor Col) {
		const float BackX = bLeft ? TipX + AW : TipX - AW;
		const float H = AH + EXTRA_H;
		const FLinearColor Sh(0.f, 0.f, 0.f, FMath::Min(0.5f, Alpha * 0.8f));
		DrawLine(BackX + 1.f, CY - H + 1.f, TipX + 1.f, CY + 1.f, Sh, THICK + 1.5f); 
		DrawLine(TipX + 1.f, CY + 1.f, BackX + 1.f, CY + H + 1.f, Sh, THICK + 1.5f);
		FLinearColor FC = Col; FC.A *= Alpha;
		DrawLine(BackX, CY - H, TipX, CY, FC, THICK); 
		DrawLine(TipX, CY, BackX, CY + H, FC, THICK);
	};

	const float MaxBeats = FMath::Max(1.0f, (float)CrosshairBeatsToShow);
	for (int32 i = CrosshairBeatsToShow - 1; i >= 0; --i) {
		const float d = BeatRemainingFraction + (float)i;
		const float DistNormalized = FMath::Clamp(d / MaxBeats, 0.0f, 1.0f);
		const float Alpha = FMath::Pow(1.0f - DistNormalized, 2.0f);
		if (Alpha < 0.01f) continue;
		const float Dist = CrosshairGateDist + d * CrosshairBeatStep;
		DrawChev(CX - Dist, true, Alpha, FLinearColor(0.8f,0.8f,0.8f,Alpha));
		DrawChev(CX + Dist, false, Alpha, FLinearColor(0.8f,0.8f,0.8f,Alpha));
	}
	
	const FLinearColor GateResting(0.1f, 0.1f, 0.1f, 0.5f), GateFlash(1.f,1.f,1.f,1.f);
	const FLinearColor GateCol = GateResting + (GateFlash - GateResting) * FlashA;
	DrawChev(CX - CrosshairGateDist, true,  1.f, GateCol);
	DrawChev(CX + CrosshairGateDist, false, 1.f, GateCol);
	
	const float DR = DotSize;
	const FLinearColor DotResting(0.05f,0.75f,1.0f,0.8f), DotFlash(1.f,1.f,1.f,1.f);
	const FLinearColor DotCol = DotResting + (DotFlash - DotResting) * FlashA;
	DrawRect(FLinearColor(0.f,0.f,0.f,0.8f), CX-DR-1.f, CY-DR-1.f, (DR+1.f)*2.f, (DR+1.f)*2.f);
	DrawRect(DotCol, CX-DR, CY-DR, DR*2.f, DR*2.f);
}

void APcQDebugHUD::DrawArcMetronome(UPcMusicAnalysisSubsystem* MusicSub, const FArcGeom& G, int32 CurrentTimeMS, int32 NextBeatMS, float IntervalMS, float FlashHard, float FlashSoft)
{
	const float BufferTimeMS = IntervalMS * 0.5f;
	DrawArcFilled(G.CX, G.CY, Arc_Radius, Arc_Thickness + 8.f, G.SpawnAngle, G.BufferAngle, FLinearColor(0.f, 0.f, 0.f, 0.12f), 60);
	DrawArcFilled(G.CX, G.CY, Arc_Radius, Arc_Thickness, G.StrikeAngle, G.BufferAngle, FLinearColor(0.01f, 0.015f, 0.025f, 0.25f), 20);

	const FLinearColor ActiveColor = FLinearColor::LerpUsingHSV(FLinearColor(0.02f, 0.05f, 0.1f, 0.25f), FLinearColor(0.015f, 0.09f, 0.16f, 0.35f), FlashSoft);
	DrawArcFilled(G.CX, G.CY, Arc_Radius, Arc_Thickness, G.SpawnAngle, G.StrikeAngle, ActiveColor, 60);
	DrawArcHUD(G.CX, G.CY, Arc_Radius - Arc_Thickness * 0.5f, 1.0f, G.SpawnAngle, G.StrikeAngle, FLinearColor(0.4f, 0.85f, 1.f, 0.025f + FlashSoft * 0.08f), 60);

	struct FNoteEntry { float TimeDiffMS; bool bIsThreat; int32 ThreatIdx; };
	TArray<FNoteEntry> AllNotes;
	AllNotes.Reserve(Arc_BeatsToShow + 2 + ActiveThreats.Num());

	for (int32 i = -1; i <= Arc_BeatsToShow; ++i) {
		const float Diff = ((float)NextBeatMS + i * IntervalMS) - (float)CurrentTimeMS;
		if (Diff >= -BufferTimeMS && Diff <= IntervalMS * (float)Arc_BeatsToShow) AllNotes.Add({ Diff, false, -1 });
	}
	for (int32 Ti = 0; Ti < ActiveThreats.Num(); ++Ti) {
		const float Diff = (float)(ActiveThreats[Ti].TimestampMS - CurrentTimeMS);
		if (Diff >= -BufferTimeMS && Diff <= IntervalMS * (float)Arc_BeatsToShow) AllNotes.Add({ Diff, true, Ti });
	}
	AllNotes.Sort([](const FNoteEntry& A, const FNoteEntry& B) { return A.TimeDiffMS < B.TimeDiffMS; });

	for (const FNoteEntry& Entry : AllNotes) {
		const float TimeDiff = Entry.TimeDiffMS;
		const bool bPassed = TimeDiff < 0.f;
		float Angle, Alpha;
		if (!bPassed) {
			const float Progress = FMath::Clamp(TimeDiff / (IntervalMS * (float)Arc_BeatsToShow), 0.f, 1.f);
			Angle = FMath::Lerp(G.StrikeAngle, G.SpawnAngle, Progress); Alpha = FMath::Lerp(1.f, 0.25f, Progress);
		} else {
			const float Progress = FMath::Clamp(FMath::Abs(TimeDiff) / BufferTimeMS, 0.f, 1.f);
			Angle = FMath::Lerp(G.StrikeAngle, G.BufferAngle, Progress); Alpha = 1.f - Progress;
		}
		if (Alpha < 0.02f) continue;
		const float NoteX = G.CX + Arc_Radius * FMath::Cos(Angle), NoteY = G.CY + Arc_Radius * FMath::Sin(Angle);
		if (NoteY > Canvas->SizeY + 4.f) continue;

		if (Entry.bIsThreat) DrawThreatNote(NoteX, NoteY, Alpha, bPassed, ActiveThreats[Entry.ThreatIdx], G);
		else {
			if (bPassed) DrawCircleHUD(NoteX, NoteY, 6.f, FLinearColor(0.20f, 0.24f, 0.34f, Alpha), 1.4f, 16);
			else {
				DrawCircleHUD(NoteX, NoteY, 12.f, FLinearColor(0.f, 0.80f, 1.f, Alpha * 0.16f), 1.f, 16);
				const bool bIsNext = TimeDiff < IntervalMS && TimeDiff >= 0.f;
				const float RingSize = bIsNext ? Arc_Thickness * 0.65f : Arc_Thickness * 0.42f;
				DrawCircleHUD(NoteX, NoteY, RingSize, FLinearColor(0.f, 0.82f, 1.f, Alpha * (bIsNext ? 1.f : 0.65f)), bIsNext ? 2.4f : 1.5f, 16);
				DrawRect(FLinearColor(0.70f, 0.95f, 1.f, Alpha * (bIsNext ? 1.f : 0.7f)), NoteX - 2.f, NoteY - 2.f, 4.f, 4.f);
			}
		}
	}

	const float StrikeX = G.CX + Arc_Radius * FMath::Cos(G.StrikeAngle), StrikeY = G.CY + Arc_Radius * FMath::Sin(G.StrikeAngle);
	if (FlashSoft > 0.01f) DrawCircleHUD(StrikeX, StrikeY, 34.f * FlashSoft, FLinearColor(0.1f, 0.9f, 0.85f, 0.05f * FlashSoft), 1.f, 24);
	if (FlashHard > 0.01f) DrawCircleHUD(StrikeX, StrikeY, 20.f, FLinearColor(0.15f, 1.f, 0.90f, 0.12f * FlashHard), 2.f, 24);
	
	const float InnerR = Arc_Radius - Arc_Thickness - 4.f, OuterR = Arc_Radius + Arc_Thickness + 4.f + 8.f * FlashHard;
	const FLinearColor TickColor = FLinearColor::LerpUsingHSV(FLinearColor(0.f, 0.55f, 0.65f, 0.55f), FLinearColor(0.16f, 1.f, 1.f, 1.f), FlashHard);
	DrawLine(G.CX + InnerR * FMath::Cos(G.StrikeAngle), G.CY + InnerR * FMath::Sin(G.StrikeAngle), G.CX + OuterR * FMath::Cos(G.StrikeAngle), G.CY + OuterR * FMath::Sin(G.StrikeAngle), TickColor, 3.f + 2.f * FlashHard);
}

void APcQDebugHUD::DrawThreatNote(float NoteX, float NoteY, float Alpha, bool bPassed, const FPcHudThreatEvent& Threat, const FArcGeom& G) {
	if (bPassed) {
		const FLinearColor Ghost(0.18f, 0.16f, 0.24f, Alpha * 0.8f); const float Hs = 6.f;
		DrawLine(NoteX, NoteY - Hs, NoteX + Hs, NoteY, Ghost, 1.5f); DrawLine(NoteX + Hs, NoteY, NoteX, NoteY + Hs, Ghost, 1.5f);
		DrawLine(NoteX, NoteY + Hs, NoteX - Hs, NoteY, Ghost, 1.5f); DrawLine(NoteX - Hs, NoteY, NoteX, NoteY - Hs, Ghost, 1.5f); return;
	}
	const FLinearColor OuterCol = Threat.Color * FLinearColor(1.f, 1.f, 1.f, Alpha);
	DrawCircleHUD(NoteX, NoteY, 15.f, Threat.Color * FLinearColor(1.f, 1.f, 1.f, Alpha * 0.15f), 1.f, 16);
	const float Os = 8.f;
	DrawLine(NoteX, NoteY - Os, NoteX + Os, NoteY, OuterCol, 2.f); DrawLine(NoteX + Os, NoteY, NoteX, NoteY + Os, OuterCol, 2.f);
	DrawLine(NoteX, NoteY + Os, NoteX - Os, NoteY, OuterCol, 2.f); DrawLine(NoteX - Os, NoteY, NoteX, NoteY - Os, OuterCol, 2.f);
	if (Alpha > 0.45f && GEngine) {
		const float DirX = NoteX - G.CX, DirY = NoteY - G.CY, DirLen = FMath::Sqrt(DirX*DirX + DirY*DirY);
		const float NormX = DirLen > 0.f ? DirX / DirLen : 0.f, NormY = DirLen > 0.f ? DirY / DirLen : -1.f;
		DrawTextWithShadow(Threat.Label, Threat.Color * FLinearColor(1.f,1.f,1.f, Alpha), NoteX + NormX*22.f - 12.f, NoteY + NormY*22.f - 5.f, GEngine->GetSmallFont(), 1.f);
	}
}

void APcQDebugHUD::DrawGlanceBoard(UPcMusicAnalysisSubsystem* MusicSub, int32 CurrentTimeMS, int32 NextBeatMS, float IntervalMS, float FlashHard) {
	if (!Canvas || !GEngine) return;
	const float StrikeY = Canvas->SizeY * GlanceBoard_ScreenYPercent, TopY = StrikeY - GlanceBoard_Height;
	const float PixPerBeat = GlanceBoard_Height / (float)GlanceBoard_BeatsToShow;
	const float PlayerTrackX = GlanceBoard_XOffset, EnemyTrackX = GlanceBoard_XOffset + GlanceBoard_TrackSpacing;
	const float PanelPad = 14.f, PanelW = GlanceBoard_TrackSpacing + PanelPad * 2.f;

	DrawRect(FLinearColor(0.01f,0.02f,0.03f,0.75f), PlayerTrackX-PanelPad, TopY-PanelPad, PanelW, GlanceBoard_Height+PanelPad*2.f);
	DrawLine(PlayerTrackX-PanelPad, TopY-PanelPad, PlayerTrackX-PanelPad, StrikeY+PanelPad, FLinearColor(0.f,0.55f,0.75f,0.3f), 1.5f); 
	DrawTextWithShadow(TEXT("BEAT"), FLinearColor(0.f,0.70f,0.85f,0.70f), PlayerTrackX-8.f, TopY-18.f, GEngine->GetSmallFont(), 1.f);
	DrawTextWithShadow(TEXT("THRT"), FLinearColor(1.f,0.20f,0.32f,0.70f), EnemyTrackX-8.f,  TopY-18.f, GEngine->GetSmallFont(), 1.f);
	DrawLine(PlayerTrackX, TopY, PlayerTrackX, StrikeY, FLinearColor(0.f,0.55f,0.75f,0.15f), 1.f);
	DrawLine(EnemyTrackX,  TopY, EnemyTrackX,  StrikeY, FLinearColor(0.8f,0.15f,0.25f,0.15f), 1.f);
	
	const FLinearColor StrikeLine = FLinearColor::LerpUsingHSV(FLinearColor(0.f,0.45f,0.55f,0.35f), FLinearColor(0.16f,1.f,1.f,0.92f), FlashHard);
	DrawLine(PlayerTrackX-PanelPad, StrikeY, PlayerTrackX-PanelPad+PanelW, StrikeY, StrikeLine, 2.f+1.5f*FlashHard);
	
	const float SqH = 5.f + 2.f * FlashHard;
	DrawRect(StrikeLine, PlayerTrackX-SqH*0.5f, StrikeY-SqH*0.5f, SqH, SqH);
	DrawRect(FLinearColor(1.f,0.25f,0.20f,0.45f+FlashHard*0.55f), EnemyTrackX-SqH*0.5f, StrikeY-SqH*0.5f, SqH, SqH);

	for (int32 i = -1; i <= GlanceBoard_BeatsToShow; ++i) {
		const float TimeDiffMS = ((float)NextBeatMS + i*IntervalMS) - (float)CurrentTimeMS;
		if (TimeDiffMS < -120.f) continue;
		const float NoteY = StrikeY - (TimeDiffMS/IntervalMS)*PixPerBeat;
		if (NoteY < TopY || NoteY > StrikeY+8.f) continue;
		const float Alpha = TimeDiffMS >= 0.f ? FMath::Clamp(1.f-(TimeDiffMS/(IntervalMS*(float)GlanceBoard_BeatsToShow)),0.f,1.f) : FMath::Clamp(1.f-FMath::Abs(TimeDiffMS)/120.f,0.f,1.f);
		const bool  bIsNext = (i==0 && TimeDiffMS>=0.f);
		const float DashW = bIsNext ? 18.f : 14.f, DashH = bIsNext ? 3.f : 2.f;
		DrawRect(FLinearColor(0.f,0.82f,1.f,Alpha*(bIsNext?0.92f:0.65f)), PlayerTrackX-DashW*0.5f, NoteY-DashH*0.5f, DashW, DashH);
	}
	
	const float LookaheadSec = (IntervalMS*(float)GlanceBoard_BeatsToShow)/1000.f;
	for (const FPcRuntimeEvent& Note : MusicSub->GetUpcomingNotes(LookaheadSec)) {
		const float TimeDiffMS = (float)(Note.TimestampMS - CurrentTimeMS);
		if (TimeDiffMS < -120.f) continue;
		const float NoteY = StrikeY-(TimeDiffMS/IntervalMS)*PixPerBeat;
		if (NoteY < TopY || NoteY > StrikeY+8.f) continue;
		const float Alpha = TimeDiffMS>=0.f ? FMath::Clamp(1.f-TimeDiffMS/(IntervalMS*(float)GlanceBoard_BeatsToShow),0.f,1.f) : FMath::Clamp(1.f-FMath::Abs(TimeDiffMS)/120.f,0.f,1.f);
		const float Dr = 5.f; const FLinearColor EC(1.f,0.15f,0.25f,Alpha);
		DrawLine(EnemyTrackX,NoteY-Dr,EnemyTrackX+Dr,NoteY,EC,1.5f); DrawLine(EnemyTrackX+Dr,NoteY,EnemyTrackX,NoteY+Dr,EC,1.5f);
		DrawLine(EnemyTrackX,NoteY+Dr,EnemyTrackX-Dr,NoteY,EC,1.5f); DrawLine(EnemyTrackX-Dr,NoteY,EnemyTrackX,NoteY-Dr,EC,1.5f);
	}
}

// =============================================================================
//  HELPERS & PRIMITIVES
// =============================================================================

void APcQDebugHUD::OnComboEvent(const FString& Label, FLinearColor Color) {
	if (!GetWorld()) return;
	FPcComboFeedEntry E; E.Label = Label; E.Color = Color; E.BornAt = GetWorld()->GetTimeSeconds();
	ComboFeed.Add(E); while (ComboFeed.Num() > ComboFeed_MaxEntries) ComboFeed.RemoveAt(0);
}

FLinearColor APcQDebugHUD::GetStateColor(EPlayerMovementState State) const {
	if (State == EPlayerMovementState::GroundPounding) return FLinearColor::Red;
	if (State == EPlayerMovementState::RecallLunging)  return FLinearColor(1.f,0.1f,0.1f);
	if (State == EPlayerMovementState::Dashing)        return FLinearColor(1.f,0.55f,0.f);
	if (State == EPlayerMovementState::InAir)          return FLinearColor(0.2f,0.6f,1.f);
	return FLinearColor::Green;
}

void APcQDebugHUD::DrawTextWithShadow(const FString& Text, FLinearColor Color, float X, float Y, UFont* Font, float Scale) {
	if (!Font) return;
	DrawText(Text, FLinearColor(0.f, 0.f, 0.f, Color.A * 0.9f), X + 1.5f * Scale, Y + 1.5f * Scale, Font, Scale);
	DrawText(Text, Color, X, Y, Font, Scale);
}
void APcQDebugHUD::DrawCircleHUD(float CX, float CY, float Radius, FLinearColor Color, float Thickness, int32 Segments, float AngleOffset) {
	if (Segments <= 0) return;
	const float Step = 2.f*PI/(float)Segments;
	for (int32 i = 0; i < Segments; ++i)
		DrawLine(CX+Radius*FMath::Cos(i*Step+AngleOffset), CY+Radius*FMath::Sin(i*Step+AngleOffset),
		         CX+Radius*FMath::Cos((i+1)*Step+AngleOffset), CY+Radius*FMath::Sin((i+1)*Step+AngleOffset), Color, Thickness);
}
void APcQDebugHUD::DrawArcHUD(float CX, float CY, float Radius, float Thickness, float StartAngle, float EndAngle, FLinearColor Color, int32 Segments) {
	if (Segments <= 0) return;
	const float Step = (EndAngle-StartAngle)/(float)Segments;
	const float InnerR = Radius-Thickness*0.5f, OuterR = Radius+Thickness*0.5f;
	for (int32 i = 0; i < Segments; ++i) {
		const float A1 = StartAngle+i*Step, A2 = StartAngle+(i+1)*Step;
		DrawLine(CX+InnerR*FMath::Cos(A1),CY+InnerR*FMath::Sin(A1),CX+InnerR*FMath::Cos(A2),CY+InnerR*FMath::Sin(A2),Color,2.f);
		DrawLine(CX+OuterR*FMath::Cos(A1),CY+OuterR*FMath::Sin(A1),CX+OuterR*FMath::Cos(A2),CY+OuterR*FMath::Sin(A2),Color,2.f);
	}
	DrawLine(CX+InnerR*FMath::Cos(StartAngle),CY+InnerR*FMath::Sin(StartAngle),CX+OuterR*FMath::Cos(StartAngle),CY+OuterR*FMath::Sin(StartAngle),Color,2.f);
	DrawLine(CX+InnerR*FMath::Cos(EndAngle),CY+InnerR*FMath::Sin(EndAngle),CX+OuterR*FMath::Cos(EndAngle),CY+OuterR*FMath::Sin(EndAngle),Color,2.f);
}
void APcQDebugHUD::DrawArcFilled(float CX, float CY, float Radius, float Thickness, float StartAngle, float EndAngle, FLinearColor Color, int32 Segments) {
	if (Segments <= 0 || Thickness <= 0.f) return;
	const float Step = (EndAngle-StartAngle)/(float)Segments;
	for (float dr = -Thickness*0.5f; dr <= Thickness*0.5f; dr += 2.2f) {
		const float R = Radius+dr;
		for (int32 i = 0; i < Segments; ++i) {
			const float A1 = StartAngle+i*Step, A2 = StartAngle+(i+1)*Step;
			DrawLine(CX+R*FMath::Cos(A1),CY+R*FMath::Sin(A1),CX+R*FMath::Cos(A2),CY+R*FMath::Sin(A2),Color,2.5f);
		}
	}
}