#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "PcQPlayerMovementComponent.h"
#include "PcQDebugHUD.generated.h"

class UPcMusicAnalysisSubsystem;
class APcQPlayerCharacter;

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

// One entry in the combo action feed.
USTRUCT()
struct FPcComboFeedEntry
{
	GENERATED_BODY()
	FString      Label;
	FLinearColor Color  = FLinearColor::White;
	float        BornAt = 0.f;  // GetWorld()->GetTimeSeconds() at creation
};

UCLASS()
class PROJECT_CIRCLE_API APcQDebugHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

	// ── Combo feed ───────────────────────────────────────────────────────────
	UFUNCTION() void OnComboEvent(const FString& Label, FLinearColor Color);

	// How long each feed entry stays on screen before fully fading.
	UPROPERTY(EditAnywhere, Category = "Rhythm UI|Combo Feed") float ComboFeed_FadeDuration = 2.4f;
	// Maximum entries shown simultaneously.
	UPROPERTY(EditAnywhere, Category = "Rhythm UI|Combo Feed") int32 ComboFeed_MaxEntries   = 6;

	// Toggle BPM debug row in the bhop panel
	UPROPERTY(EditAnywhere, Category = "Rhythm UI|Debug") bool bShowPlayerBPM = true;

	// ── Threat API ──────────────────────────────────────────────────────────
	UFUNCTION(BlueprintCallable, Category = "Rhythm UI|Threats")
	void RegisterThreat(int32 TimestampMS, const FString& Label, FLinearColor Color);

	UFUNCTION(BlueprintCallable, Category = "Rhythm UI|Threats")
	void PurgeThreat(int32 TimestampMS);

	UPROPERTY(BlueprintReadWrite, Category = "Rhythm UI|Threats")
	TArray<FPcHudThreatEvent> ActiveThreats;

	// ── CROSSHAIR  <<< < • > >>>  ───────────────────────────────────────────
	// Distance of the fixed gate markers from the centre dot (px).
	UPROPERTY(EditAnywhere, Category = "Rhythm UI|Crosshair") float CrosshairGateDist    = 32.f;
	// Pixels between each beat step outside the gate.
	UPROPERTY(EditAnywhere, Category = "Rhythm UI|Crosshair") float CrosshairBeatStep    = 40.f;
	// How many incoming beat chevrons to show.
	UPROPERTY(EditAnywhere, Category = "Rhythm UI|Crosshair") int32 CrosshairBeatsToShow =  2;
	// Chevron tip-to-back depth (px).
	UPROPERTY(EditAnywhere, Category = "Rhythm UI|Crosshair") float CrosshairChevronWidth  = 9.f;
	// Chevron half-height (px).
	UPROPERTY(EditAnywhere, Category = "Rhythm UI|Crosshair") float CrosshairChevronHeight = 7.f;
	// Centre dot half-size (px).
	UPROPERTY(EditAnywhere, Category = "Rhythm UI|Crosshair") float DotSize               = 3.f;

	// ── COMBAT METRONOME (Visor Arch) ────────────────────────────────────────
	UPROPERTY(EditAnywhere, Category = "Rhythm UI|Combat Metronome") float Arc_CenterBelowScreen = 1100.f; 
	UPROPERTY(EditAnywhere, Category = "Rhythm UI|Combat Metronome") float Arc_Radius            = 1260.f; 
	UPROPERTY(EditAnywhere, Category = "Rhythm UI|Combat Metronome") float Arc_Thickness         =    8.f;  
	UPROPERTY(EditAnywhere, Category = "Rhythm UI|Combat Metronome") int32 Arc_BeatsToShow       =    3;
	UPROPERTY(EditAnywhere, Category = "Rhythm UI|Combat Metronome") float Arc_FlashWindowPct    =    0.22f;
	
	// 0.0 = Far Right, 0.5 = Dead Center, 0.9 = Far Left
	UPROPERTY(EditAnywhere, Category = "Rhythm UI|Combat Metronome") float Arc_StrikeGatePercent = 0.88f;

	// ── GLANCE BOARD (Bottom-Left) ──────────────────────────────────────────
	UPROPERTY(EditAnywhere, Category = "Rhythm UI|Glance Board") float GlanceBoard_XOffset        = 70.f;
	UPROPERTY(EditAnywhere, Category = "Rhythm UI|Glance Board") float GlanceBoard_ScreenYPercent = 0.80f;
	UPROPERTY(EditAnywhere, Category = "Rhythm UI|Glance Board") float GlanceBoard_Height         = 180.f;
	UPROPERTY(EditAnywhere, Category = "Rhythm UI|Glance Board") float GlanceBoard_TrackSpacing   =  26.f;
	UPROPERTY(EditAnywhere, Category = "Rhythm UI|Glance Board") int32 GlanceBoard_BeatsToShow    =    4;

private:
	struct FArcGeom
	{
		float CX, CY;       
		float SpawnAngle;   
		float StrikeAngle;  
		float BufferAngle;  
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
	void DrawComboFeed();

	TArray<FPcComboFeedEntry> ComboFeed;
	void DrawAbilityBars(UPcQPlayerMovementComponent* MC, APlayerController* PC);

	// ── SYNC SYSTEM ──────────────────────────────────────────────────────────
	void DrawSyncDebug(UPcMusicAnalysisSubsystem* MusicSub, UPcQPlayerMovementComponent* MC);
	void UpdateSyncWaves(UPcMusicAnalysisSubsystem* MusicSub, UPcQPlayerMovementComponent* MC);

	// Ring buffers — 200 samples at ~20Hz = 10 seconds of history
	static constexpr int32 WaveHistorySize = 200;
	float SongWave[200]   = {};  // decaying note impulse envelope
	float PlayerWave[200] = {};  // player action pulse history
	int32 WaveWriteIdx    = 0;   // current write position in ring buffer
	float WaveSampleTimer = 0.f; // accumulates until next sample tick (every 0.05s)
	float SongPulse       = 0.f; // current song pulse (decays between notes)
	float SyncLevel       = 0.f; // -1 to 1, smoothed correlation of the two waves
	int32 LastNoteIdx     = 0;   // tracks which notes we've already processed
	FLinearColor GetStateColor(EBhopState State) const;

	// Primitives
	void DrawCircleHUD(float CX, float CY, float Radius, FLinearColor Color,
	                   float Thickness, int32 Segments, float AngleOffset = 0.f);
	void DrawArcHUD(float CX, float CY, float Radius, float Thickness,
	                float StartAngle, float EndAngle, FLinearColor Color, int32 Segments);
	void DrawArcFilled(float CX, float CY, float Radius, float Thickness,
	                   float StartAngle, float EndAngle, FLinearColor Color, int32 Segments);
};