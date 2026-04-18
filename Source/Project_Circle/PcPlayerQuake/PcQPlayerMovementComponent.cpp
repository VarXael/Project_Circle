#include "PcQPlayerMovementComponent.h"
#include "GameFramework/Character.h"
#include "Project_Circle/MusicSystem/MusicGameplaySystem/PcMusicAnalysisSubsystem.h"

UPcQPlayerMovementComponent::UPcQPlayerMovementComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	
	BrakingFrictionFactor = 0.f; 
	GroundFriction = 0.f; 
	bMaintainHorizontalGroundVelocity = true; 
	AirControl = 0.f; 
	GravityScale = 1.0f; 
	MaxWalkSpeed = 900.f; 
	JumpZVelocity = 600.f;

	BrakingDecelerationWalking = 0.f;
	BrakingDecelerationFalling = 0.f;
	bUseSeparateBrakingFriction = false; 
	BrakingFriction = 0.f;
}

float UPcQPlayerMovementComponent::GetChargeAlpha() const { 
	if (UPcMusicAnalysisSubsystem* Sub = GetWorld()->GetSubsystem<UPcMusicAnalysisSubsystem>()) {
		if (Sub->IsReadyForPlayback()) return ChargeTimer > 0.f ? FMath::Clamp(ChargeTimer / Sub->GetCurrentPulsePreset().ChargeTime, 0.f, 1.f) : 0.f;
	}
	return 0.f; 
}

float UPcQPlayerMovementComponent::GetHorizontalSpeed() const { return FVector(Velocity.X, Velocity.Y, 0.f).Size(); }
bool UPcQPlayerMovementComponent::IsInBhopChain() const { return GetHorizontalSpeed() > MaxWalkSpeed * 1.05f; }

void UPcQPlayerMovementComponent::TriggerBeatJump()
{
	if (BhopState != EBhopState::Active) return;

	if (IsFalling() && Velocity.Z > 0.f) return;

	if (IsMovingOnGround()) { ApplyJumpVelocity(); OnBhopLanded.Broadcast(GetHorizontalSpeed()); }
	else { bJumpQueuedForBeat = true; BeatQueueTimer = BeatCoyoteWindow; }
}

void UPcQPlayerMovementComponent::OnJumpPressed() {
	if (BhopState == EBhopState::Idle) { BhopState = EBhopState::Charging; ChargeTimer = 0.f; }
	else CancelAutoBhop();
}

void UPcQPlayerMovementComponent::OnJumpReleased() { if (BhopState == EBhopState::Charging) CancelAutoBhop(); }

void UPcQPlayerMovementComponent::OnGroundPoundPressed()
{
	if (IsFalling() && BhopState != EBhopState::GroundPounding)
	{
		BhopState = EBhopState::GroundPounding;
		Velocity.X = 0.f; Velocity.Y = 0.f; Velocity.Z = GroundPoundSlamSpeed; 
	}
}

void UPcQPlayerMovementComponent::ActivateAutoBhop() { BhopState = EBhopState::Active; ChargeTimer = 0.f; OnBhopActivated.Broadcast(); if (IsMovingOnGround()) ApplyJumpVelocity(); }
void UPcQPlayerMovementComponent::CancelAutoBhop() { BhopState = EBhopState::Idle; ChargeTimer = 0.f; bJumpQueuedForBeat = false; OnBhopCancelled.Broadcast(); }

void UPcQPlayerMovementComponent::ApplyJumpVelocity() { 
	if (CharacterOwner) CharacterOwner->JumpCurrentCount = 0; 

	// --- SIMPLIFIED JUMP QUANTIZATION (Using the True Gameplay Beat) ---
	if (UPcMusicAnalysisSubsystem* Sub = GetWorld()->GetSubsystem<UPcMusicAnalysisSubsystem>()) 
	{
		if (Sub->IsReadyForPlayback()) 
		{
			const FPcMovementPreset& Preset = Sub->GetCurrentPulsePreset();
			float GameplayInterval = Sub->GetGameplayBeatIntervalMS() / 1000.f; 
			
			// Exactly how much time remains until the next true gameplay pulse?
			float TargetAirTime = Sub->GetTimeUntilNextGameplayBeat();
			
			// If we landed incredibly close to the beat (e.g. 0.05 seconds early), jumping for 0.05s is a glitchy micro-hop.
			// Instead, we absorb that small error and stretch the jump out to target the beat AFTER it.
			if (TargetAirTime < GameplayInterval * 0.25f) TargetAirTime += GameplayInterval;
			
			const float PeakHeight = Preset.PeakHeightCM;
			const float TimeToPeak = TargetAirTime * 0.5f;
			
			const float RequiredGravity = (2.f * PeakHeight) / (TimeToPeak * TimeToPeak);
			const float BaseWorldGravity = FMath::Abs(GetWorld()->GetDefaultGravityZ());
			
			GravityScale = RequiredGravity / BaseWorldGravity;
			Velocity.Z = RequiredGravity * TimeToPeak;
			
			SetMovementMode(MOVE_Falling);
			return;
		}
	}

	Velocity.Z = FMath::Max(Velocity.Z, JumpZVelocity); 
	SetMovementMode(MOVE_Falling); 
}

