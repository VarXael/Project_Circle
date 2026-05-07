#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "PcQPlayerMovementComponent.h"
#include "PcQDebugHUD.generated.h"

class UPcMusicAnalysisSubsystem;
class APcQPlayerCharacter;

USTRUCT(BlueprintType)
struct FPcHudThreatEvent
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Threat") int32        TimestampMS = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Threat") FString      Label       = TEXT("THREAT");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Threat") FLinearColor Color       = FLinearColor(1.f, 0.12f, 0.22f, 1.f);
};

USTRUCT()
struct FPcComboFeedEntry
{
	GENERATED_BODY()
	FString      Label;
	FLinearColor Color  = FLinearColor::White;
	float        BornAt = 0.f;
};

UCLASS()
class PROJECT_CIRCLE_API APcQDebugHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;
	void DrawDashBoostBar(UPcQPlayerMovementComponent* MC, float CX, float BeatFlash);

	// ── Combo feed ───────────────────────────────────────────────────────────
	UFUNCTION() void OnComboEvent(const FString& Label, FLinearColor Color);
	UPROPERTY(EditAnywhere, Category = "Rhythm UI|Combo Feed") float ComboFeed_FadeDuration = 2.4f;
	UPROPERTY(EditAnywhere, Category = "Rhythm UI|Combo Feed") int32 ComboFeed_MaxEntries   = 6;

	// ── Threat API ───────────────────────────────────────────────────────────
	UFUNCTION(BlueprintCallable, Category = "Rhythm UI|Threats") void RegisterThreat(int32 TimestampMS, const FString& Label, FLinearColor Color);
	UFUNCTION(BlueprintCallable, Category = "Rhythm UI|Threats") void PurgeThreat(int32 TimestampMS);
	UPROPERTY(BlueprintReadWrite, Category = "Rhythm UI|Threats") TArray<FPcHudThreatEvent> ActiveThreats;

	// ── Crosshair ────────────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, Category = "Rhythm UI|Crosshair") float CrosshairGateDist      = 32.f;
	UPROPERTY(EditAnywhere, Category = "Rhythm UI|Crosshair") float CrosshairBeatStep      = 40.f;
	UPROPERTY(EditAnywhere, Category = "Rhythm UI|Crosshair") int32 CrosshairBeatsToShow   =  2;
	UPROPERTY(EditAnywhere, Category = "Rhythm UI|Crosshair") float CrosshairChevronWidth  =  9.f;
	UPROPERTY(EditAnywhere, Category = "Rhythm UI|Crosshair") float CrosshairChevronHeight =  7.f;
	UPROPERTY(EditAnywhere, Category = "Rhythm UI|Crosshair") float DotSize               =  3.f;

	// ── Arc Metronome ────────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, Category = "Rhythm UI|Combat Metronome") float Arc_CenterBelowScreen = 1100.f;
	UPROPERTY(EditAnywhere, Category = "Rhythm UI|Combat Metronome") float Arc_Radius            = 1260.f;
	UPROPERTY(EditAnywhere, Category = "Rhythm UI|Combat Metronome") float Arc_Thickness         =    8.f;
	UPROPERTY(EditAnywhere, Category = "Rhythm UI|Combat Metronome") int32 Arc_BeatsToShow       =    3;
	UPROPERTY(EditAnywhere, Category = "Rhythm UI|Combat Metronome") float Arc_FlashWindowPct    =    0.22f;
	UPROPERTY(EditAnywhere, Category = "Rhythm UI|Combat Metronome") float Arc_StrikeGatePercent =    0.88f;

	// ── Glance Board ─────────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, Category = "Rhythm UI|Glance Board") float GlanceBoard_XOffset        = 70.f;
	UPROPERTY(EditAnywhere, Category = "Rhythm UI|Glance Board") float GlanceBoard_ScreenYPercent = 0.80f;
	UPROPERTY(EditAnywhere, Category = "Rhythm UI|Glance Board") float GlanceBoard_Height         = 180.f;
	UPROPERTY(EditAnywhere, Category = "Rhythm UI|Glance Board") float GlanceBoard_TrackSpacing   =  26.f;
	UPROPERTY(EditAnywhere, Category = "Rhythm UI|Glance Board") int32 GlanceBoard_BeatsToShow    =    4;

private:
	struct FArcGeom { float CX, CY, SpawnAngle, StrikeAngle, BufferAngle; };
	FArcGeom BuildArcGeom() const;

	// Draw helpers — most code is unchanged from before
	void DrawDotCrosshair(float BeatRemainingFraction);
	void DrawArcMetronome(UPcMusicAnalysisSubsystem* MusicSub, const FArcGeom& G,
	                      int32 CurrentTimeMS, int32 NextBeatMS, float IntervalMS,
	                      float FlashHard, float FlashSoft);
	void DrawThreatNote(float NoteX, float NoteY, float Alpha, bool bPassed,
	                    const FPcHudThreatEvent& Threat, const FArcGeom& G);
	void DrawGlanceBoard(UPcMusicAnalysisSubsystem* MusicSub,
	                     int32 CurrentTimeMS, int32 NextBeatMS, float IntervalMS, float FlashHard);

	// Updated for new state machine
	void DrawMovementDebug(UPcQPlayerMovementComponent* MC);
	void DrawSlideGaugeBar(UPcQPlayerMovementComponent* MC, float CX, float BeatFlash);
	void DrawAbilityBars(UPcQPlayerMovementComponent* MC, APlayerController* PC);
	void DrawComboFeed();

	TArray<FPcComboFeedEntry> ComboFeed;

	// Returns a color for each movement state (for the debug panel)
	FLinearColor GetStateColor(EPlayerMovementState State) const;

	// Primitives
	void DrawCircleHUD(float CX, float CY, float Radius, FLinearColor Color,
	                   float Thickness, int32 Segments, float AngleOffset = 0.f);
	void DrawArcHUD(float CX, float CY, float Radius, float Thickness,
	                float StartAngle, float EndAngle, FLinearColor Color, int32 Segments);
	void DrawArcFilled(float CX, float CY, float Radius, float Thickness,
	                   float StartAngle, float EndAngle, FLinearColor Color, int32 Segments);
};