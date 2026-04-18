#include "PcQDebugHUD.h"
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
//  ARCH GEOMETRY
//
//  The arc centre sits Arc_CenterBelowScreen pixels below the screen bottom.
//  The limb angles (SpawnAngle / BufferAngle) are the points where the circle
//  would intersect y = Canvas->SizeY, i.e. the screen edge.  Notes outside
//  those angles are below the screen and simply are not drawn.
//
//  In canvas angle convention:  0 = right,  -PI/2 = up,  ±PI = left.
//  The arc travels counterclockwise: spawn (right-ish) → top → buffer (left-ish).
// =============================================================================

APcQDebugHUD::FArcGeom APcQDebugHUD::BuildArcGeom() const
{
	FArcGeom G;
	G.CX = Canvas->SizeX * 0.5f;
	G.CY = Canvas->SizeY + Arc_CenterBelowScreen;   // well below the screen

	// sin(limbAngle) = -D/R  where D = Arc_CenterBelowScreen
	// This is the angle where the circle crosses y = Canvas->SizeY.
	const float SinLimb = -Arc_CenterBelowScreen / Arc_Radius;
	const float BaseLimb = FMath::Asin(FMath::Clamp(SinLimb, -1.f, 1.f));

	G.SpawnAngle  = BaseLimb - 0.05f;                      // right limb, nudge just below screen
	G.BufferAngle = -(PI - FMath::Asin(-SinLimb)) + 0.05f; // left limb,  nudge just below screen
	G.StrikeAngle = -(PI * 0.5f + Arc_StrikeOffsetLeft);   // gate: slightly left of 12-o-clock

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
				// Shared flash values — computed once, used by arc gate and glance board.
				const float FlashWindow = IntervalMS * Arc_FlashWindowPct;
				const float DistToNext  = FMath::Abs((float)(NextBeatMS - CurrentTimeMS));
				const float DistToPrev  = FMath::Abs(CurrentTimeMS - ((float)NextBeatMS - IntervalMS));
				const float MinDist     = FMath::Min(DistToNext, DistToPrev);
				FlashHard = FMath::Clamp(1.f - (MinDist / (FlashWindow * 0.5f)), 0.f, 1.f);
				FlashSoft = FMath::Clamp(1.f - (MinDist /  FlashWindow),         0.f, 1.f);

				// Beat-remaining fraction for the crosshair sweep ring.
				// 1.0 = just after beat (full circle drawn), 0.0 = beat about to fire (arc = 0).
				const float TimeUntilBeat = MusicSub->GetTimeUntilNextGameplayBeat();
				BeatRemainingFraction = FMath::Clamp(TimeUntilBeat / (IntervalMS / 1000.f), 0.f, 1.f);

				const FArcGeom G = BuildArcGeom();
				DrawArcMetronome(MusicSub, G, CurrentTimeMS, NextBeatMS, IntervalMS, FlashHard, FlashSoft);
				DrawGlanceBoard(MusicSub, CurrentTimeMS, NextBeatMS, IntervalMS, FlashHard);
			}
		}
	}

	// Crosshair on top — never obscured by arc or notes.
	DrawDotCrosshair(BeatRemainingFraction);

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
}

// =============================================================================
//  ARC METRONOME (VISOR ARCH)
//
//  Because Arc_CenterBelowScreen ≈ Arc_Radius, the arc's limbs dip below the
//  screen edge, so the player sees only the shallow top portion — a wide visor.
//
//  The track is two distinct filled bands:
//    Active  (spawn → gate)  dark steel-blue, warms cyan near the beat.
//    Past    (gate → buffer) near-black void: notes here are already done.
//
//  Any note whose screen Y exceeds Canvas->SizeY is skipped silently — it is
//  below the screen and the player would never see it anyway.
// =============================================================================

