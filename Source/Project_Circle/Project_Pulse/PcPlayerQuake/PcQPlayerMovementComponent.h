#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Curves/CurveFloat.h"
#include "Project_Circle/MusicSystem/MusicImportSystem/PcMusicAnalysisTypes.h"
#include "PcQPlayerMovementComponent.generated.h"

// ---------------------------------------------------------------------------
//  Synced Actions — actions that produce a beat-locked arc.
//  Set at the start of the action, cleared on landing.
// ---------------------------------------------------------------------------
UENUM(BlueprintType)
enum class EPcSyncedAction : uint8
{
	None,
	Jump,       // ground jump — arc lands on next beat
	DoubleJump, // mid-air     — arc lands on next beat
	SuperJump,  // GP Pulse + Jump (Step 2)
};

UENUM(BlueprintType)
enum class EBhopState : uint8 { Active, PowerBoost, GroundPounding, WallSwim };

UENUM(BlueprintType)
enum class ESnapAction : uint8
{
	None,
	Jump,        // grounded: powered boost jump
	LandingJump, // near-ground: buffered boost jump on landing
	DoubleJump,  // airborne: boosted DJ
	GPPulse,     // GP Pulse active: re-trigger burst
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBhopChargeUpdated, float, ChargeAlpha);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBhopActivated);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBhopCancelled);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBhopLanded, float, HorizontalSpeed);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWallSwimChanged, float, SwimAlpha);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnActiveBeatAction);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnComboEvent, const FString&, Label, FLinearColor, Color);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSnapStateChanged, ESnapAction, NewAction);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSnapPulse, ESnapAction, SnapAction);