void UPcQPlayerMovementComponent::ProcessLanded(const FHitResult& Hit, float remainingTime, int32 Iterations)
{
	if (BhopState == EBhopState::GroundPounding)
	{
		BhopState = EBhopState::Active;
		FVector WishDir = Acceleration.GetSafeNormal2D();
		if (WishDir.IsZero() && CharacterOwner) WishDir = CharacterOwner->GetActorForwardVector().GetSafeNormal2D();
		
		Velocity.X = WishDir.X * GroundPoundDashSpeed; Velocity.Y = WishDir.Y * GroundPoundDashSpeed;
		ApplyJumpVelocity(); 
		return;
	}

	Super::ProcessLanded(Hit, remainingTime, Iterations);

	if (BhopState == EBhopState::Active && bJumpQueuedForBeat) {
		bJumpQueuedForBeat = false; ApplyJumpVelocity(); OnBhopLanded.Broadcast(GetHorizontalSpeed());
	}
}

void UPcQPlayerMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	float CurrentSpeed = GetHorizontalSpeed();
	if (PreviousFrameSpeed > 300.f && CurrentSpeed < 50.f) UE_LOG(LogTemp, Error, TEXT("[DEBUG-BHOP] WIPE DETECTED!"));
	PreviousFrameSpeed = CurrentSpeed;

	if (UPcMusicAnalysisSubsystem* Sub = GetWorld()->GetSubsystem<UPcMusicAnalysisSubsystem>()) {
		if (Sub->IsReadyForPlayback()) {
			MaxWalkSpeed = Sub->GetCurrentPulsePreset().MaxGroundSpeed;
		}
	}

	if (BhopState == EBhopState::Charging) {
		ChargeTimer += DeltaTime; OnBhopChargeUpdated.Broadcast(GetChargeAlpha());
		if (UPcMusicAnalysisSubsystem* Sub = GetWorld()->GetSubsystem<UPcMusicAnalysisSubsystem>()) {
			if (Sub->IsReadyForPlayback() && ChargeTimer >= Sub->GetCurrentPulsePreset().ChargeTime) ActivateAutoBhop();
		}
	}
	
	if (bJumpQueuedForBeat && (BeatQueueTimer -= DeltaTime) <= 0.f) bJumpQueuedForBeat = false;
}

void UPcQPlayerMovementComponent::PhysWalking(float deltaTime, int32 Iterations)
{
	if (deltaTime < MIN_TICK_TIME) return;

	float TargetSpeed = MaxWalkSpeed;
	if (UPcMusicAnalysisSubsystem* Sub = GetWorld()->GetSubsystem<UPcMusicAnalysisSubsystem>()) {
		if (Sub->IsReadyForPlayback()) TargetSpeed = Sub->GetCurrentPulsePreset().MaxGroundSpeed;
	}

	FVector WishDir = Acceleration.GetSafeNormal2D();
	FVector CurrentVel2D(Velocity.X, Velocity.Y, 0.f);
	float CurrentSpeed = CurrentVel2D.Size();
	
	float EffectiveTargetSpeed = (CurrentSpeed > TargetSpeed && !WishDir.IsZero()) ? CurrentSpeed : TargetSpeed;
	FVector TargetVel2D = WishDir * EffectiveTargetSpeed;
	
	float InterpSpeed = WishDir.IsZero() ? CustomGroundFriction : CustomGroundAcceleration;
	FVector NewVel2D = FMath::VInterpTo(CurrentVel2D, TargetVel2D, deltaTime, InterpSpeed);

	Velocity.X = NewVel2D.X; Velocity.Y = NewVel2D.Y;
	FVector SavedAccel = Acceleration; Acceleration = FVector::ZeroVector;
	
	Super::PhysWalking(deltaTime, Iterations);
	Acceleration = SavedAccel; 
}

void UPcQPlayerMovementComponent::PhysFalling(float deltaTime, int32 Iterations)
{
	if (deltaTime < MIN_TICK_TIME) return;

	if (BhopState == EBhopState::GroundPounding) {
		Acceleration = FVector::ZeroVector; Velocity.X = 0.f; Velocity.Y = 0.f;
		Super::PhysFalling(deltaTime, Iterations);
		return;
	}

	float TargetAirSpeed = MaxWalkSpeed;
	if (UPcMusicAnalysisSubsystem* Sub = GetWorld()->GetSubsystem<UPcMusicAnalysisSubsystem>()) {
		if (Sub->IsReadyForPlayback()) TargetAirSpeed = Sub->GetCurrentPulsePreset().MaxAirSpeed;
	}

	FVector WishDir = Acceleration.GetSafeNormal2D();
	FVector CurrentVel2D(Velocity.X, Velocity.Y, 0.f);
	FVector TargetVel2D = WishDir * TargetAirSpeed;

	float InterpSpeed = WishDir.IsZero() ? CustomAirFriction : CustomAirAcceleration;
	FVector NewVel2D = FMath::VInterpTo(CurrentVel2D, TargetVel2D, deltaTime, InterpSpeed);

	Velocity.X = NewVel2D.X; Velocity.Y = NewVel2D.Y;
	FVector SavedAccel = Acceleration; Acceleration = FVector::ZeroVector;

	Super::PhysFalling(deltaTime, Iterations);
	Acceleration = SavedAccel;
}