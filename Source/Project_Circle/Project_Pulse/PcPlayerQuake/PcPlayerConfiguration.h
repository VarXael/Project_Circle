#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Curves/CurveFloat.h"
#include "PcPlayerConfiguration.generated.h"

class UInputAction;
class UInputMappingContext;

UCLASS(BlueprintType)
class PROJECT_CIRCLE_API UPcPlayerConfiguration : public UDataAsset
{
	GENERATED_BODY()

public:
	// ── Input Mappings ────────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input") UInputMappingContext* DefaultMappingContext;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input") UInputAction* IA_Move;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input") UInputAction* IA_Look;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input") UInputAction* IA_Jump;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input") UInputAction* IA_GroundPound;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input") UInputAction* IA_Dash;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input") UInputAction* IA_AirHop;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input") UInputAction* IA_Fire;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input") float LookSensitivityX = 0.4f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input") float LookSensitivityY = 0.4f;

	// ── BPM & Rhythm ──────────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rhythm", meta=(ClampMin="20", ClampMax="240"))
	float NormalBPM = 50.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rhythm", meta=(ClampMin="0.05", ClampMax="0.50"))
	float OnBeatWindowFraction = 0.20f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rhythm")
	float JumpInputBufferSec = 0.22f;

	UFUNCTION(BlueprintPure) float GetNormalBeatInterval() const { return 60.f / FMath::Max(NormalBPM, 1.f); }

	// ── Base Movement ─────────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement|Base") float BaseMaxSpeed       = 900.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement|Base") float GroundAcceleration = 2400.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement|Base") float GroundFriction     = 2400.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement|Base") float AirAcceleration    = 700.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement|Base") float OverspeedDecayRate = 300.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement|Base") float HardSpeedCapMult   = 4.f;

	// ── Jump ──────────────────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement|Jump") float FallbackNormalJumpHeight = 600.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement|Jump") float BeatJumpHorizBoost       = 300.f;

	// ── Air Hop ───────────────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement|AirHop") float AirHopPeakSpeed       = 1800.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement|AirHop") float AirHopVerticalForce   = 250.f; 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement|AirHop") TObjectPtr<UCurveFloat> AirHopSpeedCurve;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement|AirHop", meta=(ClampMin="0.25", ClampMax="4.0")) float AirHopDurationBeats = 1.0f; 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement|AirHop") float AirHopCooldownBeats = 0.0f;

	// ── Dash Boost ────────────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement|DashBoost") float DashBoostPeakSpeed     = 2200.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement|DashBoost") float DashBoostDurationSec   = 0.35f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement|DashBoost") float DashBoostCooldownBeats = 1.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement|DashBoost") TObjectPtr<UCurveFloat> DashSpeedCurve;

	// ── Ground Pound ──────────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement|GroundPound") float GPDownVelocity         = 2800.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement|GroundPound") float GPLandingComboWindow   = 0.2f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement|GroundPound") float SuperJumpVerticalForce = 1200.f;

	// ── Wall Swim ─────────────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement|Wall") float WallEjectBaseSpeed = 1200.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement|Wall") float WallEjectUpKick    = 450.f;

	// ── Frenzy ────────────────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frenzy") float FrenzyDrainPerSec = 0.04f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frenzy") float FrenzyFillOnBeat  = 0.20f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frenzy|Tiers") float TierC_Threshold = 0.25f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frenzy|Tiers") float TierB_Threshold = 0.50f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frenzy|Tiers") float TierA_Threshold = 0.75f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frenzy|Tiers") float TierS_Threshold = 1.00f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frenzy|Speed") float SpeedMult_D = 1.00f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frenzy|Speed") float SpeedMult_C = 1.15f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frenzy|Speed") float SpeedMult_B = 1.30f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frenzy|Speed") float SpeedMult_A = 1.50f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frenzy|Speed") float SpeedMult_S = 1.80f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frenzy|S-Tier") float STierAutoDashPeakSpeed = 2600.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frenzy|S-Tier") float STierLockTime          = 3.0f;

	// ── UI & Feedback ─────────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Feedback") float OnBeatFlashDuration  = 0.35f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Feedback") float PlayerPulseDecayRate = 3.5f;
};