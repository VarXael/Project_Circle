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
//  ARCH GEOMETRY
// =============================================================================

APcQDebugHUD::FArcGeom APcQDebugHUD::BuildArcGeom() const
{
	FArcGeom G;
	G.CX = Canvas->SizeX * 0.5f;
	G.CY = Canvas->SizeY + Arc_CenterBelowScreen;  

	const float SinLimb = -Arc_CenterBelowScreen / Arc_Radius;
	const float BaseLimb = FMath::Asin(FMath::Clamp(SinLimb, -1.f, 1.f));

	G.SpawnAngle  = BaseLimb - 0.05f;                      
	G.BufferAngle = -(PI - FMath::Asin(-SinLimb)) + 0.05f; 
	// Lerp between the right limb (Spawn) and left limb (Buffer) based on percentage
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
				if (!MC->OnComboEvent.IsAlreadyBound(this, &APcQDebugHUD::OnComboEvent))
					MC->OnComboEvent.AddDynamic(this, &APcQDebugHUD::OnComboEvent);
				DrawBhopDebug(MC);

				// ── Frequency sync debug ──────────────────────────────────
				if (UPcMusicAnalysisSubsystem* SyncSub = GetWorld()->GetSubsystem<UPcMusicAnalysisSubsystem>())
					DrawSyncDebug(SyncSub, MC);

				const float BeatFlash  = MC->GetOnBeatFlash();
				const float CX         = Canvas->SizeX * 0.5f;
				const float CY         = Canvas->SizeY * 0.5f;
				const float W          = Canvas->SizeX;
				const float H          = Canvas->SizeY;
				const bool  bAutoJumping = MC->IsAutoJumping();

				// ── AMBIENT RHYTHM PULSE ────────────────────────────────────
				// Always beats softly so you can keep time visually
				const float AmbAlpha = 0.04f + 0.15f * FlashSoft;
				const float BandT = 16.f + 10.f * FlashSoft;
				DrawRect(FLinearColor(0.1f, 0.8f, 1.0f, AmbAlpha), 0.f, 0.f, W, BandT); 
				DrawRect(FLinearColor(0.1f, 0.8f, 1.0f, AmbAlpha), 0.f, H - BandT, W, BandT);
				DrawRect(FLinearColor(0.1f, 0.8f, 1.0f, AmbAlpha), 0.f, 0.f, BandT, H);
				DrawRect(FLinearColor(0.1f, 0.8f, 1.0f, AmbAlpha), W - BandT, 0.f, BandT, H);

				if (bAutoJumping)
				{
					const float R = 28.f + 6.f * FlashSoft;
					const float T = 1.5f + 1.f * FlashSoft;
					const FLinearColor SyncCol(0.25f, 1.f, 0.45f, 0.4f + 0.6f * FlashSoft);
					
					for (int i = 0; i < 6; ++i)
					{
						float A1 = (i * 60.f) * (PI / 180.f);
						float A2 = ((i + 1) * 60.f) * (PI / 180.f);
						float MidA = (A1 + A2) * 0.5f;
						DrawLine(CX + R * FMath::Cos(A1), CY + R * FMath::Sin(A1), 
						         CX + R * FMath::Cos(FMath::Lerp(A1, MidA, 0.5f)), CY + R * FMath::Sin(FMath::Lerp(A1, MidA, 0.5f)), SyncCol, T);
						DrawLine(CX + R * FMath::Cos(A2), CY + R * FMath::Sin(A2), 
						         CX + R * FMath::Cos(FMath::Lerp(A2, MidA, 0.5f)), CY + R * FMath::Sin(FMath::Lerp(A2, MidA, 0.5f)), SyncCol, T);
					}

					const float AX = CX - 40.f;
					const float AY = CY + 45.f; 
					if (GEngine) DrawText(TEXT("SYNCED AUTO-JUMP"), SyncCol, AX + 1.f, AY, GEngine->GetSmallFont(), 1.f);
				}

				// ── Snap Exact Debug UI ─────────────────────────────────────
				{
					const ESnapAction SnapAct  = MC->GetActiveSnap();
					const float SnapPulse      = MC->GetSnapPulseFlash();

					// Exact Code Label Debug mapping
					FLinearColor SnapCol(0.5f, 0.5f, 0.5f, 0.f);
					FString      SnapLabel;
					if (SnapAct == ESnapAction::Jump)
					{
						SnapCol   = FLinearColor(0.7f, 1.f, 0.3f);   // lime green
						SnapLabel = TEXT("GROUND JUMP");
					}
					else if (SnapAct == ESnapAction::LandingJump)
					{
						SnapCol   = FLinearColor(1.f, 0.88f, 0.2f);  // gold
						SnapLabel = TEXT("BUFFERED JUMP");
					}
					else if (SnapAct == ESnapAction::DoubleJump)
					{
						SnapCol   = FLinearColor(0.27f, 0.65f, 1.f); // blue
						SnapLabel = TEXT("DOUBLE JUMP");
					}
					else if (SnapAct == ESnapAction::Slide)
					{
						SnapCol   = FLinearColor(1.f, 0.55f, 0.1f);  // orange
						SnapLabel = TEXT("SLIDE BOOST");
					}

					const float AmbA     = (0.25f + 0.55f * FlashSoft);
					const float PulseA   = (AmbA + SnapPulse * 0.4f);
					const float RingR    = 32.f + SnapPulse * 4.f;  
					const float GapDeg   = 8.f;   
					const int32 NSeg     = 6;
					const float SegDeg   = (360.f / NSeg) - GapDeg;
					const float SegThick = 2.5f + SnapPulse * 1.5f;

					const int32 LitSeg = FMath::FloorToInt(FlashSoft * NSeg);
					for (int32 i = 0; i < NSeg; ++i)
					{
						const float StartA = FMath::DegreesToRadians(i * (360.f / NSeg) + GapDeg * 0.5f - 90.f);
						const float EndA   = StartA + FMath::DegreesToRadians(SegDeg);
						const float SegA   = (i <= LitSeg) ? PulseA : AmbA * 0.5f;
						const FLinearColor SC = SnapCol * FLinearColor(1,1,1, SegA);
						const int32 Steps = 8;
						for (int32 s = 0; s < Steps; ++s)
						{
							const float A1 = FMath::Lerp(StartA, EndA, (float)s     / Steps);
							const float A2 = FMath::Lerp(StartA, EndA, (float)(s+1) / Steps);
							DrawLine(CX + RingR * FMath::Cos(A1), CY + RingR * FMath::Sin(A1),
									 CX + RingR * FMath::Cos(A2), CY + RingR * FMath::Sin(A2),
									 SC, SegThick);
						}
					}

					if (GEngine)
					{
						const FLinearColor LC = SnapCol * FLinearColor(1,1,1, 0.5f + 0.5f * FlashSoft);
						DrawText(SnapLabel, LC, CX - SnapLabel.Len() * 3.5f, CY + RingR + 8.f, GEngine->GetSmallFont(), 1.f);
					}
				}

				// ── Boost fuel bar ─────────────────
				{
					const float BoostCool   = MC->GetBoostCooldownAlpha();
					const float BoostActive = MC->GetBoostActiveAlpha();
					const bool  bIsBoosting = MC->IsPowerBoosting();
					const FLinearColor BoostCol(1.f, 0.55f, 0.05f);

					const float BW  = 240.f;
					const float BH  =   8.f;
					const float BX  = CX - BW * 0.5f;
					const float BY  = Canvas->SizeY - 100.f;

					DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.75f), BX - 2.f, BY - 2.f, BW + 4.f, BH + 4.f);
					DrawRect(FLinearColor(0.05f, 0.04f, 0.02f, 1.f), BX, BY, BW, BH);

					float FillRatio = 0.f;
					float FillAlpha = 0.f;
					if (bIsBoosting) {
						FillRatio = BoostActive;
						FillAlpha = 0.85f + BeatFlash * 0.15f;
					} else if (BoostCool > 0.f) {
						FillRatio = 1.f - BoostCool;
						FillAlpha = 0.25f;
					} else {
						FillRatio = 1.f;
						FillAlpha = 0.6f + FlashSoft * 0.3f;
					}
					
					if (FillRatio > 0.01f)
						DrawRect(BoostCol * FLinearColor(1,1,1,FillAlpha), BX, BY, BW * FillRatio, BH);

					const float BA = bIsBoosting ? (0.7f + BeatFlash*0.3f) : (BoostCool <= 0.f ? 0.55f : 0.2f);
					DrawRect(BoostCol * FLinearColor(1,1,1,BA), BX, BY,       BW, 1.f);
					DrawRect(BoostCol * FLinearColor(1,1,1,BA), BX, BY+BH-1.f, BW, 1.f);
					DrawRect(BoostCol * FLinearColor(1,1,1,BA), BX, BY,       1.f, BH);
					DrawRect(BoostCol * FLinearColor(1,1,1,BA), BX+BW-1.f, BY, 1.f, BH);

					if (GEngine) {
						const FString BLabel = bIsBoosting ? TEXT("BOOST") : (BoostCool > 0.f ? TEXT("BOOST CD") : TEXT("BOOST RDY"));
						DrawText(BLabel, BoostCol * FLinearColor(1,1,1,FillAlpha + 0.2f), BX + 4.f, BY - 13.f, GEngine->GetSmallFont(), 1.f);
					}
				}

				if (Canvas) DrawAbilityBars(MC, PC);
				if (Canvas) DrawComboFeed();
			}
		}
	}
}

