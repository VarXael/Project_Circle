#include "PcQDebugHUD.h"
#include "PcQPlayerCharacter.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Character.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Project_Circle/MusicSystem/MusicGameplaySystem/PcMusicAnalysisSubsystem.h"

// =============================================================================
//  THREAT API
// =============================================================================

void APcQDebugHUD::RegisterThreat(int32 TimestampMS, const FString& Label, FLinearColor Color)
{
	FPcHudThreatEvent& Ev = ActiveThreats.AddDefaulted_GetRef();
	Ev.TimestampMS = TimestampMS;
	Ev.Label       = Label;
	Ev.Color       = Color;
}

void APcQDebugHUD::PurgeThreat(int32 TimestampMS)
{
	ActiveThreats.RemoveAll([TimestampMS](const FPcHudThreatEvent& Ev)
	{
		return Ev.TimestampMS == TimestampMS;
	});
}

// =============================================================================
//  ARC GEOMETRY
// =============================================================================

APcQDebugHUD::FArcGeom APcQDebugHUD::BuildArcGeom() const
{
	FArcGeom G;
	G.CX = Canvas->SizeX * 0.5f;
	G.CY = Canvas->SizeY + Arc_CenterBelowScreen;

	const float SinLimb  = -Arc_CenterBelowScreen / Arc_Radius;
	const float BaseLimb = FMath::Asin(FMath::Clamp(SinLimb, -1.f, 1.f));

	G.SpawnAngle  = BaseLimb - 0.05f;
	G.BufferAngle = -(PI - FMath::Asin(-SinLimb)) + 0.05f;
	G.StrikeAngle = FMath::Lerp(G.SpawnAngle, G.BufferAngle, Arc_StrikeGatePercent);

	return G;
}

// =============================================================================
//  ENTRY POINT
// =============================================================================

void APcQDebugHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas) return;

	float FlashHard = 0.f, FlashSoft = 0.f;
	float BeatRemainingFraction = 1.f;

	if (UPcMusicAnalysisSubsystem* MusicSub = GetWorld()->GetSubsystem<UPcMusicAnalysisSubsystem>())
	{
		if (MusicSub->IsReadyForPlayback())
		{
			const float IntervalMS    = MusicSub->GetGameplayBeatIntervalMS();
			const int32 CurrentTimeMS = MusicSub->GetCurrentPlaybackTimeMS();
			const int32 NextBeatMS    = MusicSub->GetNextGameplayBeatTimeMS();

			if (IntervalMS > 0.f)
			{
				const float FlashWindow = IntervalMS * Arc_FlashWindowPct;
				const float DistToNext  = FMath::Abs((float)(NextBeatMS - CurrentTimeMS));
				const float DistToPrev  = FMath::Abs(CurrentTimeMS - ((float)NextBeatMS - IntervalMS));
				const float MinDist     = FMath::Min(DistToNext, DistToPrev);
				FlashHard = FMath::Clamp(1.f - (MinDist / (FlashWindow * 0.5f)), 0.f, 1.f);
				FlashSoft = FMath::Clamp(1.f - (MinDist /  FlashWindow),         0.f, 1.f);

				const float TimeUntilBeat = MusicSub->GetTimeUntilNextGameplayBeat();
				BeatRemainingFraction = FMath::Clamp(TimeUntilBeat / (IntervalMS / 1000.f), 0.f, 1.f);

				const FArcGeom G = BuildArcGeom();
				DrawArcMetronome(MusicSub, G, CurrentTimeMS, NextBeatMS, IntervalMS, FlashHard, FlashSoft);
				DrawGlanceBoard(MusicSub, CurrentTimeMS, NextBeatMS, IntervalMS, FlashHard);
			}
		}
	}

	DrawDotCrosshair(BeatRemainingFraction);

	if (APlayerController* PC = GetOwningPlayerController())
	{
		if (ACharacter* Char = Cast<ACharacter>(PC->GetPawn()))
		{
			if (UPcQPlayerMovementComponent* MC = Cast<UPcQPlayerMovementComponent>(Char->GetCharacterMovement()))
			{
				// Bind combo feed if not already bound
				if (!MC->OnComboEvent.IsAlreadyBound(this, &APcQDebugHUD::OnComboEvent))
					MC->OnComboEvent.AddDynamic(this, &APcQDebugHUD::OnComboEvent);

				const float BeatFlash = MC->GetOnBeatFlash();
				const float CX        = Canvas->SizeX * 0.5f;

				// ── Screen edge pulse (beat feedback) ─────────────────────────────
				if (BeatFlash > 0.01f)
				{
					const FLinearColor EdgeCol(0.1f, 0.8f, 1.0f, BeatFlash * 0.15f);
					const float Th = 15.f + BeatFlash * 25.f;
					DrawRect(EdgeCol, 0,                     0,                     Canvas->SizeX, Th);
					DrawRect(EdgeCol, 0,                     Canvas->SizeY - Th,    Canvas->SizeX, Th);
					DrawRect(EdgeCol, 0,                     0,                     Th, Canvas->SizeY);
					DrawRect(EdgeCol, Canvas->SizeX - Th,    0,                     Th, Canvas->SizeY);
				}

				// ── Dash boost bar (centre-bottom) ───────────────────────────────
				DrawDashBoostBar(MC, CX, BeatFlash);

				// ── Ability icons and movement debug ─────────────────────────────
				DrawAbilityBars(MC, PC);
				DrawMovementDebug(MC);
				DrawComboFeed();
			}
		}
	}
}

// =============================================================================
//  SLIDE GAUGE BAR  (replaces the old boost bar)
// =============================================================================