void APcQDebugHUD::DrawArcMetronome(UPcMusicAnalysisSubsystem* MusicSub, const FArcGeom& G,
                                     int32 CurrentTimeMS, int32 NextBeatMS, float IntervalMS,
                                     float FlashHard, float FlashSoft)
{
	const float BufferTimeMS = IntervalMS * 0.5f;

	// ── 1. TRACK ────────────────────────────────────────────────────────────

	// Drop-shadow behind the full arc for contrast against the world geometry.
	DrawArcFilled(G.CX, G.CY, Arc_Radius, Arc_Thickness + 16.f,
	              G.SpawnAngle, G.BufferAngle,
	              FLinearColor(0.f, 0.f, 0.f, 0.50f), 60);

	// Past zone: dark void, notes here are greyed out.
	DrawArcFilled(G.CX, G.CY, Arc_Radius, Arc_Thickness,
	              G.StrikeAngle, G.BufferAngle,
	              FLinearColor(0.018f, 0.035f, 0.055f, 0.88f), 20);

	// Active zone: steel-blue, warms toward cyan as the beat approaches.
	const FLinearColor ActiveColor = FLinearColor::LerpUsingHSV(
		FLinearColor(0.022f, 0.064f, 0.115f, 0.90f),
		FLinearColor(0.018f, 0.106f, 0.185f, 0.94f),
		FlashSoft
	);
	DrawArcFilled(G.CX, G.CY, Arc_Radius, Arc_Thickness,
	              G.SpawnAngle, G.StrikeAngle, ActiveColor, 60);

	// Inner-surface glow hairline on the active zone only.
	DrawArcHUD(G.CX, G.CY, Arc_Radius - Arc_Thickness * 0.5f, 1.4f,
	           G.SpawnAngle, G.StrikeAngle,
	           FLinearColor(0.4f, 0.85f, 1.f, 0.06f + FlashSoft * 0.26f), 60);

	// ── 2. NOTES ────────────────────────────────────────────────────────────
	// Build a unified list of beat + threat entries, sort past-first so
	// approaching notes always render on top.

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

		// Map time offset → angle along the arch.
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

		// Skip anything that has dipped below the visible screen edge.
		if (NoteY > Canvas->SizeY + 4.f) continue;

		if (Entry.bIsThreat)
		{
			DrawThreatNote(NoteX, NoteY, Alpha, bPassed, ActiveThreats[Entry.ThreatIdx], G);
		}
		else
		{
			if (bPassed)
			{
				// Grey ghost circle — matches the "past zone" darkness.
				DrawCircleHUD(NoteX, NoteY, 6.f,
				              FLinearColor(0.20f, 0.24f, 0.34f, Alpha * 1.25f), 1.4f, 16);
			}
			else
			{
				// Glow corona behind the ring.
				DrawCircleHUD(NoteX, NoteY, 14.f,
				              FLinearColor(0.f, 0.80f, 1.f, Alpha * 0.16f), 1.f, 16);
				// Main ring — next upcoming beat is thicker + brighter.
				const bool  bIsNext  = TimeDiff < IntervalMS && TimeDiff >= 0.f;
				const float RingSize = bIsNext ? Arc_Thickness * 0.65f : Arc_Thickness * 0.42f;
				DrawCircleHUD(NoteX, NoteY, RingSize,
				              FLinearColor(0.f, 0.82f, 1.f, Alpha * (bIsNext ? 1.f : 0.72f)),
				              bIsNext ? 2.4f : 1.8f, bIsNext ? 24 : 16);
				// Bright centre dot.
				DrawRect(FLinearColor(0.70f, 0.95f, 1.f, Alpha * (bIsNext ? 1.f : 0.7f)),
				         NoteX - 2.f, NoteY - 2.f, 4.f, 4.f);
			}
		}
	}

	// ── 3. STRIKE GATE ──────────────────────────────────────────────────────

	const float StrikeX = G.CX + Arc_Radius * FMath::Cos(G.StrikeAngle);
	const float StrikeY = G.CY + Arc_Radius * FMath::Sin(G.StrikeAngle);

	// Diffuse pre-glow corona (soft, starts building a few frames early).
	if (FlashSoft > 0.01f)
	{
		DrawCircleHUD(StrikeX, StrikeY, 38.f * FlashSoft,
		              FLinearColor(0.1f, 0.9f, 0.85f, 0.06f * FlashSoft), 1.f, 32);
	}
	// Tighter rings — only visible on the hard flash frame.
	if (FlashHard > 0.01f)
	{
		DrawCircleHUD(StrikeX, StrikeY, 22.f, FLinearColor(0.15f, 1.f, 0.90f, 0.12f * FlashHard), 2.f, 32);
		DrawCircleHUD(StrikeX, StrikeY, 13.f, FLinearColor(0.20f, 1.f, 0.95f, 0.22f * FlashHard), 2.f, 24);
	}

	// Radial tick mark that grows outward on the beat.
	const float InnerR = Arc_Radius - Arc_Thickness - 6.f;
	const float OuterR = Arc_Radius + Arc_Thickness + 6.f + 10.f * FlashHard;
	const FLinearColor TickColor = FLinearColor::LerpUsingHSV(
		FLinearColor(0.f,   0.55f, 0.65f, 0.55f),
		FLinearColor(0.16f, 1.f,   1.f,   1.f  ), FlashHard);
	DrawLine(G.CX + InnerR * FMath::Cos(G.StrikeAngle), G.CY + InnerR * FMath::Sin(G.StrikeAngle),
	         G.CX + OuterR * FMath::Cos(G.StrikeAngle), G.CY + OuterR * FMath::Sin(G.StrikeAngle),
	         TickColor, 3.5f + 3.f * FlashHard);

	// Diamond landmark at the gate.
	const float Dh = 5.f + 3.f * FlashHard;
	const FLinearColor DC = FLinearColor::LerpUsingHSV(
		FLinearColor(0.f, 0.5f, 0.55f, 0.70f), FLinearColor(0.16f, 1.f, 1.f, 1.f), FlashHard);
	DrawLine(StrikeX,        StrikeY - Dh, StrikeX + Dh, StrikeY,        DC, 2.f);
	DrawLine(StrikeX + Dh,  StrikeY,      StrikeX,       StrikeY + Dh,   DC, 2.f);
	DrawLine(StrikeX,        StrikeY + Dh, StrikeX - Dh, StrikeY,        DC, 2.f);
	DrawLine(StrikeX - Dh,  StrikeY,      StrikeX,       StrikeY - Dh,   DC, 2.f);
}

