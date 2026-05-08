#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Curves/CurveFloat.h"
#include "PcPlayerConfiguration.h"
#include "PcQPlayerMovementComponent.generated.h"

// ---------------------------------------------------------------------------
//  Player movement state.
//  Grounded      — on surface, subject to ground pulses.
//  InAir         — airborne; double jump available if cooldown is clear.
//  GroundPounding — slamming down, no air control.
//  Dashing       — post-GP or post-GP-land burst; pulse immune; shoot on beat
//                  to reset timer. Responsive steering, NOT a slide.
// ---------------------------------------------------------------------------
UENUM(BlueprintType)
enum class EPlayerMovementState : uint8
{
	Grounded       UMETA(DisplayName = "Grounded"),
	InAir          UMETA(DisplayName = "In Air"),
	GroundPounding UMETA(DisplayName = "Ground Pounding"),
	Dashing        UMETA(DisplayName = "Dashing")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnComboEvent,   const FString&, Label, FLinearColor, Color);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSuperJumped);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDoubleJumped);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDashStarted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDashEnded);

UCLASS(Blueprintable, BlueprintType)
class PROJECT_CIRCLE_API UPcQPlayerMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UPcQPlayerMovementComponent();

	// ── Input API ─────────────────────────────────────────────────────────────
	UFUNCTION(BlueprintCallable, Category = "Movement") void OnJumpPressed();
	UFUNCTION(BlueprintCallable, Category = "Movement") void OnJumpReleased();
	UFUNCTION(BlueprintCallable, Category = "Movement") void OnGroundPoundPressed();

	// Called once per gameplay beat by the character's OnGameplayBeat handler.
	UFUNCTION(BlueprintCallable, Category = "Beat") void TriggerGroundPulse();

	// Called by the character every time the player fires.
	// bWasOnBeat: if true, resets/starts dash AND resets DJ cooldown.
	UFUNCTION(BlueprintCallable, Category = "Combat") void NotifyGunFired(bool bWasOnBeat);

	// ── State Queries ─────────────────────────────────────────────────────────
	UFUNCTION(BlueprintPure) EPlayerMovementState GetMovementState()   const { return MovState; }
	UFUNCTION(BlueprintPure) float                GetHorizontalSpeed() const;
	UFUNCTION(BlueprintPure) bool                 HasDoubleJump()      const { return bDJAvailable; }
	UFUNCTION(BlueprintPure) float                GetDJCooldownAlpha() const;
	UFUNCTION(BlueprintPure) bool                 IsDashing()          const { return MovState == EPlayerMovementState::Dashing; }
	UFUNCTION(BlueprintPure) bool                 IsGroundPounding()   const { return MovState == EPlayerMovementState::GroundPounding; }

	// 0 = expired / not dashing, 1 = just entered
	UFUNCTION(BlueprintPure) float GetDashActiveAlpha() const;

	// 0→1 flash on notable on-beat actions (for HUD)
	UFUNCTION(BlueprintPure) float GetOnBeatFlash() const;

	// Beat phase: 0 = beat just fired, 1 = next beat imminent.
	// Useful for drawing a timing bar in the HUD.
	UFUNCTION(BlueprintPure) float GetBeatPhase() const;

	// 0→1 remaining life of the jump input buffer (for HUD).
	UFUNCTION(BlueprintPure) float GetJumpBufferAlpha() const;

	// True if a pulse is buffered waiting for landing.
	UFUNCTION(BlueprintPure) bool  GetPulseBuffered() const { return bPulseBufferedForLanding; }

	// Utility: snaps BaseSec to the nearest whole-beat boundary ahead
	UFUNCTION(BlueprintPure) float GetBeatSnappedDuration(float BaseSec) const;

	// Window in ms for IsNearBeat checks (exposed so HUD can use it)
	UFUNCTION(BlueprintPure) int32 GetOnBeatWindowMs() const;

	// ── Config ────────────────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	TObjectPtr<UPcPlayerConfiguration> Config;

	// ── Delegates ─────────────────────────────────────────────────────────────
	UPROPERTY(BlueprintAssignable) FOnComboEvent   OnComboEvent;
	UPROPERTY(BlueprintAssignable) FOnSuperJumped  OnSuperJumped;
	UPROPERTY(BlueprintAssignable) FOnDoubleJumped OnDoubleJumped;
	UPROPERTY(BlueprintAssignable) FOnDashStarted  OnDashStarted;
	UPROPERTY(BlueprintAssignable) FOnDashEnded    OnDashEnded;

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void PhysWalking(float deltaTime, int32 Iterations) override;
	virtual void PhysFalling(float deltaTime, int32 Iterations) override;
	virtual void ProcessLanded(const FHitResult& Hit, float remainingTime, int32 Iterations) override;