void APcQDebugHUD::DrawDashBoostBar(UPcQPlayerMovementComponent* MC, float CX, float BeatFlash)
{
	if (!Canvas || !GEngine || !MC) return;

	const float DashAlpha = MC->GetDashActiveAlpha();
	const bool  bDashing  = MC->IsDashing();

	const FLinearColor DashCol(1.f, 0.55f, 0.15f);

	const float BW = 240.f;
	const float BH =   8.f;
	const float BX = CX - BW * 0.5f;
	const float BY = Canvas->SizeY - 100.f;

	// Shadow + background
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.75f), BX - 2.f, BY - 2.f, BW + 4.f, BH + 4.f);
	DrawRect(FLinearColor(0.04f, 0.03f, 0.01f, 1.f), BX, BY, BW, BH);

	// Fill
	if (DashAlpha > 0.01f)
	{
		const float FillAlpha = bDashing
			? (0.85f + BeatFlash * 0.15f)
			: (DashAlpha > 0.5f ? 0.60f : 0.40f);
		DrawRect(DashCol * FLinearColor(1, 1, 1, FillAlpha), BX, BY, BW * DashAlpha, BH);
	}

	// Border
	const float BorderA = bDashing ? (0.80f + BeatFlash * 0.20f) : (DashAlpha > 0.01f ? 0.45f : 0.18f);
	DrawRect(DashCol * FLinearColor(1,1,1,BorderA), BX,         BY,          BW, 1.f);
	DrawRect(DashCol * FLinearColor(1,1,1,BorderA), BX,         BY + BH - 1.f, BW, 1.f);
	DrawRect(DashCol * FLinearColor(1,1,1,BorderA), BX,         BY,          1.f, BH);
	DrawRect(DashCol * FLinearColor(1,1,1,BorderA), BX + BW - 1.f, BY,      1.f, BH);

	// Label
	const FString SLabel = bDashing
		? TEXT("DASHING")
		: (DashAlpha > 0.01f ? TEXT("DASH READY") : TEXT("DASH EMPTY"));
	DrawText(SLabel, DashCol * FLinearColor(1, 1, 1, BorderA + 0.15f),
	         BX + 4.f, BY - 13.f, GEngine->GetSmallFont(), 1.f);
}

// =============================================================================
//  ARC METRONOME (unchanged)
// =============================================================================

