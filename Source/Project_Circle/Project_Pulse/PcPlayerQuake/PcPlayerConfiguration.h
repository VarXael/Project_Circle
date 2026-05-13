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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BPM Scaling") float SpeedScaleMin = 0.7f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BPM Scaling") float SpeedScaleMax = 1.5f;

	// ── Base Movement ─────────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Base") float BaseMaxSpeed = 1000.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Base") float GroundAcceleration = 30.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Base") float GroundFriction = 25.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Base") float AirAcceleration = 15.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Base") float HardSpeedCapMult = 4.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Base") float OverspeedDecayRate = 200.f;

	// ── Adaptive Jump ─────────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Jump") float JumpPeakHeightCM = 260.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Jump") bool bSyncJumpToBeat = false; 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Jump") float IdealJumpAirTimeSec = 0.85f; 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Jump") float SuperJumpHorizBoost = 420.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Jump") int32 OnBeatWindowMs = 160;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Jump") float JumpInputBufferSec = 0.2f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Jump") float MagneticSlamDownforce = 2500.f;

	// ── Adaptive Double Jump ──────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|DoubleJump") int32 MaxDoubleJumps = 2;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|DoubleJump") float DJPeakHeightCM = 350.f; 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|DoubleJump") float IdealDJAirTimeSec = 0.5f;

	// ── Ground Pound & Pulse ──────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|GroundPound") float GPSlamSpeed = 2800.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|GroundPound") float GPPulseImmunityBeats = 2.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|GroundPound") float IdealGroundPulseDurationSec = 0.6f;

	// ── Dash ──────────────────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Dash") float DashBoostSpeedMult = 1.55f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Dash") float IdealDashDurationSec = 0.35f; 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Dash") float DashSteerAcceleration = 30.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Dash") float DashJumpBoost = 200.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Dash") float PostDashImmunityBeats = 0.25f;
};