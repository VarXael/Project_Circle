#include "PcQPlayerMovementComponent.h"
#include "GameFramework/Character.h"

UPcQPlayerMovementComponent::UPcQPlayerMovementComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	BrakingFrictionFactor             = 0.f;
	BrakingDecelerationWalking        = 0.f;
	GroundFriction                    = 0.f;
	AirControl                        = 1.f;
	AirControlBoostMultiplier         = 0.f;
	AirControlBoostVelocityThreshold  = 0.f;
	bMaintainHorizontalGroundVelocity = true;

	GravityScale  = 2.0f;
	MaxWalkSpeed  = 900.f;
	JumpZVelocity = 600.f; // Will be overwritten by UpdateBPM once music starts.
}

// =============================================================================
// PUBLIC API
// =============================================================================

float UPcQPlayerMovementComponent::GetChargeAlpha() const
{
	if (ChargeTime <= 0.f) return 1.f;
	return FMath::Clamp(ChargeTimer / ChargeTime, 0.f, 1.f);
}

float UPcQPlayerMovementComponent::GetHorizontalSpeed() const
{
	return FVector(Velocity.X, Velocity.Y, 0.f).Size();
}

bool UPcQPlayerMovementComponent::IsInBhopChain() const
{
	return GetHorizontalSpeed() > MaxWalkSpeed * 1.05f;
}

// =============================================================================
// MUSIC SYNC
// =============================================================================

void UPcQPlayerMovementComponent::UpdateBPM(float NewBPM)
{
	if (NewBPM <= 0.f) return;
	CurrentBPM = NewBPM;
	// Jump height is fixed — BPM only tracked for debug display.
	// The beat triggers the jump, but height is always JumpZVelocity.
}

void UPcQPlayerMovementComponent::TriggerBeatJump()
{
	if (BhopState != EBhopState::Active) return;

	if (IsMovingOnGround())
	{
		// Perfect — grounded on a beat. Jump immediately.
		ApplyJumpVelocity();
		OnBhopLanded.Broadcast(GetHorizontalSpeed());
	}
	else
	{
		// Player is still in the air from the previous jump.
		// Queue the jump — it fires the instant they touch the ground.
		bJumpQueuedForBeat = true;
		BeatQueueTimer     = BeatQueueExpiry;
	}
}

// =============================================================================
// INPUT
// =============================================================================

void UPcQPlayerMovementComponent::OnJumpPressed()
{
	switch (BhopState)
	{
	case EBhopState::Idle:
		BhopState   = EBhopState::Charging;
		ChargeTimer = 0.f;
		break;

	case EBhopState::Charging:
		CancelAutoBhop();
		break;

	case EBhopState::Active:
		// Cancel beat-sync mode. Player is back in manual control.
		CancelAutoBhop();
		break;
	}
}

void UPcQPlayerMovementComponent::OnJumpReleased()
{
	if (BhopState == EBhopState::Charging)
	{
		BhopState   = EBhopState::Idle;
		ChargeTimer = 0.f;
	}
}

// =============================================================================
// TICK
// =============================================================================

void UPcQPlayerMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                                FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Charge advancement
	if (BhopState == EBhopState::Charging)
	{
		ChargeTimer += DeltaTime;
		OnBhopChargeUpdated.Broadcast(GetChargeAlpha());

		if (ChargeTimer >= ChargeTime)
		{
			ActivateAutoBhop();
		}
	}

	// Tick down the beat queue expiry
	if (bJumpQueuedForBeat)
	{
		BeatQueueTimer -= DeltaTime;
		if (BeatQueueTimer <= 0.f)
		{
			// Beat queue expired — player took too long to land. Discard.
			bJumpQueuedForBeat = false;
		}
	}
}

void UPcQPlayerMovementComponent::ActivateAutoBhop()
{
	BhopState   = EBhopState::Active;
	ChargeTimer = 0.f;
	OnBhopActivated.Broadcast();
	// No immediate jump here — wait for the next beat to fire TriggerBeatJump().
}

void UPcQPlayerMovementComponent::CancelAutoBhop()
{
	BhopState          = EBhopState::Idle;
	ChargeTimer        = 0.f;
	bJumpQueuedForBeat = false;
	BeatQueueTimer     = 0.f;
	OnBhopCancelled.Broadcast();
}

// =============================================================================
// LANDING
// =============================================================================

