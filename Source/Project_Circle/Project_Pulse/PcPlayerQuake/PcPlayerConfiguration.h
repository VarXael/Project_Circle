#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Base") float BaseMaxSpeed = 1000.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Base") float GroundAcceleration = 30.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Base") float GroundFriction = 25.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Base") float AirAcceleration = 15.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Base") float HardSpeedCapMult = 4.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Base") float OverspeedDecayRate = 200.f;

	// ── Adaptive Jump (DICTATES WORLD GRAVITY) ────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Jump", meta = (ToolTip = "If Gravity feels too heavy, lower this height or increase the Ideal Air Time!")) 
	float JumpPeakHeightCM = 260.f;

	// FIX: Restored this variable so Jump Magnetism compiles and functions!
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Jump", meta = (ToolTip = "Set to true to dynamically snap Jump heights to land perfectly on the beat. False = Pure Physics.")) 
	bool bSyncJumpToBeat = false; 

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Jump") float IdealJumpAirTimeSec = 0.85f; 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Jump") float SuperJumpHorizBoost = 420.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Jump") int32 OnBeatWindowMs = 160;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Jump") float JumpInputBufferSec = 0.02f;

	// ── Adaptive Double Jump (Charges) ────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|DoubleJump") int32 MaxDoubleJumps = 2;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|DoubleJump", meta = (ToolTip = "Shorter times yield gentler hops, as it uses the same gravity as the main jump.")) 
	float IdealDJAirTimeSec = 0.5f;

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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|KineticSlash") float SlashCooldownSec = 0.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|KineticSlash", meta = (ToolTip = "Exactly how far the player travels (in cm) during the slash.")) float SlashLungeDistance = 525.f; 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|KineticSlash") float SlashLungeDurationSec = 0.15f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|KineticSlash") float SlashBopEnemyLift = 700.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|KineticSlash") float SlashBopWallLift = 500.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|KineticSlash", meta = (ClampMin = "0.0", ClampMax = "1.0")) float SlashBopHorizRetain = 0.2f;
};