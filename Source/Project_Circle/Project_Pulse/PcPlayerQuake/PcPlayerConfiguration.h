#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Curves/CurveFloat.h"
#include "PcPlayerConfiguration.generated.h"

// ---------------------------------------------------------------------------
//  UPcPlayerConfiguration
//
//  Single data asset driving every tunable value in the player system.
//  Assign this to UPcQPlayerMovementComponent::Config in the Blueprint.
//
//  Speed scaling formula:
//    MaxSpeed = BaseMaxSpeed * clamp(CurrentBPM / ReferenceBPM, ScaleMin, ScaleMax)
//
//  Jump timing: durations expressed in beats so they stay musically aligned
//  regardless of BPM. Converted at runtime: actualSec = beats * beatIntervalSec.
// ---------------------------------------------------------------------------

UCLASS(BlueprintType)
class PROJECT_CIRCLE_API UPcPlayerConfiguration : public UDataAsset
{
	GENERATED_BODY()

public:

	// ── BPM Speed Scaling ─────────────────────────────────────────────────────
	// At ReferenceBPM the player moves at BaseMaxSpeed.
	// Faster songs scale up toward ScaleMax, slower toward ScaleMin.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BPM Scaling",
	          meta = (ClampMin = "20", ClampMax = "300"))
	float ReferenceBPM = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BPM Scaling",
	          meta = (ClampMin = "0.3", ClampMax = "1.0",
	                  ToolTip = "Minimum speed multiplier (applied at low BPM)"))
	float SpeedScaleMin = 0.7f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BPM Scaling",
	          meta = (ClampMin = "1.0", ClampMax = "3.0",
	                  ToolTip = "Maximum speed multiplier (applied at high BPM)"))
	float SpeedScaleMax = 1.5f;


	// ── Base Movement ─────────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Base")
	float BaseMaxSpeed = 850.f;       // cm/s at reference BPM

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Base")
	float GroundAcceleration = 30.f;  // VInterpTo speed toward wish dir on ground

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Base")
	float GroundFriction = 25.f;      // VInterpTo speed toward zero with no input

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Base")
	float AirAcceleration = 15.f;     // VInterpTo speed while airborne

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Base",
	          meta = (ClampMin = "1.0", ClampMax = "8.0"))
	float GravityScale = 2.8f;        // applied when not in a jump arc

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Base",
	          meta = (ClampMin = "1.0", ClampMax = "8.0"))
	float HardSpeedCapMult = 4.f;     // absolute max = BaseMaxSpeed × this

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Base")
	float OverspeedDecayRate = 200.f; // cm/s² bleed when above current max


	// ── Jump ──────────────────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Jump")
	float JumpPeakHeightCM = 260.f;

	// Air time in beats. 1.0 = land exactly on the next beat.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Jump",
	          meta = (ClampMin = "0.5", ClampMax = "4.0"))
	float JumpAirTimeBeats = 1.f;

	// Optional smooth arc curve (Y = 0→1→0 height fraction over normalised time).
	// Leave null for standard parabolic arc.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Jump")
	TObjectPtr<UCurveFloat> JumpCurve = nullptr;

	// Extra horizontal speed added when eating a ground pulse (super jump).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Jump")
	float SuperJumpHorizBoost = 420.f;

	// Window before/after a beat where a jump press counts as on-beat.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Jump",
	          meta = (ClampMin = "50", ClampMax = "400"))
	int32 OnBeatWindowMs = 160;

	// How long before landing a jump press is still remembered.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Jump")
	float JumpInputBufferSec = 0.22f;


	// ── Double Jump ───────────────────────────────────────────────────────────
	// DJ gives the same arc height but from the player's current position,
	// maintaining their altitude band without gaining extra height.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|DoubleJump")
	float DJPeakHeightCM = 220.f;

	// Cooldown in beats after using the double jump.
	// CD is a global timer — landing and re-jumping doesn't reset it.
	// Enemy hits can refund it (see NotifyEnemyHit).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|DoubleJump",
	          meta = (ClampMin = "0.5", ClampMax = "8.0"))
	float DJCooldownBeats = 2.f;


	// ── Ground Pound ──────────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|GroundPound")
	float GPSlamSpeed = 2800.f;       // cm/s downward (absolute)

	// How many beats of pulse immunity are granted after a GP land.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|GroundPound",
	          meta = (ClampMin = "0.0", ClampMax = "8.0"))
	float GPPulseImmunityBeats = 2.f;


	// ── Dash Boost ───────────────────────────────────────────────────────────
	// Entered from ground: GP press (bypasses/converts the pulse into a dash).
	// Shooting on beat resets the timer and re-triggers. GP land also auto-enters.
	// No gauge — pure timed burst. Timer is beat-length based so it naturally aligns.

	// Speed = ComputeCurrentMaxSpeed() × this multiplier while dashing.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Dash",
	          meta = (ClampMin = "1.0", ClampMax = "3.0"))
	float DashBoostSpeedMult = 1.55f;

	// How many beats the dash lasts before expiring.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Dash",
	          meta = (ClampMin = "0.25", ClampMax = "4.0"))
	float DashDurationBeats = 1.f;

	// VInterpTo speed while dashing — higher = snappier direction correction.
	// 30 gives the responsive "feel" of the old boost.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Dash")
	float DashSteerAcceleration = 30.f;

	// Horizontal bonus added when jumping out of a dash.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Dash")
	float DashJumpBoost = 200.f;

	// Brief pulse immunity after dash ends so the player isn't immediately bounced.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Dash",
	          meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float PostDashImmunityBeats = 0.25f;
};