void APcQDebugHUD::DrawArcMetronome(UPcMusicAnalysisSubsystem* MusicSub, const FArcGeom& G,
                                     int32 CurrentTimeMS, int32 NextBeatMS, float IntervalMS,
                                     float FlashHard, float FlashSoft)
{
	const float BufferTimeMS = IntervalMS * 0.5f;

	// ── 1. TRACK ─────────────────────────────────────────────────────────────
	DrawArcFilled(G.CX, G.CY, Arc_Radius, Arc_Thickness + 10.f,
	              G.SpawnAngle, G.BufferAngle,
	              FLinearColor(0.f, 0.f, 0.f, 0.18f), 60);

	DrawArcFilled(G.CX, G.CY, Arc_Radius, Arc_Thickness,
	              G.StrikeAngle, G.BufferAngle,
	              FLinearColor(0.010f, 0.018f, 0.030f, 0.30f), 20);

	const FLinearColor ActiveColor = FLinearColor::LerpUsingHSV(
		FLinearColor(0.020f, 0.055f, 0.100f, 0.34f),
		FLinearColor(0.016f, 0.090f, 0.160f, 0.42f),
		FlashSoft);
	DrawArcFilled(G.CX, G.CY, Arc_Radius, Arc_Thickness,
	              G.SpawnAngle, G.StrikeAngle, ActiveColor, 60);

	DrawArcHUD(G.CX, G.CY, Arc_Radius - Arc_Thickness * 0.5f, 1.0f,
	           G.SpawnAngle, G.StrikeAngle,
	           FLinearColor(0.4f, 0.85f, 1.f, 0.025f + FlashSoft * 0.10f), 60);

	// ── 2. NOTES ─────────────────────────────────────────────────────────────
	struct FNoteEntry { float TimeDiffMS; bool bIsThreat; int32 ThreatIdx; };
	TArray<FNoteEntry> AllNotes;
	AllNotes.Reserve(Arc_BeatsToShow + 2 + ActiveThreats.Num());

	for (int32 i = -1; i <= Arc_BeatsToShow; ++i)
	{
		const float Diff = ((float)NextBeatMS + i * IntervalMS) - (float)CurrentTimeMS;
		if (Diff >= -BufferTimeMS && Diff <= IntervalMS * (float)Arc_BeatsToShow)
			AllNotes.Add({ Diff, false, -1 });
	}
	for (int32 Ti = 0; Ti < ActiveThreats.Num(); ++Ti)
	{
		const float Diff = (float)(ActiveThreats[Ti].TimestampMS - CurrentTimeMS);
		if (Diff >= -BufferTimeMS && Diff <= IntervalMS * (float)Arc_BeatsToShow)
			AllNotes.Add({ Diff, true, Ti });
	}
	AllNotes.Sort([](const FNoteEntry& A, const FNoteEntry& B) { return A.TimeDiffMS < B.TimeDiffMS; });

	for (const FNoteEntry& Entry : AllNotes)
	{
		const float TimeDiff = Entry.TimeDiffMS;
		const bool  bPassed  = TimeDiff < 0.f;

		float Angle, Alpha;
		if (!bPassed)
		{
			const float Progress = FMath::Clamp(TimeDiff / (IntervalMS * (float)Arc_BeatsToShow), 0.f, 1.f);
			Angle = FMath::Lerp(G.StrikeAngle, G.SpawnAngle, Progress);
			Alpha = FMath::Lerp(1.f, 0.25f, Progress);
		}
		else
		{
			const float Progress = FMath::Clamp(FMath::Abs(TimeDiff) / BufferTimeMS, 0.f, 1.f);
			Angle = FMath::Lerp(G.StrikeAngle, G.BufferAngle, Progress);
			Alpha = 1.f - Progress;
		}
		if (Alpha < 0.02f) continue;

		const float NoteX = G.CX + Arc_Radius * FMath::Cos(Angle);
		const float NoteY = G.CY + Arc_Radius * FMath::Sin(Angle);
		if (NoteY > Canvas->SizeY + 4.f) continue;

		if (Entry.bIsThreat)
		{
			DrawThreatNote(NoteX, NoteY, Alpha, bPassed, ActiveThreats[Entry.ThreatIdx], G);
		}
		else
		{
			if (bPassed)
			{
				DrawCircleHUD(NoteX, NoteY, 6.f,
				              FLinearColor(0.20f, 0.24f, 0.34f, Alpha * 1.25f), 1.4f, 16);
			}
			else
			{
				DrawCircleHUD(NoteX, NoteY, 14.f,
				              FLinearColor(0.f, 0.80f, 1.f, Alpha * 0.16f), 1.f, 16);
				const bool  bIsNext  = TimeDiff < IntervalMS && TimeDiff >= 0.f;
				const float RingSize = bIsNext ? Arc_Thickness * 0.65f : Arc_Thickness * 0.42f;
				DrawCircleHUD(NoteX, NoteY, RingSize,
				              FLinearColor(0.f, 0.82f, 1.f, Alpha * (bIsNext ? 1.f : 0.72f)),
				              bIsNext ? 2.4f : 1.8f, bIsNext ? 24 : 16);
				DrawRect(FLinearColor(0.70f, 0.95f, 1.f, Alpha * (bIsNext ? 1.f : 0.7f)),
				         NoteX - 2.f, NoteY - 2.f, 4.f, 4.f);
			}
		}
	}

	// ── 3. STRIKE GATE ───────────────────────────────────────────────────────
	const float StrikeX = G.CX + Arc_Radius * FMath::Cos(G.StrikeAngle);
	const float StrikeY = G.CY + Arc_Radius * FMath::Sin(G.StrikeAngle);

	if (FlashSoft > 0.01f)
		DrawCircleHUD(StrikeX, StrikeY, 38.f * FlashSoft,
		              FLinearColor(0.1f, 0.9f, 0.85f, 0.06f * FlashSoft), 1.f, 32);
	if (FlashHard > 0.01f)
	{
		DrawCircleHUD(StrikeX, StrikeY, 22.f, FLinearColor(0.15f, 1.f, 0.90f, 0.12f * FlashHard), 2.f, 32);
		DrawCircleHUD(StrikeX, StrikeY, 13.f, FLinearColor(0.20f, 1.f, 0.95f, 0.22f * FlashHard), 2.f, 24);
	}

	const float InnerR = Arc_Radius - Arc_Thickness - 6.f;
	const float OuterR = Arc_Radius + Arc_Thickness + 6.f + 10.f * FlashHard;
	const FLinearColor TickColor = FLinearColor::LerpUsingHSV(
		FLinearColor(0.f,   0.55f, 0.65f, 0.55f),
		FLinearColor(0.16f, 1.f,   1.f,   1.f  ), FlashHard);
	DrawLine(G.CX + InnerR * FMath::Cos(G.StrikeAngle), G.CY + InnerR * FMath::Sin(G.StrikeAngle),
	         G.CX + OuterR * FMath::Cos(G.StrikeAngle), G.CY + OuterR * FMath::Sin(G.StrikeAngle),
	         TickColor, 3.5f + 3.f * FlashHard);

	const float Dh = 5.f + 3.f * FlashHard;
	const FLinearColor DC = FLinearColor::LerpUsingHSV(
		FLinearColor(0.f, 0.5f, 0.55f, 0.70f), FLinearColor(0.16f, 1.f, 1.f, 1.f), FlashHard);
	DrawLine(StrikeX,       StrikeY - Dh, StrikeX + Dh, StrikeY,       DC, 2.f);
	DrawLine(StrikeX + Dh, StrikeY,      StrikeX,       StrikeY + Dh,  DC, 2.f);
	DrawLine(StrikeX,       StrikeY + Dh, StrikeX - Dh, StrikeY,       DC, 2.f);
	DrawLine(StrikeX - Dh, StrikeY,      StrikeX,       StrikeY - Dh,  DC, 2.f);
}

// =============================================================================
//  THREAT NOTE (unchanged)
// =============================================================================

