#include "PcQPlayerMovementComponent.h"
#include "GameFramework/Character.h"

UPcQPlayerMovementComponent::UPcQPlayerMovementComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	BrakingFrictionFactor = 0.f; GroundFriction = 0.f;
	bMaintainHorizontalGroundVelocity = true; AirControl = 1.f;
	GravityScale = 1.5f; MaxWalkSpeed = 900.f; JumpZVelocity = 600.f;

	PresetSlow.PeakHeightCM = 250.f; PresetSlow.MaxGroundSpeed = 800.f; PresetSlow.ChargeTime = 0.7f;
	PresetNormal.PeakHeightCM = 200.f; PresetNormal.MaxGroundSpeed = 900.f; PresetNormal.ChargeTime = 0.6f;
	PresetFast.PeakHeightCM = 160.f; PresetFast.MaxGroundSpeed = 1100.f; PresetFast.ChargeTime = 0.5f;
	PresetVeryFast.PeakHeightCM = 120.f; PresetVeryFast.MaxGroundSpeed = 1400.f; PresetVeryFast.ChargeTime = 0.4f;
}

float UPcQPlayerMovementComponent::GetChargeAlpha() const { return ChargeTimer > 0.f ? FMath::Clamp(ChargeTimer / GetActivePreset().ChargeTime, 0.f, 1.f) : 0.f; }
float UPcQPlayerMovementComponent::GetHorizontalSpeed() const { return FVector(Velocity.X, Velocity.Y, 0.f).Size(); }
bool UPcQPlayerMovementComponent::IsInBhopChain() const { return GetHorizontalSpeed() > MaxWalkSpeed * 1.05f; }

const FPcMovementPreset& UPcQPlayerMovementComponent::GetActivePreset() const
{
	if (CurrentPresetOverride == EPcMovementPresetOverride::Slow) return PresetSlow;
	if (CurrentPresetOverride == EPcMovementPresetOverride::Normal) return PresetNormal;
	if (CurrentPresetOverride == EPcMovementPresetOverride::Fast) return PresetFast;
	if (CurrentPresetOverride == EPcMovementPresetOverride::VeryFast) return PresetVeryFast;

	if (CurrentSubdivision <= 1) return PresetSlow;
	if (CurrentSubdivision <= 2) return PresetNormal;
	if (CurrentSubdivision <= 4) return PresetFast;
	return PresetVeryFast;
}

FString UPcQPlayerMovementComponent::GetActivePresetName() const
{
	if (CurrentPresetOverride == EPcMovementPresetOverride::Slow) return "FORCE SLOW";
	if (CurrentPresetOverride == EPcMovementPresetOverride::Normal) return "FORCE NORMAL";
	if (CurrentPresetOverride == EPcMovementPresetOverride::Fast) return "FORCE FAST";
	if (CurrentPresetOverride == EPcMovementPresetOverride::VeryFast) return "FORCE VERY FAST";
	
	if (CurrentSubdivision <= 1) return "AUTO SLOW";
	if (CurrentSubdivision <= 2) return "AUTO NORMAL";
	if (CurrentSubdivision <= 4) return "AUTO FAST";
	return "AUTO VERY FAST";
}

void UPcQPlayerMovementComponent::UpdateBPM(float NewGameplayBPM, int32 Subdivision, EPcMovementPresetOverride PresetOverride)
{
	if (NewGameplayBPM <= 0.f) return;
	CurrentBPM = NewGameplayBPM;
	CurrentSubdivision = Subdivision;
	CurrentPresetOverride = PresetOverride; // BUG FIXED: Explicitly saves override
	ApplyPreset(GetActivePreset(), 60.f / NewGameplayBPM);
}

void UPcQPlayerMovementComponent::ApplyPreset(const FPcMovementPreset& Preset, float BeatIntervalSecs)
{
	const float BaseGrav = FMath::Abs(GetGravityZ());
	if (BaseGrav <= 0.f || BeatIntervalSecs <= 0.f) return;

	JumpZVelocity = FMath::Sqrt(2.f * BaseGrav * Preset.PeakHeightCM);
	float FallTime = BeatIntervalSecs - (JumpZVelocity / BaseGrav);
	
	ActiveFallGravityMultiplier = FallTime > 0.01f ? FMath::Max(Preset.MinFallGravityMultiplier, (2.f * Preset.PeakHeightCM) / (FallTime * FallTime) / BaseGrav) : Preset.MinFallGravityMultiplier;
	MaxWalkSpeed = Preset.MaxGroundSpeed;
}

void UPcQPlayerMovementComponent::TriggerBeatJump()
{
	if (BhopState != EBhopState::Active) return;
	if (IsMovingOnGround()) { ApplyJumpVelocity(); OnBhopLanded.Broadcast(GetHorizontalSpeed()); }
	else { bJumpQueuedForBeat = true; BeatQueueTimer = BeatCoyoteWindow; }
}