void APcQDebugHUD::DrawArcMetronome(UPcMusicAnalysisSubsystem* MusicSub, const FArcGeom& G,
                                     int32 CurrentTimeMS, int32 NextBeatMS, float IntervalMS,
                                     float FlashHard, float FlashSoft)
{
	const float BufferTimeMS = IntervalMS * 0.5f;
	DrawArcFilled(G.CX, G.CY, Arc_Radius, Arc_Thickness + 10.f, G.SpawnAngle, G.BufferAngle, FLinearColor(0.f, 0.f, 0.f, 0.18f), 60);
	DrawArcFilled(G.CX, G.CY, Arc_Radius, Arc_Thickness, G.StrikeAngle, G.BufferAngle, FLinearColor(0.010f, 0.018f, 0.030f, 0.30f), 20);

	const FLinearColor ActiveColor = FLinearColor::LerpUsingHSV(FLinearColor(0.020f, 0.055f, 0.100f, 0.34f), FLinearColor(0.016f, 0.090f, 0.160f, 0.42f), FlashSoft);
	DrawArcFilled(G.CX, G.CY, Arc_Radius, Arc_Thickness, G.SpawnAngle, G.StrikeAngle, ActiveColor, 60);
	DrawArcHUD(G.CX, G.CY, Arc_Radius - Arc_Thickness * 0.5f, 1.0f, G.SpawnAngle, G.StrikeAngle, FLinearColor(0.4f, 0.85f, 1.f, 0.025f + FlashSoft * 0.10f), 60);

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
		const bool  bPassed  = TimeDiff < 0.f;

		float Angle, Alpha;
		if (!bPassed) {
			const float Progress = FMath::Clamp(TimeDiff / (IntervalMS * (float)Arc_BeatsToShow), 0.f, 1.f);
			Angle = FMath::Lerp(G.StrikeAngle, G.SpawnAngle, Progress);
			Alpha = FMath::Lerp(1.f, 0.25f, Progress);
		} else {
			const float Progress = FMath::Clamp(FMath::Abs(TimeDiff) / BufferTimeMS, 0.f, 1.f);
			Angle = FMath::Lerp(G.StrikeAngle, G.BufferAngle, Progress);
			Alpha = 1.f - Progress;
		}
		if (Alpha < 0.02f) continue;

		const float NoteX = G.CX + Arc_Radius * FMath::Cos(Angle);
		const float NoteY = G.CY + Arc_Radius * FMath::Sin(Angle);

		if (NoteY > Canvas->SizeY + 4.f) continue;

		if (Entry.bIsThreat) DrawThreatNote(NoteX, NoteY, Alpha, bPassed, ActiveThreats[Entry.ThreatIdx], G);
		else {
			if (bPassed) {
				DrawCircleHUD(NoteX, NoteY, 6.f, FLinearColor(0.20f, 0.24f, 0.34f, Alpha * 1.25f), 1.4f, 16);
			} else {
				DrawCircleHUD(NoteX, NoteY, 14.f, FLinearColor(0.f, 0.80f, 1.f, Alpha * 0.16f), 1.f, 16);
				const bool  bIsNext  = TimeDiff < IntervalMS && TimeDiff >= 0.f;
				const float RingSize = bIsNext ? Arc_Thickness * 0.65f : Arc_Thickness * 0.42f;
				DrawCircleHUD(NoteX, NoteY, RingSize, FLinearColor(0.f, 0.82f, 1.f, Alpha * (bIsNext ? 1.f : 0.72f)), bIsNext ? 2.4f : 1.8f, bIsNext ? 24 : 16);
				DrawRect(FLinearColor(0.70f, 0.95f, 1.f, Alpha * (bIsNext ? 1.f : 0.7f)), NoteX - 2.f, NoteY - 2.f, 4.f, 4.f);
			}
		}
	}

	const float StrikeX = G.CX + Arc_Radius * FMath::Cos(G.StrikeAngle);
	const float StrikeY = G.CY + Arc_Radius * FMath::Sin(G.StrikeAngle);

	if (FlashSoft > 0.01f) DrawCircleHUD(StrikeX, StrikeY, 38.f * FlashSoft, FLinearColor(0.1f, 0.9f, 0.85f, 0.06f * FlashSoft), 1.f, 32);
	if (FlashHard > 0.01f) {
		DrawCircleHUD(StrikeX, StrikeY, 22.f, FLinearColor(0.15f, 1.f, 0.90f, 0.12f * FlashHard), 2.f, 32);
		DrawCircleHUD(StrikeX, StrikeY, 13.f, FLinearColor(0.20f, 1.f, 0.95f, 0.22f * FlashHard), 2.f, 24);
	}

	const float InnerR = Arc_Radius - Arc_Thickness - 6.f;
	const float OuterR = Arc_Radius + Arc_Thickness + 6.f + 10.f * FlashHard;
	const FLinearColor TickColor = FLinearColor::LerpUsingHSV(FLinearColor(0.f, 0.55f, 0.65f, 0.55f), FLinearColor(0.16f, 1.f, 1.f, 1.f), FlashHard);
	DrawLine(G.CX + InnerR * FMath::Cos(G.StrikeAngle), G.CY + InnerR * FMath::Sin(G.StrikeAngle), G.CX + OuterR * FMath::Cos(G.StrikeAngle), G.CY + OuterR * FMath::Sin(G.StrikeAngle), TickColor, 3.5f + 3.f * FlashHard);

	const float Dh = 5.f + 3.f * FlashHard;
	const FLinearColor DC = FLinearColor::LerpUsingHSV(FLinearColor(0.f, 0.5f, 0.55f, 0.70f), FLinearColor(0.16f, 1.f, 1.f, 1.f), FlashHard);
	DrawLine(StrikeX, StrikeY - Dh, StrikeX + Dh, StrikeY, DC, 2.f);
	DrawLine(StrikeX + Dh, StrikeY, StrikeX, StrikeY + Dh, DC, 2.f);
	DrawLine(StrikeX, StrikeY + Dh, StrikeX - Dh, StrikeY, DC, 2.f);
	DrawLine(StrikeX - Dh, StrikeY, StrikeX, StrikeY - Dh, DC, 2.f);
}

