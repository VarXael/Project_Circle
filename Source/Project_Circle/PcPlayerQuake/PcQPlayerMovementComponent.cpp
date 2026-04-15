#include "PcQPlayerMovementComponent.h"
#include "GameFramework/Character.h"

// Default preset values — tuned as a starting point.
// Slow: big visible hops, moderate speed.
// Normal: medium hops, comfortable speed.
// Fast: snappy hops, higher speed.
// Very Fast: tight hops, high speed.

static FPcMovementPreset MakePresetSlow()
{
	FPcMovementPreset P;
	P.PeakHeightCM          = 250.f;
	P.MinFallGravityMultiplier = 1.8f;
	P.MaxGroundSpeed        = 800.f;
	P.AirControlFraction    = 0.08f;
	P.AirDragFraction       = 0.03f;
	P.ChargeTime            = 0.7f;
	return P;
}

static FPcMovementPreset MakePresetNormal()
{
	FPcMovementPreset P;
	P.PeakHeightCM          = 200.f;
	P.MinFallGravityMultiplier = 2.2f;
	P.MaxGroundSpeed        = 900.f;
	P.AirControlFraction    = 0.10f;
	P.AirDragFraction       = 0.04f;
	P.ChargeTime            = 0.6f;
	return P;
}

static FPcMovementPreset MakePresetFast()
{
	FPcMovementPreset P;
	P.PeakHeightCM          = 160.f;
	P.MinFallGravityMultiplier = 2.8f;
	P.MaxGroundSpeed        = 1100.f;
	P.AirControlFraction    = 0.12f;
	P.AirDragFraction       = 0.05f;
	P.ChargeTime            = 0.5f;
	return P;
}

static FPcMovementPreset MakePresetVeryFast()
{
	FPcMovementPreset P;
	P.PeakHeightCM          = 120.f;
	P.MinFallGravityMultiplier = 3.5f;
	P.MaxGroundSpeed        = 1400.f;
	P.AirControlFraction    = 0.14f;
	P.AirDragFraction       = 0.06f;
	P.ChargeTime            = 0.4f;
	return P;
}

UPcQPlayerMovementComponent::UPcQPlayerMovementComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	BrakingFrictionFactor             = 0.f;
	BrakingDecelerationWalking        = 0.f;
	GroundFriction                    = 0.f;
	bMaintainHorizontalGroundVelocity = true;

	AirControl                        = 1.f;
	AirControlBoostMultiplier         = 0.f;
	AirControlBoostVelocityThreshold  = 0.f;

	GravityScale  = 1.5f;
	MaxWalkSpeed  = 900.f;
	JumpZVelocity = 600.f;

	// Set default preset values
	PresetSlow     = MakePresetSlow();
	PresetNormal   = MakePresetNormal();
	PresetFast     = MakePresetFast();
	PresetVeryFast = MakePresetVeryFast();
}

// =============================================================================
// PUBLIC API
// =============================================================================

