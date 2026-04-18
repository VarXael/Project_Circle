#include "PcQPlayerMovementComponent.h"
#include "GameFramework/Character.h"

UPcQPlayerMovementComponent::UPcQPlayerMovementComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	
	// We set UNREAL'S built-in variables to 0 so it doesn't fight our custom physics
	BrakingFrictionFactor = 0.f; 
	GroundFriction = 0.f; 
	bMaintainHorizontalGroundVelocity = true; 
	AirControl = 0.f; 
	GravityScale = 1.0f; 
	MaxWalkSpeed = 900.f; 
	JumpZVelocity = 600.f;

	// FIX: Tell Unreal to stop applying default deceleration
	BrakingDecelerationWalking = 0.f;
	BrakingDecelerationFalling = 0.f;
	
	// NEW FAIL-SAFE: Disable UE5's separate braking friction
	bUseSeparateBrakingFriction = false; 
	BrakingFriction = 0.f;

	// Default Presets
	PresetSlow.BeatsPerJump = 0.5f; PresetSlow.PeakHeightCM = 200.f; 
	PresetSlow.MaxGroundSpeed = 800.f; PresetSlow.MaxAirSpeed = 800.f; PresetSlow.ChargeTime = 0.7f;

	PresetNormal.BeatsPerJump = 1.0f; PresetNormal.PeakHeightCM = 200.f; 
	PresetNormal.MaxGroundSpeed = 900.f; PresetNormal.MaxAirSpeed = 900.f; PresetNormal.ChargeTime = 0.6f;

	PresetFast.BeatsPerJump = 2.0f; PresetFast.PeakHeightCM = 175.f; 
	PresetFast.MaxGroundSpeed = 1100.f; PresetFast.MaxAirSpeed = 1100.f; PresetFast.ChargeTime = 0.5f;

	PresetVeryFast.BeatsPerJump = 4.0f; PresetVeryFast.PeakHeightCM = 150.f; 
	PresetVeryFast.MaxGroundSpeed = 1400.f; PresetVeryFast.MaxAirSpeed = 1400.f; PresetVeryFast.ChargeTime = 0.4f;
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

	if (CurrentSubdivision < 0.75f) return PresetSlow;
	if (CurrentSubdivision < 1.5f) return PresetNormal;
	if (CurrentSubdivision < 3.0f) return PresetFast;
	return PresetVeryFast;
}

FString UPcQPlayerMovementComponent::GetActivePresetName() const
{
	if (CurrentPresetOverride == EPcMovementPresetOverride::Slow) return "FORCE SLOW";
	if (CurrentPresetOverride == EPcMovementPresetOverride::Normal) return "FORCE NORMAL";
	if (CurrentPresetOverride == EPcMovementPresetOverride::Fast) return "FORCE FAST";
	if (CurrentPresetOverride == EPcMovementPresetOverride::VeryFast) return "FORCE VERY FAST";
	
	if (CurrentSubdivision < 0.75f) return "AUTO SLOW";
	if (CurrentSubdivision < 1.5f) return "AUTO NORMAL";
	if (CurrentSubdivision < 3.0f) return "AUTO FAST";
	return "AUTO VERY FAST";
}

void UPcQPlayerMovementComponent::UpdateBPM(float NewGameplayBPM, float Subdivision, EPcMovementPresetOverride PresetOverride)
{
	if (NewGameplayBPM <= 0.f) return;
	
	CurrentBPM = NewGameplayBPM;
	CurrentSubdivision = Subdivision;
	CurrentPresetOverride = PresetOverride;
	
	ApplyPreset(GetActivePreset(), 60.f / NewGameplayBPM);
}

void UPcQPlayerMovementComponent::ApplyPreset(const FPcMovementPreset& Preset, float BeatIntervalSecs)
{
	if (BeatIntervalSecs <= 0.f || Preset.BeatsPerJump <= 0.f) return;

	const float TotalJumpTime = BeatIntervalSecs * Preset.BeatsPerJump;
	const float TimeToPeak = TotalJumpTime * 0.5f;
	
	const float RequiredGravity = (2.f * Preset.PeakHeightCM) / (TimeToPeak * TimeToPeak);
	const float BaseWorldGravity = FMath::Abs(GetWorld()->GetDefaultGravityZ());
	
	GravityScale = RequiredGravity / BaseWorldGravity;
	JumpZVelocity = RequiredGravity * TimeToPeak;
	MaxWalkSpeed = Preset.MaxGroundSpeed;
}

void UPcQPlayerMovementComponent::TriggerBeatJump()
{
	if (BhopState != EBhopState::Active) return;

	UE_LOG(LogTemp, Warning, TEXT("[DEBUG-BHOP][BEAT TRIGGER] Fired! Speed right now: %f"), GetHorizontalSpeed());

	if (IsMovingOnGround()) { ApplyJumpVelocity(); OnBhopLanded.Broadcast(GetHorizontalSpeed()); }
	else { bJumpQueuedForBeat = true; BeatQueueTimer = BeatCoyoteWindow; }
}

void UPcQPlayerMovementComponent::OnJumpPressed()
{
	if (BhopState == EBhopState::Idle) { BhopState = EBhopState::Charging; ChargeTimer = 0.f; }
	else CancelAutoBhop();
}

void UPcQPlayerMovementComponent::OnJumpReleased() { if (BhopState == EBhopState::Charging) CancelAutoBhop(); }

void UPcQPlayerMovementComponent::ActivateAutoBhop() { BhopState = EBhopState::Active; ChargeTimer = 0.f; OnBhopActivated.Broadcast(); if (IsMovingOnGround()) ApplyJumpVelocity(); }
void UPcQPlayerMovementComponent::CancelAutoBhop() { BhopState = EBhopState::Idle; ChargeTimer = 0.f; bJumpQueuedForBeat = false; OnBhopCancelled.Broadcast(); }