void APcQDebugHUD::DrawThreatNote(float NoteX, float NoteY, float Alpha, bool bPassed, const FPcHudThreatEvent& Threat, const FArcGeom& G)
{
	if (bPassed) {
		const FLinearColor Ghost(0.18f, 0.16f, 0.24f, Alpha * 1.2f);
		const float Hs = 7.f;
		DrawLine(NoteX, NoteY - Hs, NoteX + Hs, NoteY, Ghost, 1.5f); DrawLine(NoteX + Hs, NoteY, NoteX, NoteY + Hs, Ghost, 1.5f);
		DrawLine(NoteX, NoteY + Hs, NoteX - Hs, NoteY, Ghost, 1.5f); DrawLine(NoteX - Hs, NoteY, NoteX, NoteY - Hs, Ghost, 1.5f);
		return;
	}

	const FLinearColor OuterCol = Threat.Color * FLinearColor(1.f, 1.f, 1.f, Alpha);
	const FLinearColor CoreCol  = FLinearColor::LerpUsingHSV(Threat.Color, FLinearColor::White, 0.5f) * FLinearColor(1.f, 1.f, 1.f, Alpha * 0.85f);

	DrawCircleHUD(NoteX, NoteY, 17.f, Threat.Color * FLinearColor(1.f, 1.f, 1.f, Alpha * 0.16f), 1.f, 16);

	const float Os = 10.f;
	DrawLine(NoteX, NoteY - Os, NoteX + Os, NoteY, OuterCol, 2.f); DrawLine(NoteX + Os, NoteY, NoteX, NoteY + Os, OuterCol, 2.f);
	DrawLine(NoteX, NoteY + Os, NoteX - Os, NoteY, OuterCol, 2.f); DrawLine(NoteX - Os, NoteY, NoteX, NoteY - Os, OuterCol, 2.f);

	const float Is = 5.f;
	DrawLine(NoteX, NoteY - Is, NoteX + Is, NoteY, CoreCol, 5.f); DrawLine(NoteX + Is, NoteY, NoteX, NoteY + Is, CoreCol, 5.f);
	DrawLine(NoteX, NoteY + Is, NoteX - Is, NoteY, CoreCol, 5.f); DrawLine(NoteX - Is, NoteY, NoteX, NoteY - Is, CoreCol, 5.f);

	if (Alpha > 0.45f && GEngine) {
		const float DirX = NoteX - G.CX; const float DirY = NoteY - G.CY;
		const float DirLen = FMath::Sqrt(DirX * DirX + DirY * DirY);
		const float NormX = (DirLen > 0.f) ? DirX / DirLen : 0.f; const float NormY = (DirLen > 0.f) ? DirY / DirLen : -1.f;
		DrawText(Threat.Label, Threat.Color * FLinearColor(1.f, 1.f, 1.f, Alpha * 0.80f), NoteX + NormX * 20.f - 12.f, NoteY + NormY * 20.f -  5.f, GEngine->GetSmallFont(), 1.f);
	}
}