// =============================================================================
//  THREAT NOTE
// =============================================================================

void APcQDebugHUD::DrawThreatNote(float NoteX, float NoteY, float Alpha, bool bPassed,
                                   const FPcHudThreatEvent& Threat, const FArcGeom& G)
{
	if (bPassed)
	{
		// Grey ghost diamond, same visual weight as greyed beat circles.
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

	// Soft corona.
	DrawCircleHUD(NoteX, NoteY, 17.f,
	              Threat.Color * FLinearColor(1.f, 1.f, 1.f, Alpha * 0.16f), 1.f, 16);

	// Outer stroke diamond.
	const float Os = 10.f;
	DrawLine(NoteX,       NoteY - Os, NoteX + Os, NoteY,       OuterCol, 2.f);
	DrawLine(NoteX + Os,  NoteY,      NoteX,      NoteY + Os,  OuterCol, 2.f);
	DrawLine(NoteX,       NoteY + Os, NoteX - Os, NoteY,       OuterCol, 2.f);
	DrawLine(NoteX - Os,  NoteY,      NoteX,      NoteY - Os,  OuterCol, 2.f);

	// Filled inner core (thick crossing lines simulate fill).
	const float Is = 5.f;
	DrawLine(NoteX,       NoteY - Is, NoteX + Is, NoteY,       CoreCol, 5.f);
	DrawLine(NoteX + Is,  NoteY,      NoteX,      NoteY + Is,  CoreCol, 5.f);
	DrawLine(NoteX,       NoteY + Is, NoteX - Is, NoteY,       CoreCol, 5.f);
	DrawLine(NoteX - Is,  NoteY,      NoteX,      NoteY - Is,  CoreCol, 5.f);

	// Label — only show when close enough to matter.
	if (Alpha > 0.45f && GEngine)
	{
		// Outward radial direction (away from arc centre = visually above arc).
		const float DirX   = NoteX - G.CX;
		const float DirY   = NoteY - G.CY;
		const float DirLen = FMath::Sqrt(DirX * DirX + DirY * DirY);
		const float NormX  = (DirLen > 0.f) ? DirX / DirLen : 0.f;
		const float NormY  = (DirLen > 0.f) ? DirY / DirLen : -1.f;

		DrawText(Threat.Label,
		         Threat.Color * FLinearColor(1.f, 1.f, 1.f, Alpha * 0.80f),
		         NoteX + NormX * 20.f - 12.f,
		         NoteY + NormY * 20.f -  5.f,
		         GEngine->GetSmallFont(), 1.f);
	}
}

// =============================================================================
//  CROSSHAIR
//
//  The beat-countdown sweep ring replaces the pulsing/contracting behaviour
//  from v1.  It gives the player a clock-like read of beat timing without any
//  animation — the ring is a static arc whose LENGTH encodes remaining time.
//
//  Sweep convention:
//    Full circle  (remaining = 1.0) → beat just fired, full beat left to act.
//    Arc = 0      (remaining = 0.0) → beat imminent, ring about to refill.
//    Sweeps counterclockwise from 12-o-clock so the "hand" moves naturally.
//
//  The ring colour shifts from a neutral white-cyan to a brighter white as
//  the beat approaches, giving a subtle urgency cue without any flashing.
// =============================================================================

void APcQDebugHUD::DrawDotCrosshair(float BeatRemainingFraction)
{
	if (!Canvas) return;
	const float CX = Canvas->SizeX * 0.5f;
	const float CY = Canvas->SizeY * 0.5f;

	// ── Beat-countdown sweep ring ────────────────────────────────────────────

	// Dark shadow ring behind everything so the crosshair reads on any background.
	DrawCircleHUD(CX, CY, SweepRingRadius + 1.5f,
	              FLinearColor(0.f, 0.f, 0.f, 0.55f), SweepRingThickness + 3.f, 48);

	// Ghost track (always full circle, very faint) so the player can see the
	// ring "slot" even when it is nearly empty.
	DrawCircleHUD(CX, CY, SweepRingRadius,
	              FLinearColor(1.f, 1.f, 1.f, 0.07f), SweepRingThickness, 48);

	if (BeatRemainingFraction > 0.01f)
	{
		// Urgency ramp: starts in the last 30 % of the cycle.
		const float Urgency = FMath::Clamp((1.f - BeatRemainingFraction) / 0.30f, 0.f, 1.f);
		const float R = FMath::Lerp(160.f / 255.f, 1.f, Urgency);
		const float A = FMath::Lerp(0.44f,         0.92f, Urgency);

		// Arc starts at -PI/2 (12 o'clock) and sweeps CCW for BeatRemainingFraction * 2PI.
		const float StartAngle = -PI * 0.5f;
		const float EndAngle   = StartAngle + BeatRemainingFraction * 2.f * PI;
		DrawArcHUD(CX, CY, SweepRingRadius, SweepRingThickness,
		           StartAngle, EndAngle,
		           FLinearColor(R, FMath::Lerp(0.82f, 1.f, Urgency), 1.f, A),
		           FMath::Max(4, FMath::RoundToInt(48.f * BeatRemainingFraction)));

		// Small dot at the tip of the sweep arc — acts as a "clock hand" endpoint.
		if (BeatRemainingFraction > 0.04f)
		{
			const float TipX = CX + SweepRingRadius * FMath::Cos(EndAngle);
			const float TipY = CY + SweepRingRadius * FMath::Sin(EndAngle);
			DrawRect(FLinearColor(R, 1.f, 1.f, A),
			         TipX - 1.8f, TipY - 1.8f, 3.6f, 3.6f);
		}
	}

	// ── Static inner reticle ─────────────────────────────────────────────────

	// Cardinal ticks (N / S / E / W) — fixed, no animation.
	const float Dirs[4][2] = { {0.f, -1.f}, {0.f, 1.f}, {1.f, 0.f}, {-1.f, 0.f} };
	for (const auto& Dir : Dirs)
	{
		const float Dx = Dir[0], Dy = Dir[1];
		// Drop-shadow — thicker so the white ticks pop on light backgrounds
		DrawLine(CX + (TickOuter + 2.f) * Dx, CY + (TickOuter + 2.f) * Dy,
		         CX + (TickInner - 2.f) * Dx, CY + (TickInner - 2.f) * Dy,
		         FLinearColor(0.f, 0.f, 0.f, 0.65f), 4.f);
		// Tick
		DrawLine(CX + TickOuter * Dx, CY + TickOuter * Dy,
		         CX + TickInner * Dx, CY + TickInner * Dy,
		         FLinearColor(1.f, 1.f, 1.f, 0.70f), 1.5f);
	}

	// Outer ring shadow
	DrawCircleHUD(CX, CY, InnerRingRadius + 1.f,
	              FLinearColor(0.f, 0.f, 0.f, 0.45f), InnerRingThickness + 1.f, 32);
	// Outer ring
	DrawCircleHUD(CX, CY, InnerRingRadius, CrosshairColor, InnerRingThickness, 32);

	// Centre dot
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.50f),
	         CX - DotSize - 0.5f, CY - DotSize - 0.5f,
	         (DotSize + 0.5f) * 2.f, (DotSize + 0.5f) * 2.f);
	DrawRect(CrosshairColor, CX - DotSize, CY - DotSize, DotSize * 2.f, DotSize * 2.f);
}