void UPcQPlayerMovementComponent::ProcessLanded(const FHitResult& Hit, float remainingTime, int32 Iterations)
{
	// If a beat-queued jump is pending and hasn't expired, fire it immediately.
	// This handles the case where the beat fired while the player was airborne —
	// they land very close to the beat, and we fire the jump to keep them synced.
	if (BhopState == EBhopState::Active && bJumpQueuedForBeat)
	{
		bJumpQueuedForBeat = false;
		BeatQueueTimer     = 0.f;

		ApplyJumpVelocity();
		OnBhopLanded.Broadcast(GetHorizontalSpeed());

		// Stay airborne — skip Super so ground friction never runs.
		SetMovementMode(MOVE_Falling);
		StartNewPhysics(remainingTime, Iterations);
		return;
	}

	Super::ProcessLanded(Hit, remainingTime, Iterations);
}

void UPcQPlayerMovementComponent::ApplyJumpVelocity()
{
	if (CharacterOwner)
	{
		CharacterOwner->JumpCurrentCount = 0;
	}
	Velocity.Z = FMath::Max(Velocity.Z, JumpZVelocity);
	SetMovementMode(MOVE_Falling);
}

// =============================================================================
// GROUND PHYSICS
// =============================================================================

void UPcQPlayerMovementComponent::PhysWalking(float deltaTime, int32 Iterations)
{
	if (deltaTime < MIN_TICK_TIME) return;

	QuakeFriction(deltaTime);

	FVector WishDir = Acceleration.GetSafeNormal();
	WishDir.Z = 0.f;

	if (!WishDir.IsZero())
	{
		WishDir = WishDir.GetSafeNormal();
		QuakeAccelerateGround(WishDir, FMath::Min(Acceleration.Size(), MaxWalkSpeed), deltaTime);
	}

	Super::PhysWalking(deltaTime, Iterations);
}

void UPcQPlayerMovementComponent::QuakeFriction(float DeltaTime)
{
	float Speed = FVector(Velocity.X, Velocity.Y, 0.f).Size();
	if (Speed < KINDA_SMALL_NUMBER) return;

	float Control  = FMath::Max(Speed, StopSpeed);
	float NewSpeed = FMath::Max(0.f, Speed - Control * Friction * DeltaTime);

	float Scale = NewSpeed / Speed;
	Velocity.X *= Scale;
	Velocity.Y *= Scale;
}

void UPcQPlayerMovementComponent::QuakeAccelerateGround(const FVector& WishDir, float WishSpeed, float DeltaTime)
{
	float CurrentSpeed = FVector::DotProduct(Velocity, WishDir);
	float AddSpeed     = WishSpeed - CurrentSpeed;
	if (AddSpeed <= 0.f) return;

	Velocity += FMath::Min(AddSpeed, GroundAccelerate * WishSpeed * DeltaTime) * WishDir;
}

// =============================================================================
// AIR PHYSICS
// =============================================================================

void UPcQPlayerMovementComponent::PhysFalling(float deltaTime, int32 Iterations)
{
	FVector WishDir = Acceleration.GetSafeNormal();
	WishDir.Z = 0.f;

	if (!WishDir.IsZero())
	{
		WishDir = WishDir.GetSafeNormal();
		QuakeAccelerateAir(WishDir, FMath::Min(Acceleration.Size(), MaxWalkSpeed), deltaTime);
	}

	if (MaxBhopSpeed > 0.f)
	{
		ClampHorizontalSpeed();
	}

	Super::PhysFalling(deltaTime, Iterations);
}

void UPcQPlayerMovementComponent::QuakeAccelerateAir(const FVector& WishDir, float WishSpeed, float DeltaTime)
{
	// Quake dot-product gate: only accelerate when strafing perpendicular to velocity.
	// High AirAccelerate (150+) with no AirAccelCap = instant snappy direction changes.
	float CurrentSpeed = FVector::DotProduct(Velocity, WishDir);
	float AddSpeed     = WishSpeed - CurrentSpeed;
	if (AddSpeed <= 0.f) return;

	float AccelSpeed = AirAccelerate * WishSpeed * DeltaTime;
	if (AirAccelCap > 0.f)
	{
		AccelSpeed = FMath::Min(AccelSpeed, AirAccelCap);
	}
	Velocity += FMath::Min(AccelSpeed, AddSpeed) * WishDir;
}

void UPcQPlayerMovementComponent::ClampHorizontalSpeed()
{
	FVector H(Velocity.X, Velocity.Y, 0.f);
	if (H.SizeSquared() > MaxBhopSpeed * MaxBhopSpeed)
	{
		H = H.GetSafeNormal() * MaxBhopSpeed;
		Velocity.X = H.X;
		Velocity.Y = H.Y;
	}
}