void APcQDebugHUD::DrawDotCrosshair(float BeatRemainingFraction)
{
	if (!Canvas) return;
	const float CX = Canvas->SizeX * 0.5f;
	const float CY = Canvas->SizeY * 0.5f;

	const float FlashA = FMath::Clamp(1.f - BeatRemainingFraction / 0.15f, 0.f, 1.f);

	const float AW      = CrosshairChevronWidth;
	const float AH      = CrosshairChevronHeight;
	const float EXTRA_H = 2.0f;  
	const float THICK   = 3.0f;  

	auto DrawChev = [&](float TipX, bool bLeft, float Alpha, FLinearColor Col) {
		const float BackX = bLeft ? TipX + AW : TipX - AW; 
		const float H     = AH + EXTRA_H;
		const FLinearColor Sh(0.f, 0.f, 0.f, FMath::Min(0.5f, Alpha * 0.8f));
		DrawLine(BackX, CY - H, TipX, CY, Sh, THICK + 2.5f); DrawLine(TipX,  CY, BackX, CY + H, Sh, THICK + 2.5f);
		FLinearColor FinalCol = Col; FinalCol.A *= Alpha; 
		DrawLine(BackX, CY - H, TipX, CY, FinalCol, THICK); DrawLine(TipX,  CY, BackX, CY + H, FinalCol, THICK);
	};

	const float MaxBeats = FMath::Max(1.0f, (float)CrosshairBeatsToShow);

	for (int32 i = CrosshairBeatsToShow - 1; i >= 0; --i) {
		const float d = BeatRemainingFraction + (float)i;
		const float DistNormalized = FMath::Clamp(d / MaxBeats, 0.0f, 1.0f);
		const float FadeIn = 1.0f - DistNormalized;
		const float Alpha = FMath::Pow(FadeIn, 2.0f); 
		if (Alpha < 0.01f) continue;
		const float Dist = CrosshairGateDist + d * CrosshairBeatStep;
		FLinearColor NoteCol(0.6f, 0.6f, 0.6f, Alpha);
		DrawChev(CX - Dist, true, Alpha, NoteCol); DrawChev(CX + Dist, false, Alpha, NoteCol);
	}

	const FLinearColor GateResting(0.05f, 0.05f, 0.05f, 0.45f);
	const FLinearColor GateFlash(1.0f, 1.0f, 1.0f, 1.0f);
	const FLinearColor GateCol = GateResting + (GateFlash - GateResting) * FlashA;
	
	DrawChev(CX - CrosshairGateDist, true,  1.f, GateCol); DrawChev(CX + CrosshairGateDist, false, 1.f, GateCol);

	const float DR  = DotSize;
	const FLinearColor DotResting(0.05f, 0.75f, 1.0f, 0.8f);
	const FLinearColor DotFlash(1.0f, 1.0f, 1.0f, 1.0f);
	const FLinearColor DotCol = DotResting + (DotFlash - DotResting) * FlashA;

	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.8f), CX - DR - 1.f, CY - DR - 1.f, (DR + 1.f) * 2.f, (DR + 1.f) * 2.f);
	DrawRect(DotCol, CX - DR, CY - DR, DR * 2.f, DR * 2.f);
}