void UPcQPlayerMovementComponent::ApplyJumpVelocity() 
{ 
	if (CharacterOwner) CharacterOwner->JumpCurrentCount = 0; 
	Velocity.Z = FMath::Max(Velocity.Z, JumpZVelocity); 
	SetMovementMode(MOVE_Falling); 
}

void UPcQPlayerMovementComponent::ProcessLanded(const FHitResult& Hit, float remainingTime, int32 Iterations)
{
	float SpeedBeforeLand = GetHorizontalSpeed();
	UE_LOG(LogTemp, Warning, TEXT("[DEBUG-BHOP][LAND] Impact Speed: %f"), SpeedBeforeLand);

	// 1. Let Unreal completely resolve the floor collision and safely place us on the ground!
	Super::ProcessLanded(Hit, remainingTime, Iterations);

	// 2. If a beat triggered while we were in the air, BOUNCE INSTANTLY on the exact same frame!
	if (BhopState == EBhopState::Active && bJumpQueuedForBeat) {
		bJumpQueuedForBeat = false; 
		ApplyJumpVelocity(); 
		OnBhopLanded.Broadcast(GetHorizontalSpeed());
	}
	
	float SpeedAfterLand = GetHorizontalSpeed();
	UE_LOG(LogTemp, Warning, TEXT("[DEBUG-BHOP] [LAND] Speed after resolution: %f"), SpeedAfterLand);
}

void UPcQPlayerMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// --- DEBUG: THE SPEED WIPE DETECTOR ---
	float CurrentSpeed = GetHorizontalSpeed();
	if (PreviousFrameSpeed > 300.f && CurrentSpeed < 50.f)
	{
		UE_LOG(LogTemp, Error, TEXT("[DEBUG-BHOP] [WIPE DETECTED] Speed dropped from %f to %f! Grounded: %d"), PreviousFrameSpeed, CurrentSpeed, IsMovingOnGround());
	}
	PreviousFrameSpeed = CurrentSpeed;

	if (CurrentBPM > 0.f)
	{
		const FPcMovementPreset& Preset = GetActivePreset();
		const float TotalJumpTime = (60.f / CurrentBPM) * Preset.BeatsPerJump;
		float ExpectedGrav = ((2.f * Preset.PeakHeightCM) / (FMath::Square(TotalJumpTime * 0.5f))) / FMath::Abs(GetWorld()->GetDefaultGravityZ());
		
		if (!FMath::IsNearlyEqual(GravityScale, ExpectedGrav, 0.01f))
		{
			ApplyPreset(Preset, 60.f / CurrentBPM);
		}
	}

	if (BhopState == EBhopState::Charging) {
		ChargeTimer += DeltaTime; OnBhopChargeUpdated.Broadcast(GetChargeAlpha());
		if (ChargeTimer >= GetActivePreset().ChargeTime) ActivateAutoBhop();
	}
	
	if (bJumpQueuedForBeat && (BeatQueueTimer -= DeltaTime) <= 0.f) bJumpQueuedForBeat = false;
}

void UPcQPlayerMovementComponent::PhysWalking(float deltaTime, int32 Iterations)
{
	if (deltaTime < MIN_TICK_TIME) return;

	const FPcMovementPreset& P = GetActivePreset();
	FVector WishDir = Acceleration.GetSafeNormal2D();
	
	FVector CurrentVel2D(Velocity.X, Velocity.Y, 0.f);
	float CurrentSpeed = CurrentVel2D.Size();
	
	FVector TargetVel2D = WishDir * P.MaxGroundSpeed;
	float InterpSpeed = WishDir.IsZero() ? CustomGroundFriction : CustomGroundAcceleration;

	// --- FIX: THE ICE SKATING LOGIC ---
	if (BhopState == EBhopState::Active)
	{
		if (!WishDir.IsZero()) {
			// Do not restrict speed if we are currently moving faster than MaxGroundSpeed
			TargetVel2D = WishDir * FMath::Max(CurrentSpeed, P.MaxGroundSpeed);
		} else {
			// If letting go of W, let them beautifully slide into the beat!
			InterpSpeed = CustomGroundFriction * 0.1f; 
		}
	}

	FVector NewVel2D = FMath::VInterpTo(CurrentVel2D, TargetVel2D, deltaTime, InterpSpeed);

	Velocity.X = NewVel2D.X;
	Velocity.Y = NewVel2D.Y;

	FVector SavedAccel = Acceleration;
	Acceleration = FVector::ZeroVector;
	
	Super::PhysWalking(deltaTime, Iterations);
	
	Acceleration = SavedAccel; 
}

void UPcQPlayerMovementComponent::PhysFalling(float deltaTime, int32 Iterations)
{
	if (deltaTime < MIN_TICK_TIME) return;

	const FPcMovementPreset& P = GetActivePreset();
	FVector WishDir = Acceleration.GetSafeNormal2D();

	FVector CurrentVel2D(Velocity.X, Velocity.Y, 0.f);
	FVector TargetVel2D = WishDir * P.MaxAirSpeed;

	float InterpSpeed = WishDir.IsZero() ? CustomAirFriction : CustomAirAcceleration;
	FVector NewVel2D = FMath::VInterpTo(CurrentVel2D, TargetVel2D, deltaTime, InterpSpeed);

	Velocity.X = NewVel2D.X;
	Velocity.Y = NewVel2D.Y;

	FVector SavedAccel = Acceleration;
	Acceleration = FVector::ZeroVector;

	Super::PhysFalling(deltaTime, Iterations);

	Acceleration = SavedAccel;
}