UCLASS(Blueprintable, BlueprintType)
class PROJECT_CIRCLE_API UPcQPlayerMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UPcQPlayerMovementComponent();

	UFUNCTION(BlueprintCallable, Category = "Movement")  void OnJumpPressed();
	UFUNCTION(BlueprintCallable, Category = "Movement")  void OnJumpReleased();
	UFUNCTION(BlueprintCallable, Category = "Movement")  void OnGroundPoundPressed();
	UFUNCTION(BlueprintCallable, Category = "Movement")  void OnGroundPoundReleased();
	UFUNCTION(BlueprintCallable, Category = "Beat Sync") void TriggerBeatJump();
	UFUNCTION(BlueprintCallable, Category = "Snap")      void OnSnapPressed();
	UFUNCTION(BlueprintCallable, Category = "Movement")  void NotifyGunFired();

	UFUNCTION(BlueprintPure) EBhopState      GetBhopState()          const { return BhopState; }
	UFUNCTION(BlueprintPure) float           GetChargeAlpha()        const { return 0.f; }
	UFUNCTION(BlueprintPure) float           GetHorizontalSpeed()    const;
	UFUNCTION(BlueprintPure) bool            IsInBhopChain()         const;
	UFUNCTION(BlueprintPure) bool            HasQueuedJump()         const { return bJumpQueuedForBeat; }
	UFUNCTION(BlueprintPure) bool            IsWallSwimming()        const { return BhopState == EBhopState::WallSwim; }
	UFUNCTION(BlueprintPure) bool            IsPowerBoosting()       const { return BhopState == EBhopState::PowerBoost; }
	UFUNCTION(BlueprintPure) float           GetPlayerBPM()          const { return PlayerBPM; }
	UFUNCTION(BlueprintPure) ESnapAction     GetActiveSnap()         const;
	UFUNCTION(BlueprintPure) EPcSyncedAction GetActiveSyncedAction() const { return ActiveSyncedAction; }
	UFUNCTION(BlueprintPure) float           GetSnapPulseFlash()     const { return SnapPulseTimer > 0.f ? FMath::Clamp(SnapPulseTimer / 0.15f, 0.f, 1.f) : 0.f; }
	UFUNCTION(BlueprintPure) float           GetPlayerPulse()        const { return PlayerPulse; }

	// GP Pulse
	UFUNCTION(BlueprintPure) bool  IsGPPulseActive() const { return bGPPulseActive; }
	UFUNCTION(BlueprintPure) float GetGPPulseAlpha() const
	{
		if (!bGPPulseActive || GPPulseMaxTimer <= 0.f) return 0.f;
		return FMath::Clamp(GPPulseTimer / GPPulseMaxTimer, 0.f, 1.f);
	}

	// Frenzy
	UFUNCTION(BlueprintPure) float GetFrenzyGauge()     const { return FrenzyGauge; }
	UFUNCTION(BlueprintPure) float GetFrenzySpeedMult() const;

	UFUNCTION(BlueprintPure) float GetBoostCooldownAlpha()      const;
	UFUNCTION(BlueprintPure) float GetBoostActiveAlpha()        const;
	UFUNCTION(BlueprintPure) float GetDoubleJumpCooldownAlpha() const;
	UFUNCTION(BlueprintPure) float GetBeatSnappedDuration(float BaseSec) const;
	UFUNCTION(BlueprintPure) float GetScaledSpeed(float BaseSpeed) const;

	UFUNCTION(BlueprintPure) float GetOnBeatFlash() const
	{
		return OnBeatFlashDuration > 0.f ? FMath::Clamp(OnBeatFlashTimer / OnBeatFlashDuration, 0.f, 1.f) : 0.f;
	}
	UFUNCTION(BlueprintPure) float GetCurveJumpProgress() const
	{
		return bUsingJumpCurve ? FMath::Clamp(JumpCurveTimer / JumpCurveTotalTime, 0.f, 1.f) : -1.f;
	}

	UPROPERTY(BlueprintAssignable) FOnBhopChargeUpdated OnBhopChargeUpdated;
	UPROPERTY(BlueprintAssignable) FOnBhopActivated     OnBhopActivated;
	UPROPERTY(BlueprintAssignable) FOnBhopCancelled     OnBhopCancelled;
	UPROPERTY(BlueprintAssignable) FOnBhopLanded        OnBhopLanded;
	UPROPERTY(BlueprintAssignable) FOnWallSwimChanged   OnWallSwimChanged;
	UPROPERTY(BlueprintAssignable) FOnActiveBeatAction  OnActiveBeatAction;
	UPROPERTY(BlueprintAssignable) FOnComboEvent        OnComboEvent;
	UPROPERTY(BlueprintAssignable) FOnSnapStateChanged  OnSnapStateChanged;
	UPROPERTY(BlueprintAssignable) FOnSnapPulse         OnSnapPulse;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Jump Curve")
	TObjectPtr<UCurveFloat> JumpCurve = nullptr;

	// ── Ground movement ───────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Ground") float CustomGroundAcceleration = 30.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Ground") float CustomGroundFriction     = 25.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Air")   float CustomAirAcceleration    = 15.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Air")   float CustomAirFriction        =  0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Queue") float BeatCoyoteWindow         =  0.25f;

	// ── Speed management ──────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Speed") float OverspeedDecayRate = 180.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Speed") float HardSpeedCapMult   =   4.f;

	// ── Jump ──────────────────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Jump Buffer") float JumpInputBufferWindow  = 0.22f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Jump Chain")  float GPJumpHorizBoost       = 400.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Jump Chain")  float GPBoostJumpHorizMult   = 1.8f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Jump Chain")  float GPComboWindowSec       = 0.45f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Bonus Hop")   float BonusHopSpeedBoost     = 300.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Bonus Hop")   int32 OnBeatWindowMS         = 160;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Jump Arc")    float ReferenceBeatInterval  = 0.55f;

	// ── Power Boost ───────────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Power Boost") float BoostSpeedMultiplier = 1.55f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Power Boost") float BoostBaseDurationSec = 2.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Power Boost") float BoostBaseCooldownSec = 3.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Power Boost") float BoostExtendPerShot   = 0.5f;

	// ── Double Jump ───────────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Double Jump") float DoubleJumpBaseCooldownSec = 2.0f;

	// ── Ground Pound ──────────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Ground Pound") float GroundPoundSlamSpeed   = -2800.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Ground Pound") float GroundPoundCancelDelay =  0.18f;

	// ── GP Pulse (replaces ground Slide) ─────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|GP Pulse") float GPPulseSpeedBoost  = 700.f;   // cm/s added on top of current MaxWalkSpeed
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|GP Pulse") float GPPulseDurationSec = 0.35f;   // how long the window stays open
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|GP Pulse") float GPPulseOnBeatBonus = 0.5f;    // fraction of boost added extra when on beat

	// ── Frenzy ────────────────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Frenzy") float FrenzyDrainPerSec = 0.04f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Frenzy") float FrenzyFillOnBeat  = 0.20f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Frenzy") float FrenzyMult_D = 1.00f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Frenzy") float FrenzyMult_C = 1.15f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Frenzy") float FrenzyMult_B = 1.30f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Frenzy") float FrenzyMult_A = 1.50f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Frenzy") float FrenzyMult_S = 1.80f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Frenzy") float FrenzyThresh_C = 0.25f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Frenzy") float FrenzyThresh_B = 0.50f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Frenzy") float FrenzyThresh_A = 0.75f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Frenzy") float FrenzyThresh_S = 1.00f;

	// ── Wall Spring ───────────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Wall") float Wall_EnterMinSpeed    = 200.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Wall") float Wall_CompressionSec   = 0.22f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Wall") float Wall_CompressionDecay =  14.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Wall") float Wall_EjectSpeed       = 1200.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Wall") float Wall_BeatEjectBoost   =  400.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Wall") float Wall_JumpEjectUpKick  =  350.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Wall") float Wall_FloorCheckDist   =  150.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Wall") float Wall_EjectImmunitySec =   0.40f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Wall") float Wall_AutoEjectUpKick  =  450.f;

	// ── Beat feedback ─────────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Feedback") float OnBeatFlashDuration = 0.35f;

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void PhysWalking(float deltaTime, int32 Iterations) override;
	virtual void PhysFalling(float deltaTime, int32 Iterations) override;
	virtual void ProcessLanded(const FHitResult& Hit, float remainingTime, int32 Iterations) override;
	virtual void HandleImpact(const FHitResult& Hit, float TimeSlice, const FVector& MoveDelta) override;