void APcQDebugHUD::DrawGlanceBoard(UPcMusicAnalysisSubsystem* MusicSub, int32 CurrentTimeMS, int32 NextBeatMS, float IntervalMS, float FlashHard)
{
	if (!Canvas) return;
	const float StrikeY      = Canvas->SizeY * GlanceBoard_ScreenYPercent;
	const float TopY         = StrikeY - GlanceBoard_Height;
	const float PixPerBeat   = GlanceBoard_Height / (float)GlanceBoard_BeatsToShow;
	const float PlayerTrackX = GlanceBoard_XOffset;
	const float EnemyTrackX  = GlanceBoard_XOffset + GlanceBoard_TrackSpacing;
	const float PanelPad = 14.f;
	const float PanelW   = GlanceBoard_TrackSpacing + PanelPad * 2.f;

	DrawRect(FLinearColor(0.01f, 0.02f, 0.05f, 0.70f), PlayerTrackX - PanelPad, TopY - PanelPad, PanelW, GlanceBoard_Height + PanelPad * 2.f);
	DrawLine(PlayerTrackX - PanelPad, TopY  - PanelPad, PlayerTrackX - PanelPad, StrikeY + PanelPad, FLinearColor(0.f, 0.55f, 0.75f, 0.22f), 1.f);

	if (GEngine) {
		DrawText(TEXT("BEAT"), FLinearColor(0.f, 0.70f, 0.85f, 0.50f), PlayerTrackX - 6.f, TopY - 14.f, GEngine->GetSmallFont(), 1.f);
		DrawText(TEXT("THRT"), FLinearColor(1.f, 0.20f, 0.32f, 0.50f), EnemyTrackX  - 6.f, TopY - 14.f, GEngine->GetSmallFont(), 1.f);
	}

	DrawLine(PlayerTrackX, TopY, PlayerTrackX, StrikeY, FLinearColor(0.f,  0.55f, 0.75f, 0.15f), 1.f);
	DrawLine(EnemyTrackX,  TopY, EnemyTrackX,  StrikeY, FLinearColor(0.8f, 0.15f, 0.25f, 0.15f), 1.f);

	const FLinearColor StrikeLineColor = FLinearColor::LerpUsingHSV(FLinearColor(0.f, 0.45f, 0.55f, 0.35f), FLinearColor(0.16f, 1.f, 1.f, 0.92f), FlashHard);
	DrawLine(PlayerTrackX - PanelPad, StrikeY, PlayerTrackX - PanelPad + PanelW, StrikeY, StrikeLineColor, 2.f + 1.5f * FlashHard);

	const float SqH = 5.f + 2.f * FlashHard;
	DrawRect(StrikeLineColor, PlayerTrackX - SqH * 0.5f, StrikeY - SqH * 0.5f, SqH, SqH);
	DrawRect(FLinearColor(1.f, 0.25f, 0.20f, 0.45f + FlashHard * 0.55f), EnemyTrackX  - SqH * 0.5f, StrikeY - SqH * 0.5f, SqH, SqH);

	for (int32 i = -1; i <= GlanceBoard_BeatsToShow; ++i) {
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
		DrawRect(FLinearColor(0.f, 0.82f, 1.f, Alpha * (bIsNext ? 0.92f : 0.65f)), PlayerTrackX - DashW * 0.5f, NoteY - DashH * 0.5f, DashW, DashH);
	}

	const float LookaheadSec = (IntervalMS * (float)GlanceBoard_BeatsToShow) / 1000.f;
	for (const FPcRuntimeEvent& Note : MusicSub->GetUpcomingNotes(LookaheadSec)) {
		const float TimeDiffMS = (float)(Note.TimestampMS - CurrentTimeMS);
		if (TimeDiffMS < -120.f) continue;
		const float NoteY = StrikeY - (TimeDiffMS / IntervalMS) * PixPerBeat;
		if (NoteY < TopY || NoteY > StrikeY + 8.f) continue;
		const float Alpha = (TimeDiffMS >= 0.f)
			? FMath::Clamp(1.f - TimeDiffMS / (IntervalMS * (float)GlanceBoard_BeatsToShow), 0.f, 1.f)
			: FMath::Clamp(1.f - FMath::Abs(TimeDiffMS) / 120.f, 0.f, 1.f);

		const float Dr = 5.f; const FLinearColor EC(1.f, 0.15f, 0.25f, Alpha);
		DrawLine(EnemyTrackX, NoteY - Dr, EnemyTrackX + Dr, NoteY, EC, 1.5f); DrawLine(EnemyTrackX + Dr, NoteY, EnemyTrackX, NoteY + Dr, EC, 1.5f);
		DrawLine(EnemyTrackX, NoteY + Dr, EnemyTrackX - Dr, NoteY, EC, 1.5f); DrawLine(EnemyTrackX - Dr, NoteY, EnemyTrackX, NoteY - Dr, EC, 1.5f);
	}

	for (const FPcHudThreatEvent& Threat : ActiveThreats) {
		const float TimeDiffMS = (float)(Threat.TimestampMS - CurrentTimeMS);
		if (TimeDiffMS < -120.f) continue;
		const float NoteY = StrikeY - (TimeDiffMS / IntervalMS) * PixPerBeat;
		if (NoteY < TopY || NoteY > StrikeY + 8.f) continue;
		const float Alpha = (TimeDiffMS >= 0.f)
			? FMath::Clamp(1.f - TimeDiffMS / (IntervalMS * (float)GlanceBoard_BeatsToShow), 0.f, 1.f)
			: FMath::Clamp(1.f - FMath::Abs(TimeDiffMS) / 120.f, 0.f, 1.f);
		const float Dr = 6.f; const FLinearColor TC = Threat.Color * FLinearColor(1.f, 1.f, 1.f, Alpha);
		DrawLine(EnemyTrackX, NoteY - Dr, EnemyTrackX + Dr, NoteY, TC, 2.f); DrawLine(EnemyTrackX + Dr, NoteY, EnemyTrackX, NoteY + Dr, TC, 2.f);
		DrawLine(EnemyTrackX, NoteY + Dr, EnemyTrackX - Dr, NoteY, TC, 2.f); DrawLine(EnemyTrackX - Dr, NoteY, EnemyTrackX, NoteY - Dr, TC, 2.f);
	}
}

void APcQDebugHUD::DrawBhopDebug(UPcQPlayerMovementComponent* MC)
{
	const float PanelX = 30.f; float PanelY = 30.f; const float LineH = 22.f;

	auto Row = [&](const FString& Label, const FString& Value, FLinearColor Color = FLinearColor::White) {
		DrawText(Label + TEXT("  ") + Value, Color, PanelX, PanelY, GEngine->GetSmallFont(), 1.f);
		PanelY += LineH;
	};

	EBhopState State = MC->GetBhopState();
	FString StateStr;
	if      (State == EBhopState::PowerBoost)     StateStr = TEXT("BOOST");
	else if (State == EBhopState::GroundPounding)  StateStr = TEXT("GROUND POUND");
	else if (State == EBhopState::WallSwim)        StateStr = TEXT("WALL SWIM");
	else                                            StateStr = TEXT("ACTIVE");
	Row(TEXT("STATE:"), StateStr, GetStateColor(State));

	if (UPcMusicAnalysisSubsystem* Sub = GetWorld()->GetSubsystem<UPcMusicAnalysisSubsystem>()) {
		Row(TEXT("PRESET:"),   Sub->GetActivePresetName(), FLinearColor::Yellow);
		Row(TEXT("GAME BPM:"), FString::Printf(TEXT("%.1f"), Sub->GetCurrentGameplayBPM()));
	}

	Row(TEXT("COYOTE:"), MC->HasQueuedJump() ? TEXT("ACTIVE") : TEXT("--"), MC->HasQueuedJump() ? FLinearColor::Yellow : FLinearColor(0.5f, 0.5f, 0.5f));

	const float HSpeed    = MC->GetHorizontalSpeed();
	FLinearColor SpeedCol = MC->IsInBhopChain() ? FLinearColor(1.f, 0.45f, 0.f) : FLinearColor::White;
	Row(TEXT("SPEED:"), FString::Printf(TEXT("%.0f u/s"), HSpeed), SpeedCol);

	DrawRect(FLinearColor(0.05f, 0.05f, 0.05f, 0.85f), PanelX, PanelY, 160.f, 6.f);
	DrawRect(SpeedCol, PanelX, PanelY, 160.f * FMath::Clamp(HSpeed / (MC->MaxWalkSpeed * 3.f), 0.f, 1.f), 6.f);
	PanelY += 14.f;

	Row(TEXT("V SPEED:"),  FString::Printf(TEXT("%.0f u/s"), MC->Velocity.Z), MC->Velocity.Z < -10.f ? FLinearColor(0.6f, 0.6f, 1.f) : FLinearColor::White);
	Row(TEXT("GROUNDED:"), MC->IsMovingOnGround() ? TEXT("YES") : TEXT("NO"), MC->IsMovingOnGround() ? FLinearColor::Green : FLinearColor(0.6f, 0.6f, 1.f));
}