void UPcQPlayerMovementComponent::OnJumpPressed()
{
	if (BhopState == EBhopState::Idle) { BhopState = EBhopState::Charging; ChargeTimer = 0.f; }
	else CancelAutoBhop();
}

void UPcQPlayerMovementComponent::OnJumpReleased() { if (BhopState == EBhopState::Charging) CancelAutoBhop(); }

void UPcQPlayerMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (BhopState == EBhopState::Charging) {
		ChargeTimer += DeltaTime; OnBhopChargeUpdated.Broadcast(GetChargeAlpha());
		if (ChargeTimer >= GetActivePreset().ChargeTime) ActivateAutoBhop();
	}
	if (bJumpQueuedForBeat && (BeatQueueTimer -= DeltaTime) <= 0.f) bJumpQueuedForBeat = false;
}

void UPcQPlayerMovementComponent::ActivateAutoBhop() { BhopState = EBhopState::Active; ChargeTimer = 0.f; OnBhopActivated.Broadcast(); if (IsMovingOnGround()) ApplyJumpVelocity(); }
void UPcQPlayerMovementComponent::CancelAutoBhop() { BhopState = EBhopState::Idle; ChargeTimer = 0.f; bJumpQueuedForBeat = false; OnBhopCancelled.Broadcast(); }

void UPcQPlayerMovementComponent::ApplyJumpVelocity() { if (CharacterOwner) CharacterOwner->JumpCurrentCount = 0; Velocity.Z = FMath::Max(Velocity.Z, JumpZVelocity); SetMovementMode(MOVE_Falling); }

void UPcQPlayerMovementComponent::ProcessLanded(const FHitResult& Hit, float remainingTime, int32 Iterations)
{
	if (BhopState == EBhopState::Active && bJumpQueuedForBeat) {
		bJumpQueuedForBeat = false; ApplyJumpVelocity(); OnBhopLanded.Broadcast(GetHorizontalSpeed());
		SetMovementMode(MOVE_Falling); StartNewPhysics(remainingTime, Iterations); return;
	}
	Super::ProcessLanded(Hit, remainingTime, Iterations);
}

void UPcQPlayerMovementComponent::PhysWalking(float deltaTime, int32 Iterations)
{
	if (deltaTime < MIN_TICK_TIME) return;
	QuakeFriction(deltaTime);
	FVector WishDir = Acceleration.GetSafeNormal(); WishDir.Z = 0.f;
	if (!WishDir.IsZero()) QuakeAccelerateGround(WishDir.GetSafeNormal(), FMath::Min(Acceleration.Size(), MaxWalkSpeed), deltaTime);
	Super::PhysWalking(deltaTime, Iterations);
}

void UPcQPlayerMovementComponent::QuakeFriction(float DeltaTime)
{
	float Speed = FVector(Velocity.X, Velocity.Y, 0.f).Size();
	if (Speed < KINDA_SMALL_NUMBER) return;
	float Drop = FMath::Max(Speed, StopSpeed) * Friction * DeltaTime;
	float Scale = FMath::Max(0.f, Speed - Drop) / Speed;
	Velocity.X *= Scale; Velocity.Y *= Scale;
}

void UPcQPlayerMovementComponent::QuakeAccelerateGround(const FVector& WishDir, float WishSpeed, float DeltaTime)
{
	float AddSpeed = WishSpeed - FVector::DotProduct(Velocity, WishDir);
	if (AddSpeed > 0.f) Velocity += FMath::Min(AddSpeed, GroundAccelerate * WishSpeed * DeltaTime) * WishDir;
}

void UPcQPlayerMovementComponent::PhysFalling(float deltaTime, int32 Iterations)
{
	const FPcMovementPreset& P = GetActivePreset();
	FVector WishDir = Acceleration.GetSafeNormal(); WishDir.Z = 0.f;
	FVector H(Velocity.X, Velocity.Y, 0.f);

	if (!WishDir.IsZero() && P.AirControlFraction > 0.f) H = FMath::Lerp(H, WishDir.GetSafeNormal() * H.Size(), P.AirControlFraction);
	else if (P.AirDragFraction > 0.f) H = FMath::Lerp(H, FVector::ZeroVector, P.AirDragFraction);

	Velocity.X = H.X; Velocity.Y = H.Y;
	if (Velocity.Z < 0.f) Velocity.Z -= FMath::Abs(GetGravityZ()) * (ActiveFallGravityMultiplier - 1.f) * deltaTime;

	ClampHorizontalSpeed();
	Super::PhysFalling(deltaTime, Iterations);
}

void UPcQPlayerMovementComponent::ClampHorizontalSpeed()
{
	const float Cap = IsInBhopChain() ? GetActivePreset().MaxGroundSpeed * 2.f : GetActivePreset().MaxGroundSpeed;
	FVector H(Velocity.X, Velocity.Y, 0.f);
	if (H.SizeSquared() > Cap * Cap) { H = H.GetSafeNormal() * Cap; Velocity.X = H.X; Velocity.Y = H.Y; }
}