private:
	EBhopState BhopState          = EBhopState::Active;
	bool       bJumpQueuedForBeat = false;
	float      BeatQueueTimer     = 0.f;
	float      PreviousFrameSpeed = 0.f;

	bool  bJumpInputBuffered   = false;
	float JumpInputBufferTimer = 0.f;
	bool  bGPInputBuffered     = false;
	float GPInputBufferTimer   = 0.f;

	bool  bBonusHopRequested = false;
	float SnapPulseTimer     = 0.f;
	float PlayerPulse        = 0.f;

	float PlayerBPM = 0.f;
	static constexpr int32 HitHistorySize = 8;
	float HitTimestamps[8] = {};
	int32 HitWriteIdx      = 0;
	int32 HitCount         = 0;

	float GPCancelTimer        = 0.f;
	bool  bGPLandedRecently    = false;
	bool  bBoostActiveOnGPLand = false;
	float GPComboTimer         = 0.f;

	float BoostTimer    = 0.f;
	float BoostCooldown = 0.f;

	bool  bDoubleJumpUsed    = false;
	float DoubleJumpCooldown = 0.f;

	float OnBeatFlashTimer = 0.f;

	// ── GP Pulse ──────────────────────────────────────────────────────────────
	bool    bGPPulseActive   = false;
	float   GPPulseTimer     = 0.f;
	float   GPPulseMaxTimer  = 0.f;
	FVector GPPulseDirection = FVector::ZeroVector;

	// ── Frenzy ────────────────────────────────────────────────────────────────
	float FrenzyGauge = 0.f;

	// ── Synced Action ─────────────────────────────────────────────────────────
	EPcSyncedAction ActiveSyncedAction = EPcSyncedAction::None;

	// ── Jump curve ────────────────────────────────────────────────────────────
	bool  bUsingJumpCurve     = false;
	float JumpCurveTimer      = 0.f;
	float JumpCurveTotalTime  = 0.f;
	float JumpCurvePeakHeight = 0.f;
	float JumpCurveLaunchZ    = 0.f;

	// ── Wall spring ───────────────────────────────────────────────────────────
	FName   WallPrevCollisionProfile = NAME_None;
	FVector WallEntryNormal          = FVector::ZeroVector;
	float   WallEntrySpeed           = 0.f;
	float   WallCompressionTimer     = 0.f;
	bool    bWallBeatPending         = false;
	float   WallEjectImmunityTimer   = 0.f;

	void  TriggerOnBeatFlash();
	void  RecordHit();
	void  PushCombo(const FString& Label, FLinearColor Color);
	void  OnSnapPressed_Internal();
	void  ActivateGPPulse(bool bOnBeat);

	bool  IsOnBeat()   const;
	bool  BoostReady() const { return BoostCooldown <= 0.f && BhopState != EBhopState::PowerBoost; }
	bool  DJumpReady() const { return DoubleJumpCooldown <= 0.f && !bDoubleJumpUsed; }
	bool  CanBufferLanding() const;

	void  ActivateBoost();
	void  ExitBoost();
	void  ExecutePlayerJump(bool bFromBoost = false);

	void ApplyJumpVelocity();
	void ApplyFixedBeatJump();
	void ApplyArcWithAirTime(float PeakHeightCM, float AirTimeSec);
	void ExitCurveJump();

	void  EnterWallSpring(const FHitResult& Hit);
	void  EjectFromWall(bool bBeatBoost, bool bJumpEject);
	bool  IsAboveGround() const;
};