void APcQDebugHUD::OnComboEvent(const FString& Label, FLinearColor Color)
{
	if (!GetWorld()) return;
	FPcComboFeedEntry E; E.Label = Label; E.Color = Color; E.BornAt = GetWorld()->GetTimeSeconds();
	ComboFeed.Add(E);
	while (ComboFeed.Num() > ComboFeed_MaxEntries) ComboFeed.RemoveAt(0);
}

void APcQDebugHUD::DrawComboFeed()
{
	if (!Canvas || !GEngine || !GetWorld()) return;

	const float Now = GetWorld()->GetTimeSeconds();
	const float FX = 22.f; const float FBY = Canvas->SizeY * 0.82f; const float LineH = 24.f;
	const float HalfDur = ComboFeed_FadeDuration * 0.55f;

	ComboFeed.RemoveAll([&](const FPcComboFeedEntry& E) { return (Now - E.BornAt) >= ComboFeed_FadeDuration; });

	for (int32 i = 0; i < ComboFeed.Num(); ++i) {
		const FPcComboFeedEntry& E = ComboFeed[ComboFeed.Num() - 1 - i];
		const float Age = Now - E.BornAt;
		const float Alpha = Age < HalfDur ? 1.f : FMath::Clamp(1.f - (Age - HalfDur) / (ComboFeed_FadeDuration - HalfDur), 0.f, 1.f);
		if (Alpha < 0.02f) continue;

		const float Y = FBY - i * LineH;
		DrawRect(E.Color * FLinearColor(1,1,1,Alpha * 0.88f), FX, Y - 14.f, 3.f, 18.f);
		DrawText(E.Label, FLinearColor(0,0,0, Alpha * 0.65f), FX + 9.f, Y, GEngine->GetSmallFont(), 1.f);
		DrawText(E.Label, E.Color * FLinearColor(1,1,1,Alpha), FX + 8.f, Y - 1.f, GEngine->GetSmallFont(), 1.f);
	}
}

FLinearColor APcQDebugHUD::GetStateColor(EBhopState State) const
{
	if (State == EBhopState::GroundPounding) return FLinearColor::Red;
	if (State == EBhopState::WallSwim)       return FLinearColor(0.f, 0.82f, 1.f);
	if (State == EBhopState::PowerBoost)     return FLinearColor(1.f, 0.55f, 0.f);
	return FLinearColor::Green;
}

void APcQDebugHUD::DrawAbilityBars(UPcQPlayerMovementComponent* MC, APlayerController* PC)
{
	if (!Canvas || !GEngine) return;

	const float IconSz  = 52.f; const float IconGap = 10.f;
	const float TotalW  = IconSz * 2.f + IconGap * 1.f;
	const float StartX  = (Canvas->SizeX - TotalW) * 0.5f;
	const float IconY   = Canvas->SizeY - IconSz - 20.f;

	const float BeatFlash = MC ? MC->GetOnBeatFlash() : 0.f;

	float PistolCoolAlpha = 0.f, PistolBaseSec = 0.5f;
	if (APcQPlayerCharacter* Ch = Cast<APcQPlayerCharacter>(PC->GetPawn())) {
		PistolCoolAlpha = Ch->GetPistolCooldownAlpha(); PistolBaseSec   = Ch->PistolBaseCooldownSec;
	}

	const float DJCoolAlpha = MC ? MC->GetDoubleJumpCooldownAlpha() : 0.f;

	struct FIcon { FString Label; FLinearColor Col; float Fill; bool bActive; float CoolSec; };
	FIcon Icons[2];

	Icons[0] = { TEXT("DJUMP"), FLinearColor(0.18f, 0.65f, 1.f), 1.f - DJCoolAlpha, false, DJCoolAlpha * (MC ? MC->DoubleJumpBaseCooldownSec : 2.f) };
	Icons[1] = { TEXT("FIRE"),  FLinearColor(0.9f, 0.18f, 0.28f), 1.f - PistolCoolAlpha, false, PistolCoolAlpha * PistolBaseSec };

	for (int32 i = 0; i < 2; ++i) {
		const FIcon& Ic = Icons[i];
		const float IX = StartX + i * (IconSz + IconGap);
		const bool bRdy = Ic.Fill >= 1.f && !Ic.bActive;
		const bool bOnCD = Ic.Fill < 1.f && !Ic.bActive;

		DrawRect(FLinearColor(0,0,0,0.82f), IX - 3.f, IconY - 3.f, IconSz + 6.f, IconSz + 6.f);
		DrawRect(FLinearColor(0.03f, 0.04f, 0.08f, 1.f), IX, IconY, IconSz, IconSz);

		const float FillH = IconSz * Ic.Fill;
		const float FillY = IconY + IconSz - FillH;
		const float FillAlpha = bRdy ? (0.75f + BeatFlash * 0.20f) : Ic.bActive ? 0.88f : 0.28f;
		if (FillH > 0.5f) DrawRect(Ic.Col * FLinearColor(1,1,1, FillAlpha), IX, FillY, IconSz, FillH);
		if (Ic.bActive && BeatFlash > 0.01f) DrawRect(FLinearColor(1,1,1, BeatFlash * 0.22f), IX, FillY, IconSz, FillH);

		const float BorderA = bRdy ? (0.85f + BeatFlash * 0.15f) : (Ic.bActive ? 0.70f : 0.22f);
		DrawRect(Ic.Col * FLinearColor(1,1,1, BorderA), IX, IconY, IconSz, 2.f);
		DrawRect(Ic.Col * FLinearColor(1,1,1, BorderA), IX, IconY+IconSz-2.f, IconSz, 2.f);
		DrawRect(Ic.Col * FLinearColor(1,1,1, BorderA), IX, IconY, 2.f, IconSz);
		DrawRect(Ic.Col * FLinearColor(1,1,1, BorderA), IX+IconSz-2.f, IconY, 2.f, IconSz);

		if (bRdy && BeatFlash > 0.05f) {
			DrawRect(Ic.Col * FLinearColor(1,1,1, BeatFlash * 0.90f), IX, IconY, IconSz, 2.f);
			DrawRect(Ic.Col * FLinearColor(1,1,1, BeatFlash * 0.90f), IX, IconY+IconSz-2.f, IconSz, 2.f);
			DrawRect(Ic.Col * FLinearColor(1,1,1, BeatFlash * 0.90f), IX, IconY, 2.f, IconSz);
			DrawRect(Ic.Col * FLinearColor(1,1,1, BeatFlash * 0.90f), IX+IconSz-2.f, IconY, 2.f, IconSz);
		}

		const FLinearColor TextCol = bRdy ? Ic.Col * FLinearColor(1,1,1,1.f) : (Ic.bActive ? Ic.Col * FLinearColor(1,1,1,1.f) : FLinearColor(0.40f, 0.45f, 0.55f, 0.90f));
		DrawText(Ic.Label, TextCol, IX + 4.f, IconY + 4.f, GEngine->GetSmallFont(), 1.f);

		FString NumStr;
		if (Ic.bActive) NumStr = FString::Printf(TEXT("%.1f"), Ic.Fill);
		else if (bOnCD) NumStr = FString::Printf(TEXT("%.1f"), Ic.CoolSec);

		if (!NumStr.IsEmpty()) DrawText(NumStr, TextCol, IX + 8.f, IconY + IconSz * 0.5f - 4.f, GEngine->GetSmallFont(), 1.f);
	}
}

