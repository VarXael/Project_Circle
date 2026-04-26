#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Curves/CurveFloat.h"
#include "PcPlayerConfiguration.generated.h"

// ---------------------------------------------------------------------------
//  PcPlayerConfiguration  —  single data asset driving every tunable value
//  in the player movement system.
//
//  Durations are expressed in BEATS relative to NormalBPM so you always know
//  "0.5 beats at 50 BPM = 0.6 seconds" without mental arithmetic.
//  Runtime converts them:  actualSec = beatFraction * (60f / currentBPM)
// ---------------------------------------------------------------------------
UCLASS(BlueprintType)
class PROJECT_CIRCLE_API UPcPlayerConfiguration : public UDataAsset
{
	GENERATED_BODY()

public:

	// ── BPM ──────────────────────────────────────────────────────────────────
	// Your target "normal" BPM.  On song load the system snaps this to the
	// song's slowest section BPM automatically.
	// FastBPM = NormalBPM × 2  (always, no separate property).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BPM",
	          meta=(ClampMin="20", ClampMax="240"))
	float NormalBPM = 50.f;

	// Helper — call at runtime to get the beat interval in seconds
	UFUNCTION(BlueprintPure) float GetNormalBeatInterval() const
		{ return 60.f / FMath::Max(NormalBPM, 1.f); }
	UFUNCTION(BlueprintPure) float GetFastBeatInterval() const
		{ return 60.f / FMath::Max(NormalBPM * 2.f, 1.f); }


	// ── Base Movement ─────────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement|Base")
	float BaseMaxSpeed         = 900.f;   // cm/s, Tier-D baseline

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement|Base")
	float GroundAcceleration   = 2400.f;  // cm/s² when moving toward desired vel

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement|Base")
	float GroundFriction       = 2400.f;  // cm/s² deceleration with no input

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement|Base")
	float AirAcceleration      = 700.f;   // cm/s² air-strafe acceleration

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement|Base")
	float OverspeedDecayRate   = 300.f;   // cm/s² decay when above current max

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement|Base",
	          meta=(ClampMin="1.0", ClampMax="10.0"))
	float HardSpeedCapMult     = 4.f;     // absolute max = BaseMaxSpeed × this

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement|Base",
	          meta=(ClampMin="1.0", ClampMax="6.0"))
	float GravityScale         = 3.f;     // multiplier on −9.8 (Reaver style)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement|Base")
	float MaxFallSpeed         = 3000.f;  // cm/s, abs. clamp on downward velocity


	// ── Jump ──────────────────────────────────────────────────────────────────
	// Air time = JumpDurationBeats × currentBeatInterval
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement|Jump",
	          meta=(ClampMin="0.25", ClampMax="4.0"))
	float JumpDurationBeats    = 1.f;

	// Horizontal speed added when jumping on beat (added to current XZ speed)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement|Jump")
	float BeatJumpHorizBoost   = 300.f;

	// Extra horizontal on top of BeatJumpHorizBoost when in Frenzy S
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement|Jump")
	float FrenzyBoostedJumpExtraHoriz = 300.f;

	// Optional smooth arc curve (Y = 0→1→0 height fraction over normalized time)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement|Jump")
	TObjectPtr<UCurveFloat> JumpCurve = nullptr;


	// ── Double Jump ───────────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement|DoubleJump",
	          meta=(ClampMin="0.25", ClampMax="4.0"))
	float DoubleJumpDurationBeats = 1.f;

	// Ratio of DJ vertical force vs regular jump (0.8 = 80% as high)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement|DoubleJump",
	          meta=(ClampMin="0.1", ClampMax="2.0"))
	float DoubleJumpHeightRatio = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement|DoubleJump")
	float BeatDoubleJumpHorizBoost = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement|DoubleJump")
	float FrenzyBoostedDJExtraHoriz = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement|DoubleJump",
	          meta=(ClampMin="0.25", ClampMax="8.0"))
	float DoubleJumpCooldownBeats = 2.f;


	// ── Ground Pound ──────────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement|GroundPound")
	float GPDownVelocity = 2800.f;        // cm/s downward (positive value)

	// GP Pulse (ground GP): velocity burst in WASD direction
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement|GroundPound")
	float GPPulseSpeedBoost = 700.f;      // cm/s overshoot above current max

	// Speed decays from (currentMax + GPPulseSpeedBoost) to currentMax over this
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement|GroundPound",
	          meta=(ClampMin="0.1", ClampMax="4.0"))
	float GPPulseDurationBeats = 0.5f;

	// GP Pulse + Jump → Super Jump
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement|GroundPound")
	float SuperJumpVerticalForce  = 1800.f; // cm/s
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement|GroundPound")
	float SuperJumpOnBeatBonus    =  400.f; // added if triggered on beat

	// Synced air GP bounce: max height you'll get back (in beats of height)
	// At cap, the bounce air time = exactly 1 beat.  Below cap, proportional.
	// HeightCap = g × beatInterval² / 8  for your reference, but the config
	// value below is a hard cm cap.  Set to 0 to let physics handle it freely.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement|GroundPound")
	float GPSyncedBounceHeightCapCm = 800.f;


	// ── Frenzy (Music Frenzy) ─────────────────────────────────────────────────
	// Gauge 0→1, D→C→B→A→S tiers.

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frenzy")
	float FrenzyDrainPerSec = 0.04f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frenzy",
	          meta=(ClampMin="0.0", ClampMax="1.0"))
	float FrenzyFillOnBeat  = 0.20f;   // action on beat

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frenzy",
	          meta=(ClampMin="0.0", ClampMax="1.0"))
	float FrenzyFillOffBeat = 0.05f;   // action off beat

	// Thresholds to enter each tier
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frenzy|Tiers")
	float TierC_Threshold = 0.25f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frenzy|Tiers")
	float TierB_Threshold = 0.50f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frenzy|Tiers")
	float TierA_Threshold = 0.75f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frenzy|Tiers")
	float TierS_Threshold = 1.00f;

	// Max speed multiplier per tier (applied to BaseMaxSpeed)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frenzy|Speed") float SpeedMult_D = 1.00f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frenzy|Speed") float SpeedMult_C = 1.15f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frenzy|Speed") float SpeedMult_B = 1.30f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frenzy|Speed") float SpeedMult_A = 1.50f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frenzy|Speed") float SpeedMult_S = 1.80f;

	// Ground friction scales DOWN with frenzy (at S it feels like ice)
	// 0 = no friction reduction at max gauge, 1 = zero friction at max gauge
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frenzy|Speed",
	          meta=(ClampMin="0.0", ClampMax="1.0"))
	float FrenzyFrictionReductionAtS = 0.85f;

	// Dash boost (Frenzy S, fires on each gameplay beat while grounded)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frenzy|DashBoost")
	float DashBoostSpeed = 900.f;   // cm/s injected in WASD dir


	// ── Rhythm ────────────────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rhythm",
	          meta=(ClampMin="50", ClampMax="400"))
	int32 OnBeatWindowMs = 160;

	// How long before landing a jump press is still remembered
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rhythm")
	float JumpInputBufferSec = 0.22f;
};