void APcQDebugHUD::DrawThreatNote(float NoteX, float NoteY, float Alpha, bool bPassed,
                                   const FPcHudThreatEvent& Threat, const FArcGeom& G)
{
	if (bPassed)
	{
		const FLinearColor Ghost(0.18f, 0.16f, 0.24f, Alpha * 1.2f);
		const float Hs = 7.f;
		DrawLine(NoteX,      NoteY - Hs, NoteX + Hs, NoteY,      Ghost, 1.5f);
		DrawLine(NoteX + Hs, NoteY,      NoteX,      NoteY + Hs, Ghost, 1.5f);
		DrawLine(NoteX,      NoteY + Hs, NoteX - Hs, NoteY,      Ghost, 1.5f);
		DrawLine(NoteX - Hs, NoteY,      NoteX,      NoteY - Hs, Ghost, 1.5f);
		return;
	}

	const FLinearColor OuterCol = Threat.Color * FLinearColor(1.f, 1.f, 1.f, Alpha);
	const FLinearColor CoreCol  = FLinearColor::LerpUsingHSV(Threat.Color, FLinearColor::White, 0.5f)
	                              * FLinearColor(1.f, 1.f, 1.f, Alpha * 0.85f);

	DrawCircleHUD(NoteX, NoteY, 17.f, Threat.Color * FLinearColor(1.f, 1.f, 1.f, Alpha * 0.16f), 1.f, 16);

	const float Os = 10.f;
	DrawLine(NoteX,       NoteY - Os, NoteX + Os, NoteY,       OuterCol, 2.f);
	DrawLine(NoteX + Os,  NoteY,      NoteX,      NoteY + Os,  OuterCol, 2.f);
	DrawLine(NoteX,       NoteY + Os, NoteX - Os, NoteY,       OuterCol, 2.f);
	DrawLine(NoteX - Os,  NoteY,      NoteX,      NoteY - Os,  OuterCol, 2.f);

	const float Is = 5.f;
	DrawLine(NoteX,       NoteY - Is, NoteX + Is, NoteY,       CoreCol, 5.f);
	DrawLine(NoteX + Is,  NoteY,      NoteX,      NoteY + Is,  CoreCol, 5.f);
	DrawLine(NoteX,       NoteY + Is, NoteX - Is, NoteY,       CoreCol, 5.f);
	DrawLine(NoteX - Is,  NoteY,      NoteX,      NoteY - Is,  CoreCol, 5.f);

	if (Alpha > 0.45f && GEngine)
	{
		const float DirX   = NoteX - G.CX;
		const float DirY   = NoteY - G.CY;
		const float DirLen = FMath::Sqrt(DirX * DirX + DirY * DirY);
		DrawText(Threat.Label,
		         Threat.Color * FLinearColor(1.f, 1.f, 1.f, Alpha * 0.80f),
		         NoteX + (DirLen > 0.f ? DirX / DirLen : 0.f) * 20.f - 12.f,
		         NoteY + (DirLen > 0.f ? DirY / DirLen : -1.f) * 20.f - 5.f,
		         GEngine->GetSmallFont(), 1.f);
	}
}

// =============================================================================
//  CROSSHAIR (unchanged)
// =============================================================================

void APcQDebugHUD::DrawDotCrosshair(float BeatRemainingFraction)
{
	if (!Canvas) return;
	const float CX = Canvas->SizeX * 0.5f;
	const float CY = Canvas->SizeY * 0.5f;

	const float FlashA = FMath::Clamp(1.f - BeatRemainingFraction / 0.15f, 0.f, 1.f);
	const float AW = CrosshairChevronWidth;
	const float AH = CrosshairChevronHeight;
	const float EXTRA_H = 2.0f;
	const float THICK   = 3.0f;

	auto DrawChev = [&](float TipX, bool bLeft, float Alpha, FLinearColor Col)
	{
		const float BackX = bLeft ? TipX + AW : TipX - AW;
		const float H     = AH + EXTRA_H;
		const FLinearColor Sh(0.f, 0.f, 0.f, FMath::Min(0.5f, Alpha * 0.8f));
		DrawLine(BackX, CY - H, TipX, CY, Sh, THICK + 2.5f);
		DrawLine(TipX,  CY,     BackX, CY + H, Sh, THICK + 2.5f);
		FLinearColor FinalCol = Col; FinalCol.A *= Alpha;
		DrawLine(BackX, CY - H, TipX, CY, FinalCol, THICK);
		DrawLine(TipX,  CY,     BackX, CY + H, FinalCol, THICK);
	};

	const float MaxBeats = FMath::Max(1.0f, (float)CrosshairBeatsToShow);
	for (int32 i = CrosshairBeatsToShow - 1; i >= 0; --i)
	{
		const float d              = BeatRemainingFraction + (float)i;
		const float DistNormalized = FMath::Clamp(d / MaxBeats, 0.0f, 1.0f);
		const float Alpha          = FMath::Pow(1.0f - DistNormalized, 2.0f);
		if (Alpha < 0.01f) continue;
		const float Dist = CrosshairGateDist + d * CrosshairBeatStep;
		FLinearColor NoteCol(0.6f, 0.6f, 0.6f, Alpha);
		DrawChev(CX - Dist, true, Alpha, NoteCol);
		DrawChev(CX + Dist, false, Alpha, NoteCol);
	}

	const FLinearColor GateResting(0.05f, 0.05f, 0.05f, 0.45f);
	const FLinearColor GateFlash(1.0f, 1.0f, 1.0f, 1.0f);
	const FLinearColor GateCol = GateResting + (GateFlash - GateResting) * FlashA;
	DrawChev(CX - CrosshairGateDist, true,  1.f, GateCol);
	DrawChev(CX + CrosshairGateDist, false, 1.f, GateCol);

	const float DR = DotSize;
	const FLinearColor DotResting(0.05f, 0.75f, 1.0f, 0.8f);
	const FLinearColor DotFlash(1.0f, 1.0f, 1.0f, 1.0f);
	const FLinearColor DotCol = DotResting + (DotFlash - DotResting) * FlashA;
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.8f), CX - DR - 1.f, CY - DR - 1.f, (DR + 1.f) * 2.f, (DR + 1.f) * 2.f);
	DrawRect(DotCol, CX - DR, CY - DR, DR * 2.f, DR * 2.f);
}

// =============================================================================
//  GLANCE BOARD (unchanged)
// =============================================================================