// =============================================================================
//  SYNC SYSTEM — Frequency visualization
//
//  Two waves: the song's note envelope and the player's action pulse.
//  SyncLevel measures how much they correlate over a rolling window.
//  No gameplay effect — pure debug visualization.
// =============================================================================

void APcQDebugHUD::UpdateSyncWaves(UPcMusicAnalysisSubsystem* MusicSub, UPcQPlayerMovementComponent* MC)
{
	if (!GetWorld() || !MusicSub || !MusicSub->IsReadyForPlayback()) return;

	const float DeltaTime = GetWorld()->GetDeltaSeconds();

	// ── Song pulse: spike when notes fire ─────────────────────────────────────
	const int32 NowMS = MusicSub->GetCurrentPlaybackTimeMS();
	const TArray<FPcRuntimeEvent> UpcomingNotes = MusicSub->GetUpcomingNotes(0.08f);
	for (const FPcRuntimeEvent& Ev : UpcomingNotes)
	{
		// Fire on notes within 1 frame of the playhead (~16ms at 60fps)
		const int32 DistMS = FMath::Abs(Ev.TimestampMS - NowMS);
		if (Ev.EventType == EPcRuntimeEventType::NoteHit && DistMS < 25)
		{
			// Check we haven't already spiked for this note this frame
			if (Ev.TimestampMS != LastNoteIdx)
			{
				SongPulse = FMath::Min(SongPulse + 0.90f, 1.5f);
				LastNoteIdx = Ev.TimestampMS;  // reuse as "last fired timestamp"
			}
		}
	}

	// Decay song pulse (same rate as player pulse for fair comparison)
	SongPulse = FMath::Max(0.f, SongPulse - DeltaTime * 3.5f);

	// ── Sample both waves into ring buffer at ~20Hz ────────────────────────────
	WaveSampleTimer += DeltaTime;
	if (WaveSampleTimer >= 0.05f)
	{
		WaveSampleTimer = 0.f;
		SongWave  [WaveWriteIdx] = SongPulse;
		PlayerWave[WaveWriteIdx] = MC->GetPlayerPulse();
		WaveWriteIdx = (WaveWriteIdx + 1) % WaveHistorySize;
	}

	// ── Compute sync level: normalized dot product over full history ───────────
	// High when both waves peak together, near zero when uncorrelated.
	float Dot = 0.f, MagSong = 0.f, MagPlayer = 0.f;
	for (int32 i = 0; i < WaveHistorySize; ++i)
	{
		Dot       += SongWave[i] * PlayerWave[i];
		MagSong   += SongWave[i]   * SongWave[i];
		MagPlayer += PlayerWave[i] * PlayerWave[i];
	}
	const float Denom = FMath::Sqrt(MagSong * MagPlayer);
	const float RawSync = (Denom > 0.001f) ? Dot / Denom : 0.f;

	// Smooth sync level so it doesn't jump instantly
	SyncLevel = FMath::FInterpTo(SyncLevel, RawSync, DeltaTime, 2.5f);
}