// =============================================================================
//  GLANCE BOARD  (bottom-left, two columns: beats | threats)
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

	// Background panel
	const float PanelPad = 14.f;
	const float PanelW   = GlanceBoard_TrackSpacing + PanelPad * 2.f;
	DrawRect(FLinearColor(0.01f, 0.02f, 0.05f, 0.70f),
	         PlayerTrackX - PanelPad, TopY - PanelPad, PanelW, GlanceBoard_Height + PanelPad * 2.f);
	DrawLine(PlayerTrackX - PanelPad, TopY  - PanelPad,
	         PlayerTrackX - PanelPad, StrikeY + PanelPad,
	         FLinearColor(0.f, 0.55f, 0.75f, 0.22f), 1.f);

	// Column labels
	if (GEngine)
	{
		DrawText(TEXT("BEAT"), FLinearColor(0.f, 0.70f, 0.85f, 0.50f),
		         PlayerTrackX - 6.f, TopY - 14.f, GEngine->GetSmallFont(), 1.f);
		DrawText(TEXT("THRT"), FLinearColor(1.f, 0.20f, 0.32f, 0.50f),
		         EnemyTrackX  - 6.f, TopY - 14.f, GEngine->GetSmallFont(), 1.f);
	}

	// Spine lines
	DrawLine(PlayerTrackX, TopY, PlayerTrackX, StrikeY, FLinearColor(0.f,  0.55f, 0.75f, 0.15f), 1.f);
	DrawLine(EnemyTrackX,  TopY, EnemyTrackX,  StrikeY, FLinearColor(0.8f, 0.15f, 0.25f, 0.15f), 1.f);

	// Strike line
	const FLinearColor StrikeLineColor = FLinearColor::LerpUsingHSV(
		FLinearColor(0.f, 0.45f, 0.55f, 0.35f), FLinearColor(0.16f, 1.f, 1.f, 0.92f), FlashHard);
	DrawLine(PlayerTrackX - PanelPad, StrikeY, PlayerTrackX - PanelPad + PanelW, StrikeY,
	         StrikeLineColor, 2.f + 1.5f * FlashHard);

	const float SqH = 5.f + 2.f * FlashHard;
	DrawRect(StrikeLineColor,
	         PlayerTrackX - SqH * 0.5f, StrikeY - SqH * 0.5f, SqH, SqH);
	DrawRect(FLinearColor(1.f, 0.25f, 0.20f, 0.45f + FlashHard * 0.55f),
	         EnemyTrackX  - SqH * 0.5f, StrikeY - SqH * 0.5f, SqH, SqH);

	// Beat track (cyan dashes)
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

	// Rhythm NoteHit events (song layer — from subsystem)
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

	// Manually registered enemy threats (larger diamond so they're distinguishable)
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
//  BHOP DEBUG PANEL (logic unchanged)
// =============================================================================