private:
	// ── Runtime State ─────────────────────────────────────────────────────────
	EPlayerMovementState MovState = EPlayerMovementState::Grounded;

	bool  bDJAvailable    = true;
	float DJCooldownTimer = 0.f;

	// Dash: timed burst, no gauge. Timer counts down. MaxTime stored for alpha.
	float DashBoostTimer   = 0.f;
	float DashBoostMaxTime = 0.f;

	float PulseImmunityTimer = 0.f;

	bool  bJumpInputBuffered   = false;
	float JumpInputBufferTimer = 0.f;
	bool  bGPInputBuffered     = false;
	float GPInputBufferTimer   = 0.f;

	// Pulse buffering — beat fired while player was in air.
	// Fires the pulse on landing if the player touches down within the window.
	bool  bPulseBufferedForLanding = false;
	float PulseBufferTimer         = 0.f;

	float OnBeatFlashTimer    = 0.f;
	float OnBeatFlashDuration = 0.35f;

	bool  bUsingJumpCurve     = false;
	float JumpCurveTimer      = 0.f;
	float JumpCurveTotalTime  = 0.f;
	float JumpCurvePeakHeight = 0.f;

	float PreviousFrameSpeed = 0.f;

	// ── Jump Executors ────────────────────────────────────────────────────────
	void DoNormalJump();
	void DoSuperJump();
	void DoPulseJump();
	void DoDoubleJump();
	void DoFreeDoubleJump(); // on-beat gun shot: same impulse, resource untouched
	void DoGroundPound();

	// ── Dash ──────────────────────────────────────────────────────────────────
	void EnterDash();
	void ExitDash();

	// ── Arc Math ──────────────────────────────────────────────────────────────
	void ApplyArcWithAirTime(float PeakHeightCM, float AirTimeSec);
	void ExitCurveJump();

	// ── Helpers ───────────────────────────────────────────────────────────────
	bool IsNearBeat()       const;
	bool CanBufferLanding() const;
	void PushCombo(const FString& Label, FLinearColor Color);

	// Returns the air time (seconds) needed so the player lands exactly
	// AirTimeBeats beats after the nearest beat — compensates for when
	// in the beat cycle the jump was pressed.
	float ComputeSyncedAirTime(float AirTimeBeats) const;

	// ── Config Accessors ──────────────────────────────────────────────────────
	float         Cfg_BaseMaxSpeed()           const;
	float         Cfg_GroundAcceleration()     const;
	float         Cfg_GroundFriction()         const;
	float         Cfg_AirAcceleration()        const;
	float         Cfg_GravityScale()           const;
	float         Cfg_JumpPeakHeight()         const;
	float         Cfg_JumpAirTimeBeats()       const;
	float         Cfg_SuperJumpHorizBoost()    const;
	float         Cfg_DJPeakHeight()           const;
	float         Cfg_DJCooldownBeats()        const;
	float         Cfg_GPSlamSpeed()            const;
	float         Cfg_GPImmunityBeats()        const;
	float         Cfg_DashBoostSpeedMult()     const;
	float         Cfg_DashDurationBeats()      const;
	float         Cfg_DashSteerAccel()         const;
	float         Cfg_DashJumpBoost()          const;
	float         Cfg_PostDashImmunityBeats()  const;
	float         Cfg_HardSpeedCapMult()       const;
	float         Cfg_OverspeedDecay()         const;
	int32         Cfg_OnBeatWindowMs()         const;
	float         Cfg_JumpInputBuffer()        const;
	UCurveFloat*  Cfg_JumpCurve()              const;

	float ComputeCurrentMaxSpeed()    const;
	float GetCurrentBeatIntervalSec() const;
};