void APcQDebugHUD::DrawSyncDebug(UPcMusicAnalysisSubsystem* MusicSub, UPcQPlayerMovementComponent* MC)
{
	if (!Canvas || !GEngine || !MusicSub) return;

	UpdateSyncWaves(MusicSub, MC);

	// ── Layout: bottom-right, two stacked waveforms + sync bar ───────────────
	const float PanelW  = 280.f;
	const float WaveH   = 50.f;   // height of each waveform strip
	const float Gap     = 8.f;
	const float PanelX  = Canvas->SizeX - PanelW - 16.f;
	const float PanelY  = Canvas->SizeY - (WaveH * 2.f + Gap + 22.f + 16.f);

	// Background
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.72f), PanelX - 4.f, PanelY - 18.f,
	         PanelW + 8.f, WaveH * 2.f + Gap + 22.f + 8.f);

	// Title + sync number
	const FLinearColor SyncCol = FLinearColor::LerpUsingHSV(
		FLinearColor(0.8f, 0.2f, 0.2f),   // red = desynced
		FLinearColor(0.2f, 1.f,  0.4f),   // green = synced
		FMath::Clamp(SyncLevel, 0.f, 1.f));
	const FString SyncStr = FString::Printf(TEXT("SYNC  %.2f"), SyncLevel);
	DrawText(SyncStr, SyncCol, PanelX, PanelY - 16.f, GEngine->GetSmallFont(), 1.f);

	// ── Wave drawing helper (reads ring buffer from oldest to newest) ─────────
	auto DrawWave = [&](float WaveData[], float BaseY, FLinearColor Col)
	{
		// Background strip
		DrawRect(FLinearColor(0.05f, 0.05f, 0.08f, 1.f), PanelX, BaseY, PanelW, WaveH);
		// Zero line
		DrawRect(FLinearColor(0.2f, 0.2f, 0.25f, 0.6f), PanelX, BaseY + WaveH * 0.5f, PanelW, 1.f);

		// Draw waveform as connected line segments
		for (int32 i = 0; i < WaveHistorySize - 1; ++i)
		{
			// Read from ring buffer oldest-first
			const int32 IdxA = (WaveWriteIdx + i)     % WaveHistorySize;
			const int32 IdxB = (WaveWriteIdx + i + 1) % WaveHistorySize;
			const float ValA = FMath::Clamp(WaveData[IdxA] / 1.5f, 0.f, 1.f);
			const float ValB = FMath::Clamp(WaveData[IdxB] / 1.5f, 0.f, 1.f);
			const float X1   = PanelX + (i     / (float)(WaveHistorySize - 1)) * PanelW;
			const float X2   = PanelX + ((i+1) / (float)(WaveHistorySize - 1)) * PanelW;
			// Draw from bottom up (0 = bottom, 1 = top)
			const float Y1 = BaseY + WaveH - ValA * WaveH;
			const float Y2 = BaseY + WaveH - ValB * WaveH;
			DrawLine(X1, Y1, X2, Y2, Col * FLinearColor(1,1,1, 0.85f), 1.5f);
		}

		// Current value marker (rightmost point)
		const int32 LatestIdx = (WaveWriteIdx + WaveHistorySize - 1) % WaveHistorySize;
		const float LatestVal = FMath::Clamp(WaveData[LatestIdx] / 1.5f, 0.f, 1.f);
		const float MarkerY   = BaseY + WaveH - LatestVal * WaveH;
		DrawRect(Col, PanelX + PanelW - 3.f, MarkerY - 2.f, 4.f, 4.f);
	};

	// Song wave (teal)
	DrawWave(SongWave,   PanelY,            FLinearColor(0.1f, 0.9f, 0.85f));
	DrawText(TEXT("SONG"),   FLinearColor(0.1f, 0.9f, 0.85f, 0.7f), PanelX + 3.f, PanelY + 2.f, GEngine->GetSmallFont(), 1.f);

	// Player wave (gold)
	DrawWave(PlayerWave, PanelY + WaveH + Gap, FLinearColor(1.f, 0.75f, 0.15f));
	DrawText(TEXT("PLAYER"), FLinearColor(1.f, 0.75f, 0.15f, 0.7f), PanelX + 3.f, PanelY + WaveH + Gap + 2.f, GEngine->GetSmallFont(), 1.f);
}

void APcQDebugHUD::DrawCircleHUD(float CX, float CY, float Radius, FLinearColor Color, float Thickness, int32 Segments, float AngleOffset)
{
	if (Segments <= 0) return;
	const float Step = 2.f * PI / (float)Segments;
	for (int32 i = 0; i < Segments; ++i) {
		DrawLine(CX + Radius * FMath::Cos(i * Step + AngleOffset), CY + Radius * FMath::Sin(i * Step + AngleOffset),
		         CX + Radius * FMath::Cos((i + 1) * Step + AngleOffset), CY + Radius * FMath::Sin((i + 1) * Step + AngleOffset), Color, Thickness);
	}
}

void APcQDebugHUD::DrawArcHUD(float CX, float CY, float Radius, float Thickness, float StartAngle, float EndAngle, FLinearColor Color, int32 Segments)
{
	if (Segments <= 0) return;
	const float AngleStep = (EndAngle - StartAngle) / (float)Segments;
	const float InnerR = Radius - Thickness * 0.5f; const float OuterR = Radius + Thickness * 0.5f;

	for (int32 i = 0; i < Segments; ++i) {
		const float A1 = StartAngle + i * AngleStep; const float A2 = StartAngle + (i + 1) * AngleStep;
		DrawLine(CX + InnerR * FMath::Cos(A1), CY + InnerR * FMath::Sin(A1), CX + InnerR * FMath::Cos(A2), CY + InnerR * FMath::Sin(A2), Color, 2.f);
		DrawLine(CX + OuterR * FMath::Cos(A1), CY + OuterR * FMath::Sin(A1), CX + OuterR * FMath::Cos(A2), CY + OuterR * FMath::Sin(A2), Color, 2.f);
	}
	DrawLine(CX + InnerR * FMath::Cos(StartAngle), CY + InnerR * FMath::Sin(StartAngle), CX + OuterR * FMath::Cos(StartAngle), CY + OuterR * FMath::Sin(StartAngle), Color, 2.f);
	DrawLine(CX + InnerR * FMath::Cos(EndAngle), CY + InnerR * FMath::Sin(EndAngle), CX + OuterR * FMath::Cos(EndAngle), CY + OuterR * FMath::Sin(EndAngle), Color, 2.f);
}

void APcQDebugHUD::DrawArcFilled(float CX, float CY, float Radius, float Thickness, float StartAngle, float EndAngle, FLinearColor Color, int32 Segments)
{
	if (Segments <= 0 || Thickness <= 0.f) return;
	const float AngleStep = (EndAngle - StartAngle) / (float)Segments;
	const float HalfThick = Thickness * 0.5f;

	for (float dr = -HalfThick; dr <= HalfThick; dr += 2.2f) {
		const float R = Radius + dr;
		for (int32 i = 0; i < Segments; ++i) {
			const float A1 = StartAngle + i * AngleStep; const float A2 = StartAngle + (i + 1) * AngleStep;
			DrawLine(CX + R * FMath::Cos(A1), CY + R * FMath::Sin(A1), CX + R * FMath::Cos(A2), CY + R * FMath::Sin(A2), Color, 2.5f);
		}
	}
}