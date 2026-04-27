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
	// Called by the character on every gameplay beat when S tier is active and grounded
	UFUNCTION(BlueprintCallable, Category = "Movement")  void TriggerFrenzyDashBoost();

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
	UFUNCTION(BlueprintPure) float GetFrenzyGauge()              const { return FrenzyGauge; }
	UFUNCTION(BlueprintPure) float GetFrenzySpeedMult()          const;
	UFUNCTION(BlueprintPure) bool  IsSTierActive()               const { return bSTierActive; }
	UFUNCTION(BlueprintPure) bool  IsSTierLocked()               const { return bSTierLocked; }
	// Returns the next-beat timestamp of the last granted CD reset.
	// Character uses this to determine if the current on-beat action got a free grant.
	UFUNCTION(BlueprintPure) int32 GetLastBeatGrantTimestampMS() const { return LastBeatResetTimestampMS; }

	UFUNCTION(BlueprintPure) float GetBoostCooldownAlpha()      const;
	UFUNCTION(BlueprintPure) float GetBoostActiveAlpha()        const;
	UFUNCTION(BlueprintPure) float GetDoubleJumpCooldownAlpha() const;
	UFUNCTION(BlueprintPure) int32 GetOnBeatWindowMs()          const; // dynamic: fraction of current beat interval
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

	// ── Base speed ────────────────────────────────────────────────────────────
	// MaxWalkSpeed = BaseMaxSpeed * TierMultiplier. Song has no effect on speed.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Speed") float BaseMaxSpeed = 900.f;

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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Bonus Hop")   float BoostJumpExtraSpeed    = 600.f;  // added on top of boost speed on a boost jump

	// On-beat window: scales as a fraction of the current beat interval so it stays
	// proportional at all BPMs. At 75 BPM (800ms) fraction 0.20 = 160ms.
	// At 150 BPM (400ms) fraction 0.20 = 80ms — tighter, harder to spam.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Bonus Hop",
	          meta = (ClampMin = "0.05", ClampMax = "0.5",
	                  ToolTip = "On-beat hit window as a fraction of the beat interval. 0.20 = 20% of the interval."))
	float OnBeatWindowFraction = 0.20f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Jump Arc")    float ReferenceBeatInterval  = 0.55f;

	// ── Power Boost ───────────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Power Boost") float BoostSpeedMultiplier = 1.55f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Power Boost") float BoostBaseDurationSec = 2.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Power Boost") float BoostBaseCooldownSec = 3.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Power Boost") float BoostExtendPerShot   = 0.5f;

	// ── Double Jump ───────────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Double Jump") float DoubleJumpBaseCooldownSec = 2.0f;

	// ── Ground Pound ──────────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Ground Pound") float GroundPoundSlamSpeed      = -2800.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Ground Pound") float GroundPoundCancelDelay    =  0.18f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Ground Pound") float GPCancelMinUpVelocity     =  600.f;  // min upward velocity when cancelling a GP mid-air

	// ── GP Pulse (replaces ground Slide) ─────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|GP Pulse") float GPPulseSpeedBoost    = 700.f;   // cm/s added on top of current MaxWalkSpeed
	// Duration in beats + window tolerance. At 75 BPM, 1 beat ≈ 0.96s total (800ms + 20% window).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|GP Pulse") float GPPulseDurationBeats = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|GP Pulse") float GPPulseOnBeatBonus   = 0.5f;    // fraction of boost added extra when on beat

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

	// ── S Tier (Frenzy) ───────────────────────────────────────────────────────
	// S is only reachable when gauge == 1.0 AND the song is in an Enhanced section.
	// Beat actions reset the lock timer, keeping you in S as long as you play.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Frenzy|S Tier") float STierLockSec        = 3.f;   // beat actions reset this
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Frenzy|S Tier") float STierDrainMult      = 0.1f;  // gauge drains at this fraction of normal after lock expires
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Frenzy|S Tier") float STierExitThreshold  = 0.85f; // drop out below this to avoid flickering
	// Dash boost: fired on every gameplay beat while grounded in S tier.
	// Injects FrenzyDashBoostSpeed above MaxWalkSpeed; decays at FrenzyDashDecayRate
	// so the excess is fully absorbed before the next beat arrives.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Frenzy|S Tier") float FrenzyDashBoostSpeed = 900.f;   // cm/s added above MaxWalkSpeed
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Frenzy|S Tier") float FrenzyDashDecayRate  = 2000.f;  // cm/s² — brings excess back to MaxWalkSpeed

	// ── Super Jump ────────────────────────────────────────────────────────────
	// Triggered by GP Pulse + jump (grounded) or air GP landing + jump.
	// On beat: arc is synced to land exactly 1 beat later.
	// Height = clamp(fallHeight * Mult, Min, Max).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Super Jump") float SuperJumpMinHeightCM = 400.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Super Jump") float SuperJumpMaxHeightCM = 1000.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement|Super Jump") float SuperJumpHeightMult  = 1.5f;

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

	// ── Per-beat CD gate ──────────────────────────────────────────────────────
	// Tracks the next-beat timestamp of the last CD reset grant.
	// TriggerOnBeatFlash only grants CDs once per beat cycle to prevent spamming.
	int32 LastBeatResetTimestampMS = -1;

	// Set false each beat by TriggerOnBeatFlash. Consumed by the first action
	// that uses the on-beat free charge (free DJ, free GP re-trigger, etc.).
	// Prevents infinite free-action looping within a single beat window.
	bool bFreeChargeUsedThisBeat = false;

	// ── GP Pulse ──────────────────────────────────────────────────────────────
	bool    bGPPulseActive   = false;
	float   GPPulseTimer     = 0.f;
	float   GPPulseMaxTimer  = 0.f;
	FVector GPPulseDirection = FVector::ZeroVector;

	// ── Frenzy ────────────────────────────────────────────────────────────────
	float FrenzyGauge = 0.f;

	// ── S Tier ────────────────────────────────────────────────────────────────
	bool  bSTierActive  = false;
	bool  bSTierLocked  = false;
	float STierLockTimer = 0.f;

	// ── Frenzy Dash ───────────────────────────────────────────────────────────
	bool  bFrenzyDashActive = false;
	float FrenzyDashTimer   = 0.f;

	// ── Super Jump ────────────────────────────────────────────────────────────
	float GPInitiatedZ        = 0.f;  // Z when air GP was pressed
	float SuperJumpSourceHeight = 0.f; // height saved at GP landing or 0 for ground GP

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
	void  ActivateSuperJump(bool bOnBeat);

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