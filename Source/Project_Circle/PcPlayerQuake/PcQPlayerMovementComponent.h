#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PcQPlayerMovementComponent.generated.h"

UENUM(BlueprintType)
enum class EBhopState : uint8
{
	Idle		UMETA(DisplayName = "Idle"),
	Charging	UMETA(DisplayName = "Charging"),
	Active		UMETA(DisplayName = "Beat-Synced")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBhopChargeUpdated, float, ChargeAlpha);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBhopActivated);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBhopCancelled);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBhopLanded, float, HorizontalSpeed);

UCLASS(Blueprintable, BlueprintType)
class PROJECT_CIRCLE_API UPcQPlayerMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UPcQPlayerMovementComponent();

	// -------------------------------------------------------------------------
	// INPUT
	// -------------------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "Bhop")
	void OnJumpPressed();

	UFUNCTION(BlueprintCallable, Category = "Bhop")
	void OnJumpReleased();

	// -------------------------------------------------------------------------
	// MUSIC SYNC — Call these from your character's beat/BPM delegates
	// -------------------------------------------------------------------------

	/**
	 * Call this every beat (bind to PcMusicAnalysisSubsystem::OnBeatTriggered).
	 * If Active: jumps immediately when grounded, or queues a jump if mid-air.
	 */
	UFUNCTION(BlueprintCallable, Category = "Beat Sync")
	void TriggerBeatJump();

	/**
	 * Call this when BPM changes (bind to PcMusicAnalysisSubsystem::OnBPMChanged).
	 * Recalculates JumpZVelocity so airtime matches one beat interval.
	 * i.e. jump on beat N → land on beat N+1 naturally.
	 */
	UFUNCTION(BlueprintCallable, Category = "Beat Sync")
	void UpdateBPM(float NewBPM);

	// -------------------------------------------------------------------------
	// STATE QUERIES
	// -------------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "Bhop")
	EBhopState GetBhopState() const { return BhopState; }

	UFUNCTION(BlueprintPure, Category = "Bhop")
	float GetChargeAlpha() const;

	UFUNCTION(BlueprintPure, Category = "Bhop")
	float GetHorizontalSpeed() const;

	UFUNCTION(BlueprintPure, Category = "Bhop")
	bool IsInBhopChain() const;

	UFUNCTION(BlueprintPure, Category = "Beat Sync")
	float GetCurrentBPM() const { return CurrentBPM; }

	/** True if a beat-queued jump is pending (player is in air, waiting to land). */
	UFUNCTION(BlueprintPure, Category = "Beat Sync")
	bool HasQueuedJump() const { return bJumpQueuedForBeat; }

	// -------------------------------------------------------------------------
	// EVENTS
	// -------------------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "Bhop|Events")
	FOnBhopChargeUpdated OnBhopChargeUpdated;

	UPROPERTY(BlueprintAssignable, Category = "Bhop|Events")
	FOnBhopActivated OnBhopActivated;

	UPROPERTY(BlueprintAssignable, Category = "Bhop|Events")
	FOnBhopCancelled OnBhopCancelled;

	UPROPERTY(BlueprintAssignable, Category = "Bhop|Events")
	FOnBhopLanded OnBhopLanded;

	// -------------------------------------------------------------------------
	// TUNING — CHARGE
	// -------------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bhop|Charge", meta = (ClampMin = "0.1"))
	float ChargeTime = 0.6f;

	// -------------------------------------------------------------------------
	// TUNING — JUMP SYNC
	// -------------------------------------------------------------------------

	/**
	 * How long after a beat fires (in seconds) a queued jump will still trigger on landing.
	 * Prevents stale queued jumps from firing on the wrong beat.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beat Sync", meta = (ClampMin = "0.0"))
	float BeatQueueExpiry = 0.3f;

	// -------------------------------------------------------------------------
	// TUNING — GROUND
	// -------------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quake Movement|Ground", meta = (ClampMin = "1.0"))
	float GroundAccelerate = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quake Movement|Ground", meta = (ClampMin = "0.0"))
	float Friction = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quake Movement|Ground", meta = (ClampMin = "0.0"))
	float StopSpeed = 100.0f;

	// -------------------------------------------------------------------------
	// TUNING — AIR
	// -------------------------------------------------------------------------

	/**
	 * Quake sv_airaccelerate.
	 * Viscera Fest uses very high values for snappy direction changes.
	 * 150 = CPMA feel. Start here.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quake Movement|Air", meta = (ClampMin = "1.0"))
	float AirAccelerate = 150.0f;

	/**
	 * Per-frame air speed gain cap.
	 * Set to 0 to disable — gives maximum snappiness.
	 * If movement feels too twitchy, bring this back up to ~100.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quake Movement|Air", meta = (ClampMin = "0.0"))
	float AirAccelCap = 0.0f;

	// -------------------------------------------------------------------------
	// TUNING — SPEED
	// -------------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quake Movement|Speed", meta = (ClampMin = "0.0"))
	float MaxBhopSpeed = 4500.0f;

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void PhysWalking(float deltaTime, int32 Iterations) override;
	virtual void PhysFalling(float deltaTime, int32 Iterations) override;
	virtual void ProcessLanded(const FHitResult& Hit, float remainingTime, int32 Iterations) override;

private:
	// State
	EBhopState BhopState         = EBhopState::Idle;
	float      ChargeTimer       = 0.f;
	float      CurrentBPM        = 0.f;

	// Beat-queued jump: set when a beat fires while player is airborne.
	// Fires the jump the moment they touch the ground.
	bool  bJumpQueuedForBeat  = false;
	float BeatQueueTimer      = 0.f; // Counts down — if expired, queue is discarded.

	void ActivateAutoBhop();
	void CancelAutoBhop();

	/**
	 * The actual jump: resets jump count, applies vertical velocity, stays MOVE_Falling.
	 * Bypasses DoJump/CanJump because inside ProcessLanded IsMovingOnGround() is false.
	 */
	void ApplyJumpVelocity();

	void QuakeFriction(float DeltaTime);
	void QuakeAccelerateGround(const FVector& WishDir, float WishSpeed, float DeltaTime);
	void QuakeAccelerateAir(const FVector& WishDir, float WishSpeed, float DeltaTime);
	void ClampHorizontalSpeed();
};