void APcQDebugHUD::DrawBhopDebug(UPcQPlayerMovementComponent* MC)
{
	const float PanelX = 30.f;
	float PanelY = 30.f;
	const float LineH = 22.f;

	auto Row = [&](const FString& Label, const FString& Value, FLinearColor Color = FLinearColor::White)
	{
		DrawText(Label + TEXT("  ") + Value, Color, PanelX, PanelY, GEngine->GetSmallFont(), 1.f);
		PanelY += LineH;
	};

	EBhopState State = MC->GetBhopState();
	FString StateStr;
	if      (State == EBhopState::Idle)           StateStr = TEXT("IDLE");
	else if (State == EBhopState::Charging)        StateStr = TEXT("CHARGING");
	else if (State == EBhopState::GroundPounding)  StateStr = TEXT("GROUND POUND");
	else                                            StateStr = TEXT("BEAT-SYNCED");
	Row(TEXT("STATE:"), StateStr, GetStateColor(State));

	if (State == EBhopState::Charging)
	{
		const float Alpha = MC->GetChargeAlpha();
		DrawRect(FLinearColor(0.05f, 0.05f, 0.05f, 0.85f), PanelX, PanelY, 160.f, 10.f);
		DrawRect(FLinearColor::LerpUsingHSV(FLinearColor::Yellow, FLinearColor::Green, Alpha),
		         PanelX, PanelY, 160.f * Alpha, 10.f);
		PanelY += 16.f;
	}

	if (UPcMusicAnalysisSubsystem* Sub = GetWorld()->GetSubsystem<UPcMusicAnalysisSubsystem>())
	{
		Row(TEXT("PRESET:"),   Sub->GetActivePresetName(),                                     FLinearColor::Yellow);
		Row(TEXT("GAME BPM:"), FString::Printf(TEXT("%.1f"), Sub->GetCurrentGameplayBPM()));
	}

	Row(TEXT("COYOTE:"), MC->HasQueuedJump() ? TEXT("ACTIVE") : TEXT("--"),
	    MC->HasQueuedJump() ? FLinearColor::Yellow : FLinearColor(0.5f, 0.5f, 0.5f));

	const float HSpeed    = MC->GetHorizontalSpeed();
	FLinearColor SpeedCol = MC->IsInBhopChain() ? FLinearColor(1.f, 0.45f, 0.f) : FLinearColor::White;
	Row(TEXT("SPEED:"), FString::Printf(TEXT("%.0f u/s"), HSpeed), SpeedCol);

	DrawRect(FLinearColor(0.05f, 0.05f, 0.05f, 0.85f), PanelX, PanelY, 160.f, 6.f);
	DrawRect(SpeedCol, PanelX, PanelY,
	         160.f * FMath::Clamp(HSpeed / (MC->MaxWalkSpeed * 3.f), 0.f, 1.f), 6.f);
	PanelY += 14.f;

	Row(TEXT("V SPEED:"),  FString::Printf(TEXT("%.0f u/s"), MC->Velocity.Z),
	    MC->Velocity.Z < -10.f ? FLinearColor(0.6f, 0.6f, 1.f) : FLinearColor::White);
	Row(TEXT("GROUNDED:"), MC->IsMovingOnGround() ? TEXT("YES") : TEXT("NO"),
	    MC->IsMovingOnGround() ? FLinearColor::Green : FLinearColor(0.6f, 0.6f, 1.f));
}

