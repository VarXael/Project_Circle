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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Base") float GravityScale = 2.8f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Base") float HardSpeedCapMult = 4.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Base") float OverspeedDecayRate = 200.f;

	// ── Jump ──────────────────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Jump") float JumpPeakHeightCM = 260.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Jump") float JumpAirTimeBeats = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Jump") TObjectPtr<UCurveFloat> JumpCurve = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Jump") float SuperJumpHorizBoost = 420.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Jump") int32 OnBeatWindowMs = 160;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Jump") float JumpInputBufferSec = 0.22f;

	// ── Double Jump ───────────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|DoubleJump") float DJPeakHeightCM = 220.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|DoubleJump") float DJCooldownBeats = 2.f;

	// ── Ground Pound ──────────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|GroundPound") float GPSlamSpeed = 2800.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|GroundPound") float GPPulseImmunityBeats = 2.f;

	// ── Dash Boost ───────────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Dash") float DashBoostSpeedMult = 1.55f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Dash") float DashDurationBeats = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Dash") float DashSteerAcceleration = 30.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Dash") float DashJumpBoost = 200.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Dash") float PostDashImmunityBeats = 0.25f;

	// ── Phase C: Sword ────────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Sword") float SwordLungeSpeed = 3500.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Sword") float SwordLungeDurationSec = 0.15f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Sword") float SwordBopEnemyLift = 700.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Sword") float SwordBopWallLift = 500.f;
	
	// Percentage of horizontal speed kept after a bop (e.g. 0.2 = 20%)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Sword", meta = (ClampMin = "0.0", ClampMax = "1.0")) 
	float SwordBopHorizRetain = 0.2f;
};