void APcQDebugHUD::DrawGlanceBoard(UPcMusicAnalysisSubsystem* MusicSub,
                                    int32 CurrentTimeMS, int32 NextBeatMS, float IntervalMS,
                                    float FlashHard)
{
	if (!Canvas) return;

	const float StrikeY      = Canvas->SizeY * GlanceBoard_ScreenYPercent;
	const float TopY         = StrikeY - GlanceBoard_Height;
	const float PixPerBeat   = GlanceBoard_Height / (float)GlanceBoard_BeatsToShow;
	const float PlayerTrackX = GlanceBoard_XOffset;
	const float EnemyTrackX  = GlanceBoard_XOffset + GlanceBoard_TrackSpacing;

	const float PanelPad = 14.f;
	const float PanelW   = GlanceBoard_TrackSpacing + PanelPad * 2.f;
	DrawRect(FLinearColor(0.01f, 0.02f, 0.05f, 0.70f),
	         PlayerTrackX - PanelPad, TopY - PanelPad, PanelW, GlanceBoard_Height + PanelPad * 2.f);
	DrawLine(PlayerTrackX - PanelPad, TopY  - PanelPad,
	         PlayerTrackX - PanelPad, StrikeY + PanelPad,
	         FLinearColor(0.f, 0.55f, 0.75f, 0.22f), 1.f);

	if (GEngine)
	{
		DrawText(TEXT("BEAT"), FLinearColor(0.f, 0.70f, 0.85f, 0.50f),
		         PlayerTrackX - 6.f, TopY - 14.f, GEngine->GetSmallFont(), 1.f);
		DrawText(TEXT("THRT"), FLinearColor(1.f, 0.20f, 0.32f, 0.50f),
		         EnemyTrackX  - 6.f, TopY - 14.f, GEngine->GetSmallFont(), 1.f);
	}

	DrawLine(PlayerTrackX, TopY, PlayerTrackX, StrikeY, FLinearColor(0.f,  0.55f, 0.75f, 0.15f), 1.f);
	DrawLine(EnemyTrackX,  TopY, EnemyTrackX,  StrikeY, FLinearColor(0.8f, 0.15f, 0.25f, 0.15f), 1.f);

	const FLinearColor StrikeLineColor = FLinearColor::LerpUsingHSV(
		FLinearColor(0.f, 0.45f, 0.55f, 0.35f), FLinearColor(0.16f, 1.f, 1.f, 0.92f), FlashHard);
	DrawLine(PlayerTrackX - PanelPad, StrikeY, PlayerTrackX - PanelPad + PanelW, StrikeY,
	         StrikeLineColor, 2.f + 1.5f * FlashHard);

	const float SqH = 5.f + 2.f * FlashHard;
	DrawRect(StrikeLineColor, PlayerTrackX - SqH * 0.5f, StrikeY - SqH * 0.5f, SqH, SqH);
	DrawRect(FLinearColor(1.f, 0.25f, 0.20f, 0.45f + FlashHard * 0.55f),
	         EnemyTrackX  - SqH * 0.5f, StrikeY - SqH * 0.5f, SqH, SqH);

	for (int32 i = -1; i <= GlanceBoard_BeatsToShow; ++i)
	{
		const float TimeDiffMS = ((float)NextBeatMS + i * IntervalMS) - (float)CurrentTimeMS;
		if (TimeDiffMS < -120.f) continue;
		const float NoteY = StrikeY - (TimeDiffMS / IntervalMS) * PixPerBeat;
		if (NoteY < TopY || NoteY > StrikeY + 8.f) continue;

		const float Alpha = (TimeDiffMS >= 0.f)
			? FMath::Clamp(1.f - (TimeDiffMS / (IntervalMS * (float)GlanceBoard_BeatsToShow)), 0.f, 1.f)
			: FMath::Clamp(1.f - FMath::Abs(TimeDiffMS) / 120.f, 0.f, 1.f);

		const bool  bIsNext = (i == 0 && TimeDiffMS >= 0.f);
		const float DashW   = bIsNext ? 18.f : 14.f;
		const float DashH   = bIsNext ?  3.f :  2.f;
		DrawRect(FLinearColor(0.f, 0.82f, 1.f, Alpha * (bIsNext ? 0.92f : 0.65f)),
		         PlayerTrackX - DashW * 0.5f, NoteY - DashH * 0.5f, DashW, DashH);
	}

	const float LookaheadSec = (IntervalMS * (float)GlanceBoard_BeatsToShow) / 1000.f;
	for (const FPcRuntimeEvent& Note : MusicSub->GetUpcomingNotes(LookaheadSec))
	{
		const float TimeDiffMS = (float)(Note.TimestampMS - CurrentTimeMS);
		if (TimeDiffMS < -120.f) continue;
		const float NoteY = StrikeY - (TimeDiffMS / IntervalMS) * PixPerBeat;
		if (NoteY < TopY || NoteY > StrikeY + 8.f) continue;

		const float Alpha = (TimeDiffMS >= 0.f)
			? FMath::Clamp(1.f - TimeDiffMS / (IntervalMS * (float)GlanceBoard_BeatsToShow), 0.f, 1.f)
			: FMath::Clamp(1.f - FMath::Abs(TimeDiffMS) / 120.f, 0.f, 1.f);

		const float Dr = 5.f;
		const FLinearColor EC(1.f, 0.15f, 0.25f, Alpha);
		DrawLine(EnemyTrackX,      NoteY - Dr, EnemyTrackX + Dr, NoteY,      EC, 1.5f);
		DrawLine(EnemyTrackX + Dr, NoteY,      EnemyTrackX,      NoteY + Dr, EC, 1.5f);
		DrawLine(EnemyTrackX,      NoteY + Dr, EnemyTrackX - Dr, NoteY,      EC, 1.5f);
		DrawLine(EnemyTrackX - Dr, NoteY,      EnemyTrackX,      NoteY - Dr, EC, 1.5f);
	}

	for (const FPcHudThreatEvent& Threat : ActiveThreats)
	{
		const float TimeDiffMS = (float)(Threat.TimestampMS - CurrentTimeMS);
		if (TimeDiffMS < -120.f) continue;
		const float NoteY = StrikeY - (TimeDiffMS / IntervalMS) * PixPerBeat;
		if (NoteY < TopY || NoteY > StrikeY + 8.f) continue;

		const float Alpha = (TimeDiffMS >= 0.f)
			? FMath::Clamp(1.f - TimeDiffMS / (IntervalMS * (float)GlanceBoard_BeatsToShow), 0.f, 1.f)
			: FMath::Clamp(1.f - FMath::Abs(TimeDiffMS) / 120.f, 0.f, 1.f);

		const float Dr = 6.f;
		const FLinearColor TC = Threat.Color * FLinearColor(1.f, 1.f, 1.f, Alpha);
		DrawLine(EnemyTrackX,      NoteY - Dr, EnemyTrackX + Dr, NoteY,      TC, 2.f);
		DrawLine(EnemyTrackX + Dr, NoteY,      EnemyTrackX,      NoteY + Dr, TC, 2.f);
		DrawLine(EnemyTrackX,      NoteY + Dr, EnemyTrackX - Dr, NoteY,      TC, 2.f);
		DrawLine(EnemyTrackX - Dr, NoteY,      EnemyTrackX,      NoteY - Dr, TC, 2.f);
	}
}

