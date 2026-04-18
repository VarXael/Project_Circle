#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "PcQPlayerMovementComponent.h"
#include "PcQDebugHUD.generated.h"

class UPcMusicAnalysisSubsystem;

// ---------------------------------------------------------------------------
//  Registered by an enemy during its telegraph phase.
//  Call RegisterThreat() to inject, PurgeThreat() when the attack resolves.
// ---------------------------------------------------------------------------
USTRUCT(BlueprintType)
struct FPcHudThreatEvent
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Threat") int32        TimestampMS = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Threat") FString      Label       = TEXT("THREAT");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Threat") FLinearColor Color       = FLinearColor(1.f, 0.12f, 0.22f, 1.f);
};

UCLASS()
class PROJECT_CIRCLE_API APcQDebugHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

	// ── Threat API ──────────────────────────────────────────────────────────
	UFUNCTION(BlueprintCallable, Category = "Rhythm UI|Threats")
	void RegisterThreat(int32 TimestampMS, const FString& Label, FLinearColor Color);

	UFUNCTION(BlueprintCallable, Category = "Rhythm UI|Threats")
	void PurgeThreat(int32 TimestampMS);

	UPROPERTY(BlueprintReadWrite, Category = "Rhythm UI|Threats")
	TArray<FPcHudThreatEvent> ActiveThreats;

	// ── CROSSHAIR ───────────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, Category = "Rhythm UI|Crosshair") float        InnerRingRadius    =  7.f;
	UPROPERTY(EditAnywhere, Category = "Rhythm UI|Crosshair") float        InnerRingThickness =  1.2f;
	UPROPERTY(EditAnywhere, Category = "Rhythm UI|Crosshair") float        DotSize            =  2.f;
	UPROPERTY(EditAnywhere, Category = "Rhythm UI|Crosshair") FLinearColor CrosshairColor     = FLinearColor(1.f, 1.f, 1.f, 0.92f);

	// The beat-countdown sweep ring: sweeps counterclockwise from 12-o-clock,
	// draining from a full circle to nothing over one beat cycle.
	// Gives the player an instant read of "time remaining until pulse" without
	// any pulsing or visual noise.
	UPROPERTY(EditAnywhere, Category = "Rhythm UI|Crosshair") float SweepRingRadius    = 18.f;
	UPROPERTY(EditAnywhere, Category = "Rhythm UI|Crosshair") float SweepRingThickness =  2.f;

	// Cardinal tick lengths (fixed — no animation)
	UPROPERTY(EditAnywhere, Category = "Rhythm UI|Crosshair") float TickOuter = 15.f;
	UPROPERTY(EditAnywhere, Category = "Rhythm UI|Crosshair") float TickInner =  9.f;

	// ── COMBAT METRONOME (Visor Arch) ────────────────────────────────────────
	//
	//  The arc centre sits BELOW the screen bottom.  Only the shallow peak of
	//  a large circle is visible, giving a wide flat visor look instead of a
	//  tall horseshoe.
	//
	//  Quick tuning guide:
	//    Peak height above screen bottom  ≈  Arc_Radius − Arc_CenterBelowScreen
	//    Arc spans more of the screen     →  increase both values proportionally
	//    Arch feels too tall / round      →  increase Arc_CenterBelowScreen
	//
	//  Defaults: peak ≈ 200 px, arch spans roughly 80 % of screen width.
	//
	UPROPERTY(EditAnywhere, Category = "Rhythm UI|Combat Metronome") float Arc_CenterBelowScreen = 1100.f;  // was 1010 — pushes arch lower
	UPROPERTY(EditAnywhere, Category = "Rhythm UI|Combat Metronome") float Arc_Radius            = 1260.f;  // was 1210 — peak ≈ 160 px above screen bottom
	UPROPERTY(EditAnywhere, Category = "Rhythm UI|Combat Metronome") float Arc_Thickness         =    8.f;  // was 9 — slightly slimmer band
	UPROPERTY(EditAnywhere, Category = "Rhythm UI|Combat Metronome") int32 Arc_BeatsToShow       =    3;
	UPROPERTY(EditAnywhere, Category = "Rhythm UI|Combat Metronome") float Arc_FlashWindowPct    =    0.22f;

	// Radians left of the arch's topmost point where the strike gate sits.
	// 0.18 rad ≈ 10°.  Increase to push the gate further left (larger past zone).
	UPROPERTY(EditAnywhere, Category = "Rhythm UI|Combat Metronome") float Arc_StrikeOffsetLeft  = 0.18f;

	// ── GLANCE BOARD (Bottom-Left) ──────────────────────────────────────────
	UPROPERTY(EditAnywhere, Category = "Rhythm UI|Glance Board") float GlanceBoard_XOffset        = 70.f;
	UPROPERTY(EditAnywhere, Category = "Rhythm UI|Glance Board") float GlanceBoard_ScreenYPercent = 0.80f;
	UPROPERTY(EditAnywhere, Category = "Rhythm UI|Glance Board") float GlanceBoard_Height         = 180.f;
	UPROPERTY(EditAnywhere, Category = "Rhythm UI|Glance Board") float GlanceBoard_TrackSpacing   =  26.f;
	UPROPERTY(EditAnywhere, Category = "Rhythm UI|Glance Board") int32 GlanceBoard_BeatsToShow    =    4;

private:
	// Geometry for the visor arch, computed once per frame.
	struct FArcGeom
	{
		float CX, CY;       // arc centre (CY is below Canvas->SizeY)
		float SpawnAngle;   // right limb — where notes enter from
		float StrikeAngle;  // gate marker
		float BufferAngle;  // left limb — where notes exit
	};
	FArcGeom BuildArcGeom() const;

	void DrawDotCrosshair(float BeatRemainingFraction);
	void DrawArcMetronome(UPcMusicAnalysisSubsystem* MusicSub, const FArcGeom& G,
	                      int32 CurrentTimeMS, int32 NextBeatMS, float IntervalMS,
	                      float FlashHard, float FlashSoft);
	void DrawThreatNote(float NoteX, float NoteY, float Alpha, bool bPassed,
	                    const FPcHudThreatEvent& Threat, const FArcGeom& G);
	void DrawGlanceBoard(UPcMusicAnalysisSubsystem* MusicSub,
	                     int32 CurrentTimeMS, int32 NextBeatMS, float IntervalMS,
	                     float FlashHard);
	void DrawBhopDebug(UPcQPlayerMovementComponent* MC);
	FLinearColor GetStateColor(EBhopState State) const;

	// Primitives
	void DrawCircleHUD(float CX, float CY, float Radius, FLinearColor Color,
	                   float Thickness, int32 Segments, float AngleOffset = 0.f);
	void DrawArcHUD(float CX, float CY, float Radius, float Thickness,
	                float StartAngle, float EndAngle, FLinearColor Color, int32 Segments);
	void DrawArcFilled(float CX, float CY, float Radius, float Thickness,
	                   float StartAngle, float EndAngle, FLinearColor Color, int32 Segments);
};