FLinearColor APcQDebugHUD::GetStateColor(EBhopState State) const
{
	if (State == EBhopState::GroundPounding) return FLinearColor::Red;
	if (State == EBhopState::Idle)           return FLinearColor(0.5f, 0.5f, 0.5f);
	if (State == EBhopState::Charging)       return FLinearColor::Yellow;
	return FLinearColor::Green;
}

// =============================================================================
//  PRIMITIVES
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

// Inner + outer edge curves only.  Use for glow hairlines and the sweep ring.
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
	// End caps
	DrawLine(CX + InnerR * FMath::Cos(StartAngle), CY + InnerR * FMath::Sin(StartAngle),
	         CX + OuterR * FMath::Cos(StartAngle), CY + OuterR * FMath::Sin(StartAngle), Color, 2.f);
	DrawLine(CX + InnerR * FMath::Cos(EndAngle),   CY + InnerR * FMath::Sin(EndAngle),
	         CX + OuterR * FMath::Cos(EndAngle),   CY + OuterR * FMath::Sin(EndAngle),   Color, 2.f);
}

// Solid filled arc band.  Layered concentric strokes at 2.2 px intervals.
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
			         CX + R * FMath::Cos(A2), CY + R * FMath::Sin(A2),
			         Color, 2.5f);
		}
	}
}