// =============================================================================
//  MOVEMENT DEBUG PANEL  (replaces DrawBhopDebug)
// =============================================================================

void APcQDebugHUD::DrawMovementDebug(UPcQPlayerMovementComponent* MC)
{
	if (!MC || !GEngine) return;

	const float PanelX = 30.f;
	float       PanelY = 30.f;
	const float LineH  = 22.f;

	auto Row = [&](const FString& Label, const FString& Value, FLinearColor Color = FLinearColor::White)
	{
		DrawText(Label + TEXT("  ") + Value, Color, PanelX, PanelY, GEngine->GetSmallFont(), 1.f);
		PanelY += LineH;
	};

	// ── State ─────────────────────────────────────────────────────────────────
	const EPlayerMovementState State = MC->GetMovementState();
	FString StateStr;
	switch (State)
	{
	case EPlayerMovementState::Grounded:       StateStr = TEXT("GROUNDED");     break;
	case EPlayerMovementState::InAir:          StateStr = TEXT("IN AIR");       break;
	case EPlayerMovementState::GroundPounding: StateStr = TEXT("GROUND POUND"); break;
	case EPlayerMovementState::Dashing:        StateStr = TEXT("DASHING");      break;
	default:                                   StateStr = TEXT("UNKNOWN");      break;
	}
	Row(TEXT("STATE:"), StateStr, GetStateColor(State));

	// ── Double jump ───────────────────────────────────────────────────────────
	const bool bDJ = MC->HasDoubleJump();
	Row(TEXT("DJ:"), bDJ ? TEXT("READY") : TEXT("CD"),
	    bDJ ? FLinearColor::Green : FLinearColor(0.5f, 0.5f, 0.5f));

	if (!bDJ)
	{
		const float DJAlpha = MC->GetDJCooldownAlpha();
		DrawRect(FLinearColor(0.05f, 0.05f, 0.05f, 0.85f), PanelX, PanelY, 80.f, 5.f);
		DrawRect(FLinearColor(0.27f, 0.67f, 1.f, 0.8f),    PanelX, PanelY, 80.f * (1.f - DJAlpha), 5.f);
		PanelY += 10.f;
	}

	// ── Dash active ───────────────────────────────────────────────────────────
	const float DashAlpha = MC->GetDashActiveAlpha();
	Row(TEXT("DASH:"), FString::Printf(TEXT("%.0f%%"), DashAlpha * 100.f),
	    DashAlpha > 0.3f ? FLinearColor(1.f, 0.55f, 0.15f) : FLinearColor(0.55f, 0.25f, 0.05f));

	DrawRect(FLinearColor(0.05f, 0.05f, 0.05f, 0.85f), PanelX, PanelY, 80.f, 5.f);
	DrawRect(FLinearColor(1.f, 0.55f, 0.15f, 0.8f),    PanelX, PanelY, 80.f * DashAlpha, 5.f);
	PanelY += 12.f;

	// ── BPM / speed ──────────────────────────────────────────────────────────
	if (UPcMusicAnalysisSubsystem* Sub = GetWorld()->GetSubsystem<UPcMusicAnalysisSubsystem>())
	{
		Row(TEXT("BPM:"),     FString::Printf(TEXT("%.1f"), Sub->GetCurrentGameplayBPM()), FLinearColor::Yellow);
		Row(TEXT("MAX SPD:"), FString::Printf(TEXT("%.0f"), MC->MaxWalkSpeed));
	}

	const float HSpeed = MC->GetHorizontalSpeed();
	const bool  bOver  = HSpeed > MC->MaxWalkSpeed * 1.05f;
	Row(TEXT("SPEED:"), FString::Printf(TEXT("%.0f u/s"), HSpeed),
	    bOver ? FLinearColor(1.f, 0.45f, 0.f) : FLinearColor::White);

	DrawRect(FLinearColor(0.05f, 0.05f, 0.05f, 0.85f), PanelX, PanelY, 160.f, 6.f);
	DrawRect(bOver ? FLinearColor(1.f, 0.45f, 0.f) : FLinearColor::White, PanelX, PanelY,
	         160.f * FMath::Clamp(HSpeed / (MC->MaxWalkSpeed * 3.f), 0.f, 1.f), 6.f);
	PanelY += 14.f;

	Row(TEXT("V SPD:"), FString::Printf(TEXT("%.0f u/s"), MC->Velocity.Z),
	    MC->Velocity.Z < -10.f ? FLinearColor(0.6f, 0.6f, 1.f) : FLinearColor::White);

	Row(TEXT("GRND:"), MC->IsMovingOnGround() ? TEXT("YES") : TEXT("NO"),
	    MC->IsMovingOnGround() ? FLinearColor::Green : FLinearColor(0.6f, 0.6f, 1.f));
}