float UPcQPlayerMovementComponent::GetChargeAlpha() const
{
	if (ChargeTimer <= 0.f) return 0.f;
	const FPcMovementPreset& P = GetPresetForSubdivision(CurrentSubdivision);
	return FMath::Clamp(ChargeTimer / P.ChargeTime, 0.f, 1.f);
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
// PRESET SELECTION
// =============================================================================

const FPcMovementPreset& UPcQPlayerMovementComponent::GetPresetForSubdivision(int32 Subdivision) const
{
	// Note: 'Subdivision' is now being passed as the Speed Multiplier (1x, 2x, 4x) from the subsystem!
	
	// Multiplier 1 means it matches the song's baseline speed.
	if (Subdivision <= 1) return PresetSlow;
	
	// Multiplier 2 means double-time.
	if (Subdivision <= 2) return PresetNormal;
	
	// Multiplier 4 or higher means quadruple-time (drops/intense sections).
	return PresetFast;
}

void UPcQPlayerMovementComponent::ApplyPreset(const FPcMovementPreset& Preset, float BeatIntervalSeconds)
{
	const float BaseGravity = FMath::Abs(GetGravityZ());
	if (BaseGravity <= 0.f || BeatIntervalSeconds <= 0.f) return;

	const float H  = Preset.PeakHeightCM;
	const float V0 = FMath::Sqrt(2.f * BaseGravity * H);

	JumpZVelocity = V0;

	const float RiseTime = V0 / BaseGravity;
	const float FallTime = BeatIntervalSeconds - RiseTime;

	if (FallTime > 0.01f)
	{
		const float RequiredFallGravity = (2.f * H) / (FallTime * FallTime);
		const float RequiredMultiplier  = RequiredFallGravity / BaseGravity;

		ActiveFallGravityMultiplier = FMath::Max(Preset.MinFallGravityMultiplier, RequiredMultiplier);
	}
	else
	{
		ActiveFallGravityMultiplier = Preset.MinFallGravityMultiplier;
		UE_LOG(LogTemp, Warning,
			TEXT("PcQMovement: Beat interval (%.3fs) too short for PeakHeight %.0fcm at this BPM. Using MinFallGravity."),
			BeatIntervalSeconds, H);
	}

	MaxWalkSpeed = Preset.MaxGroundSpeed;

	UE_LOG(LogTemp, Log,
		TEXT("PcQMovement: Preset applied — JumpZVel=%.1f  FallGravMult=%.2f  Speed=%.0f  BeatInterval=%.3fs"),
		JumpZVelocity, ActiveFallGravityMultiplier, MaxWalkSpeed, BeatIntervalSeconds);
}

// =============================================================================
// MUSIC SYNC
// =============================================================================

void UPcQPlayerMovementComponent::UpdateBPM(float NewGameplayBPM, int32 Subdivision)
{
	if (NewGameplayBPM <= 0.f) return;

	CurrentBPM         = NewGameplayBPM;
	CurrentSubdivision = Subdivision; // This now holds our Multiplier!

	const float BeatIntervalSeconds = 60.f / NewGameplayBPM;
	const FPcMovementPreset& Preset = GetPresetForSubdivision(Subdivision);

	ApplyPreset(Preset, BeatIntervalSeconds);

	OnPresetChanged.Broadcast(Subdivision);
}

void UPcQPlayerMovementComponent::TriggerBeatJump()
{
	if (BhopState != EBhopState::Active) return;

	if (IsMovingOnGround())
	{
		ApplyJumpVelocity();
		OnBhopLanded.Broadcast(GetHorizontalSpeed());
	}
	else
	{
		bJumpQueuedForBeat = true;
		BeatQueueTimer     = BeatCoyoteWindow;
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

	if (BhopState == EBhopState::Charging)
	{
		ChargeTimer += DeltaTime;
		OnBhopChargeUpdated.Broadcast(GetChargeAlpha());

		const FPcMovementPreset& P = GetPresetForSubdivision(CurrentSubdivision);
		if (ChargeTimer >= P.ChargeTime)
			ActivateAutoBhop();
	}

	if (bJumpQueuedForBeat)
	{
		BeatQueueTimer -= DeltaTime;
		if (BeatQueueTimer <= 0.f)
			bJumpQueuedForBeat = false;
	}
}

void UPcQPlayerMovementComponent::ActivateAutoBhop()
{
	BhopState   = EBhopState::Active;
	ChargeTimer = 0.f;
	OnBhopActivated.Broadcast();

	if (IsMovingOnGround())
		ApplyJumpVelocity();
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
	if (BhopState == EBhopState::Active && bJumpQueuedForBeat)
	{
		bJumpQueuedForBeat = false;
		BeatQueueTimer     = 0.f;

		ApplyJumpVelocity();
		OnBhopLanded.Broadcast(GetHorizontalSpeed());

		SetMovementMode(MOVE_Falling);
		StartNewPhysics(remainingTime, Iterations);
		return;
	}

	Super::ProcessLanded(Hit, remainingTime, Iterations);
}

void UPcQPlayerMovementComponent::ApplyJumpVelocity()
{
	if (CharacterOwner)
		CharacterOwner->JumpCurrentCount = 0;

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
	float Scale    = NewSpeed / Speed;
	Velocity.X    *= Scale;
	Velocity.Y    *= Scale;
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
	const FPcMovementPreset& Preset = GetPresetForSubdivision(CurrentSubdivision);

	FVector WishDir = Acceleration.GetSafeNormal();
	WishDir.Z = 0.f;

	FVector Horizontal(Velocity.X, Velocity.Y, 0.f);

	if (!WishDir.IsZero() && Preset.AirControlFraction > 0.f)
	{
		WishDir    = WishDir.GetSafeNormal();
		FVector Target = WishDir * Horizontal.Size();
		Horizontal = FMath::Lerp(Horizontal, Target, FMath::Clamp(Preset.AirControlFraction, 0.f, 1.f));
	}
	else if (Preset.AirDragFraction > 0.f)
	{
		Horizontal = FMath::Lerp(Horizontal, FVector::ZeroVector, FMath::Clamp(Preset.AirDragFraction, 0.f, 1.f));
	}

	Velocity.X = Horizontal.X;
	Velocity.Y = Horizontal.Y;

	if (Velocity.Z < 0.f)
	{
		const float ExtraGravity = FMath::Abs(GetGravityZ()) * (ActiveFallGravityMultiplier - 1.f);
		Velocity.Z -= ExtraGravity * deltaTime;
	}

	if (MaxWalkSpeed > 0.f)
		ClampHorizontalSpeed();

	Super::PhysFalling(deltaTime, Iterations);
}

void UPcQPlayerMovementComponent::ClampHorizontalSpeed()
{
	const float Cap = IsInBhopChain()
		? GetPresetForSubdivision(CurrentSubdivision).MaxGroundSpeed * 2.f
		: GetPresetForSubdivision(CurrentSubdivision).MaxGroundSpeed;

	FVector H(Velocity.X, Velocity.Y, 0.f);
	if (H.SizeSquared() > Cap * Cap)
	{
		H = H.GetSafeNormal() * Cap;
		Velocity.X = H.X;
		Velocity.Y = H.Y;
	}
}