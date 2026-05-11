#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Curves/CurveFloat.h"
#include "PcPlayerConfiguration.generated.h"

UCLASS(BlueprintType)
class PROJECT_CIRCLE_API UPcPlayerConfiguration : public UDataAsset
{
	GENERATED_BODY()

public:

	// ── BPM Speed Scaling ─────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BPM Scaling", meta = (ClampMin = "20", ClampMax = "300"))
	float ReferenceBPM = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BPM Scaling", meta = (ClampMin = "0.3", ClampMax = "1.0"))
	float SpeedScaleMin = 0.7f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BPM Scaling", meta = (ClampMin = "1.0", ClampMax = "3.0"))
	float SpeedScaleMax = 1.5f;

	// ── Base Movement ─────────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Base") float BaseMaxSpeed = 850.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Base") float GroundAcceleration = 30.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Base") float GroundFriction = 25.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Base") float AirAcceleration = 15.f;
	
	// REDUCED GRAVITY for floatier, calmer movement
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Base") float GravityScale = 1.8f; 
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Base") float HardSpeedCapMult = 4.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Base") float OverspeedDecayRate = 200.f;

	// ── Adaptive Jump ─────────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Jump") float JumpPeakHeightCM = 260.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Jump", meta = (ToolTip = "Ideal real-world time. Dynamically snaps to nearest music beat.")) 
	float IdealJumpAirTimeSec = 0.65f; 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Jump") TObjectPtr<UCurveFloat> JumpCurve = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Jump") float SuperJumpHorizBoost = 420.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Jump") int32 OnBeatWindowMs = 160;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Jump") float JumpInputBufferSec = 0.22f;

	// ── Adaptive Double Jump (Charges) ────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|DoubleJump") int32 MaxDoubleJumps = 2;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|DoubleJump") float DJPeakHeightCM = 150.f; // Gentler hop
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|DoubleJump") float IdealDJAirTimeSec = 0.5f;

	// ── Ground Pound & Pulse ──────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|GroundPound") float GPSlamSpeed = 2800.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|GroundPound") float GPPulseImmunityBeats = 2.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|GroundPound") float IdealGroundPulseDurationSec = 0.6f;

	// ── Adaptive Dash Boost ───────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Dash") float DashBoostSpeedMult = 1.55f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Dash") float IdealDashDurationSec = 0.35f; 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Dash") float DashSteerAcceleration = 30.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Dash") float DashJumpBoost = 200.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Dash") float PostDashImmunityBeats = 0.25f;

	// ── Kinetic Slash ─────────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|KineticSlash") float SlashCooldownSec = 2.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|KineticSlash") float SlashLungeSpeed = 3500.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|KineticSlash") float SlashLungeDurationSec = 0.15f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|KineticSlash") float SlashBopEnemyLift = 700.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|KineticSlash") float SlashBopWallLift = 500.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|KineticSlash", meta = (ClampMin = "0.0", ClampMax = "1.0")) float SlashBopHorizRetain = 0.2f;
};