// =============================================================================
//  COMBO FEED (unchanged)
// =============================================================================

void APcQDebugHUD::OnComboEvent(const FString& Label, FLinearColor Color)
{
	if (!GetWorld()) return;
	FPcComboFeedEntry E;
	E.Label  = Label;
	E.Color  = Color;
	E.BornAt = GetWorld()->GetTimeSeconds();
	ComboFeed.Add(E);
	while (ComboFeed.Num() > ComboFeed_MaxEntries)
		ComboFeed.RemoveAt(0);
}

void APcQDebugHUD::DrawComboFeed()
{
	if (!Canvas || !GEngine || !GetWorld()) return;

	const float Now     = GetWorld()->GetTimeSeconds();
	const float FX      = 22.f;
	const float FBY     = Canvas->SizeY * 0.82f;
	const float LineH   = 24.f;
	const float HalfDur = ComboFeed_FadeDuration * 0.55f;

	ComboFeed.RemoveAll([&](const FPcComboFeedEntry& E)
	{
		return (Now - E.BornAt) >= ComboFeed_FadeDuration;
	});

	for (int32 i = 0; i < ComboFeed.Num(); ++i)
	{
		const FPcComboFeedEntry& E     = ComboFeed[ComboFeed.Num() - 1 - i];
		const float              Age   = Now - E.BornAt;
		const float              Alpha = Age < HalfDur
			? 1.f
			: FMath::Clamp(1.f - (Age - HalfDur) / (ComboFeed_FadeDuration - HalfDur), 0.f, 1.f);
		if (Alpha < 0.02f) continue;

		const float Y = FBY - i * LineH;
		DrawRect(E.Color * FLinearColor(1,1,1,Alpha * 0.88f), FX, Y - 14.f, 3.f, 18.f);
		DrawText(E.Label, FLinearColor(0,0,0,Alpha * 0.65f), FX + 9.f, Y,     GEngine->GetSmallFont(), 1.f);
		DrawText(E.Label, E.Color * FLinearColor(1,1,1,Alpha), FX + 8.f, Y - 1.f, GEngine->GetSmallFont(), 1.f);
	}
}

// =============================================================================
//  STATE COLOR  (updated for new enum)
// =============================================================================

FLinearColor APcQDebugHUD::GetStateColor(EPlayerMovementState State) const
{
	switch (State)
	{
	case EPlayerMovementState::GroundPounding: return FLinearColor(1.f, 0.30f, 0.10f);
	case EPlayerMovementState::Dashing:        return FLinearColor(1.f, 0.55f, 0.15f);
	case EPlayerMovementState::InAir:          return FLinearColor(0.27f, 0.67f, 1.f);
	default:                                   return FLinearColor::Green; // Grounded
	}
}

// =============================================================================
//  ABILITY BARS  (DJUMP + FIRE;  dash boost bar has its own bar above)
// =============================================================================

void APcQDebugHUD::DrawAbilityBars(UPcQPlayerMovementComponent* MC, APlayerController* PC)
{
	if (!Canvas || !GEngine) return;

	const float IconSz  = 52.f;
	const float IconGap = 10.f;
	const float TotalW  = IconSz * 2.f + IconGap;
	const float StartX  = (Canvas->SizeX - TotalW) * 0.5f;
	const float IconY   = Canvas->SizeY - IconSz - 20.f;
	const float BeatFlash = MC ? MC->GetOnBeatFlash() : 0.f;

	// ── Gather values ─────────────────────────────────────────────────────────
	float PistolCoolAlpha = 0.f;
	if (APcQPlayerCharacter* Ch = PC ? Cast<APcQPlayerCharacter>(PC->GetPawn()) : nullptr)
		PistolCoolAlpha = Ch->GetPistolCooldownAlpha();

	const float DJCoolAlpha = MC ? MC->GetDJCooldownAlpha() : 0.f;
	const bool  bDJReady    = MC ? MC->HasDoubleJump() : false;

	// ── Icon descriptor ───────────────────────────────────────────────────────
	struct FIcon
	{
		FString      Label;
		FLinearColor Col;
		float        Fill;    // 0 = empty/on CD, 1 = full/ready
		bool         bActive;
	};

	const FIcon Icons[2] =
	{
		// Double jump: full when ready, drains toward 0 while on CD
		{ TEXT("DJUMP"), FLinearColor(0.18f, 0.65f, 1.f),
		  bDJReady ? 1.f : (1.f - DJCoolAlpha), false },

		// Fire: full when ready, drains while on cooldown
		{ TEXT("FIRE"),  FLinearColor(0.9f, 0.18f, 0.28f),
		  1.f - PistolCoolAlpha, false },
	};

	// ── Draw each icon ────────────────────────────────────────────────────────
	for (int32 i = 0; i < 2; ++i)
	{
		const FIcon& Ic   = Icons[i];
		const float  IX   = StartX + i * (IconSz + IconGap);
		const bool   bRdy = Ic.Fill >= 1.f && !Ic.bActive;

		// Shadow
		DrawRect(FLinearColor(0,0,0,0.82f), IX - 3.f, IconY - 3.f, IconSz + 6.f, IconSz + 6.f);
		// Background
		DrawRect(FLinearColor(0.03f, 0.04f, 0.08f, 1.f), IX, IconY, IconSz, IconSz);

		// Fill — rises from the bottom
		const float FillH     = IconSz * Ic.Fill;
		const float FillY     = IconY  + IconSz - FillH;
		const float FillAlpha = bRdy
			? (0.75f + BeatFlash * 0.20f)
			: (Ic.bActive ? 0.88f : 0.28f);
		if (FillH > 0.5f)
			DrawRect(Ic.Col * FLinearColor(1,1,1,FillAlpha), IX, FillY, IconSz, FillH);

		// Border
		const float BorderA = bRdy ? (0.85f + BeatFlash * 0.15f) : (Ic.bActive ? 0.70f : 0.22f);
		DrawRect(Ic.Col * FLinearColor(1,1,1,BorderA), IX,            IconY,             IconSz, 2.f);
		DrawRect(Ic.Col * FLinearColor(1,1,1,BorderA), IX,            IconY+IconSz-2.f,  IconSz, 2.f);
		DrawRect(Ic.Col * FLinearColor(1,1,1,BorderA), IX,            IconY,             2.f, IconSz);
		DrawRect(Ic.Col * FLinearColor(1,1,1,BorderA), IX+IconSz-2.f, IconY,             2.f, IconSz);

		// Beat flash border
		if (bRdy && BeatFlash > 0.05f)
		{
			DrawRect(Ic.Col * FLinearColor(1,1,1,BeatFlash * 0.90f), IX,            IconY,            IconSz, 2.f);
			DrawRect(Ic.Col * FLinearColor(1,1,1,BeatFlash * 0.90f), IX,            IconY+IconSz-2.f, IconSz, 2.f);
			DrawRect(Ic.Col * FLinearColor(1,1,1,BeatFlash * 0.90f), IX,            IconY,            2.f, IconSz);
			DrawRect(Ic.Col * FLinearColor(1,1,1,BeatFlash * 0.90f), IX+IconSz-2.f, IconY,            2.f, IconSz);
		}

		// Label
		const FLinearColor TextCol = (bRdy || Ic.bActive)
			? Ic.Col
			: FLinearColor(0.40f, 0.45f, 0.55f, 0.90f);
		DrawText(Ic.Label, TextCol, IX + 4.f, IconY + 4.f, GEngine->GetSmallFont(), 1.f);

		// Status text in centre when on cooldown
		if (!bRdy && !Ic.bActive)
			DrawText(TEXT("CD"), TextCol, IX + 10.f, IconY + IconSz * 0.5f - 4.f, GEngine->GetSmallFont(), 1.f);
	}
}

// =============================================================================
//  PRIMITIVES (unchanged)
// =============================================================================

void APcQDebugHUD::DrawCircleHUD(float CX, float CY, float Radius, FLinearColor Color,
                                  float Thickness, int32 Segments, float AngleOffset)
{
	if (Segments <= 0) return;
	const float Step = 2.f * PI / (float)Segments;
	for (int32 i = 0; i < Segments; ++i)
	{
		DrawLine(CX + Radius * FMath::Cos(i       * Step + AngleOffset),
		         CY + Radius * FMath::Sin(i       * Step + AngleOffset),
		         CX + Radius * FMath::Cos((i + 1) * Step + AngleOffset),
		         CY + Radius * FMath::Sin((i + 1) * Step + AngleOffset),
		         Color, Thickness);
	}
}

void APcQDebugHUD::DrawArcHUD(float CX, float CY, float Radius, float Thickness,
                               float StartAngle, float EndAngle, FLinearColor Color, int32 Segments)
{
	if (Segments <= 0) return;
	const float AngleStep = (EndAngle - StartAngle) / (float)Segments;
	const float InnerR    = Radius - Thickness * 0.5f;
	const float OuterR    = Radius + Thickness * 0.5f;
	for (int32 i = 0; i < Segments; ++i)
	{
		const float A1 = StartAngle + i       * AngleStep;
		const float A2 = StartAngle + (i + 1) * AngleStep;
		DrawLine(CX + InnerR * FMath::Cos(A1), CY + InnerR * FMath::Sin(A1),
		         CX + InnerR * FMath::Cos(A2), CY + InnerR * FMath::Sin(A2), Color, 2.f);
		DrawLine(CX + OuterR * FMath::Cos(A1), CY + OuterR * FMath::Sin(A1),
		         CX + OuterR * FMath::Cos(A2), CY + OuterR * FMath::Sin(A2), Color, 2.f);
	}
	DrawLine(CX + InnerR * FMath::Cos(StartAngle), CY + InnerR * FMath::Sin(StartAngle),
	         CX + OuterR * FMath::Cos(StartAngle), CY + OuterR * FMath::Sin(StartAngle), Color, 2.f);
	DrawLine(CX + InnerR * FMath::Cos(EndAngle),   CY + InnerR * FMath::Sin(EndAngle),
	         CX + OuterR * FMath::Cos(EndAngle),   CY + OuterR * FMath::Sin(EndAngle),   Color, 2.f);
}

void APcQDebugHUD::DrawArcFilled(float CX, float CY, float Radius, float Thickness,
                                  float StartAngle, float EndAngle, FLinearColor Color, int32 Segments)
{
	if (Segments <= 0 || Thickness <= 0.f) return;
	const float AngleStep = (EndAngle - StartAngle) / (float)Segments;
	const float HalfThick = Thickness * 0.5f;
	for (float dr = -HalfThick; dr <= HalfThick; dr += 2.2f)
	{
		const float R = Radius + dr;
		for (int32 i = 0; i < Segments; ++i)
		{
			const float A1 = StartAngle + i       * AngleStep;
			const float A2 = StartAngle + (i + 1) * AngleStep;
			DrawLine(CX + R * FMath::Cos(A1), CY + R * FMath::Sin(A1),
			         CX + R * FMath::Cos(A2), CY + R * FMath::Sin(A2), Color, 2.5f);
		}
	}
}