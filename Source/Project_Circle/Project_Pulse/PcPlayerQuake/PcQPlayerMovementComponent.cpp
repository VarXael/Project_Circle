#include "PcQPlayerMovementComponent.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "Project_Circle/MusicSystem/MusicGameplaySystem/PcMusicAnalysisSubsystem.h"

UPcQPlayerMovementComponent::UPcQPlayerMovementComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	BrakingFrictionFactor         = 0.f;
	GroundFriction                = 0.f;
	bMaintainHorizontalGroundVelocity = true;
	AirControl                    = 0.f;
	GravityScale                  = 1.0f;
	MaxWalkSpeed                  = 900.f;
	JumpZVelocity                 = 600.f;
	BrakingDecelerationWalking    = 0.f;
	BrakingDecelerationFalling    = 0.f;
	bUseSeparateBrakingFriction   = false;
	BrakingFriction               = 0.f;
}

float UPcQPlayerMovementComponent::GetChargeAlpha() const
{
	if (UPcMusicAnalysisSubsystem* Sub = GetWorld()->GetSubsystem<UPcMusicAnalysisSubsystem>())
	{
		if (Sub->IsReadyForPlayback() && ChargeTimer > 0.f)
			return FMath::Clamp(ChargeTimer / Sub->GetCurrentPulsePreset().ChargeTime, 0.f, 1.f);
	}
	return 0.f;
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
//  INPUT SURFACE
// =============================================================================

void UPcQPlayerMovementComponent::TriggerBeatJump()
{
	if (BhopState == EBhopState::WallCushioned)
	{
		// ── BEAT WALL JUMP ───────────────────────────────────────────────────
		// Read player's WASD input so they can steer the wall jump
		FVector WishDir = Acceleration.GetSafeNormal2D();
		FVector LaunchDir = MagneticWallNormal;
		
		// If they are pressing a direction that isn't directly INTO the wall, blend it
		if (!WishDir.IsZero() && FVector::DotProduct(WishDir, MagneticWallNormal) > -0.3f)
		{
			LaunchDir = (MagneticWallNormal + WishDir).GetSafeNormal();
		}

		Velocity = LaunchDir * WallBounceForce;
		Velocity.Z = WallBounceForce * 0.7f; // Add vertical pop
		
		BhopState = EBhopState::Active;
		bJumpQueuedForBeat = false;
		ExitCurveJump();
		
		OnBhopLanded.Broadcast(GetHorizontalSpeed());
		return;
	}

	if (BhopState != EBhopState::Active) return;
	if (IsFalling() && Velocity.Z > 0.f) return;

	if (IsMovingOnGround())
	{
		ApplyJumpVelocity();
		OnBhopLanded.Broadcast(GetHorizontalSpeed());
	}
	else
	{
		bJumpQueuedForBeat = true;
		BeatQueueTimer    = BeatCoyoteWindow;
	}
}

void UPcQPlayerMovementComponent::OnJumpPressed()
{
	if (BhopState == EBhopState::Idle) { BhopState = EBhopState::Charging; ChargeTimer = 0.f; }
	else CancelAutoBhop();
}

void UPcQPlayerMovementComponent::OnJumpReleased()
{
	if (BhopState == EBhopState::Charging) CancelAutoBhop();
}

void UPcQPlayerMovementComponent::OnGroundPoundPressed()
{
	if (IsFalling() && BhopState != EBhopState::GroundPounding)
	{
		ExitCurveJump(); 
		BhopState  = EBhopState::GroundPounding;
		Velocity.X = 0.f; Velocity.Y = 0.f; Velocity.Z = GroundPoundSlamSpeed;
	}
}

void UPcQPlayerMovementComponent::ActivateAutoBhop()
{
	BhopState    = EBhopState::Active;
	ChargeTimer  = 0.f;
	OnBhopActivated.Broadcast();
	if (IsMovingOnGround()) ApplyJumpVelocity();
}

void UPcQPlayerMovementComponent::CancelAutoBhop()
{
	ExitCurveJump();
	BhopState          = EBhopState::Idle;
	ChargeTimer        = 0.f;
	bJumpQueuedForBeat = false;
	OnBhopCancelled.Broadcast();
}

// =============================================================================
//  APPLY JUMP VELOCITY
// =============================================================================

void UPcQPlayerMovementComponent::ApplyJumpVelocity()
{
	if (CharacterOwner) CharacterOwner->JumpCurrentCount = 0;

	if (UPcMusicAnalysisSubsystem* Sub = GetWorld()->GetSubsystem<UPcMusicAnalysisSubsystem>())
	{
		if (Sub->IsReadyForPlayback())
		{
			const FPcMovementPreset& Preset       = Sub->GetCurrentPulsePreset();
			const float GameplayInterval           = Sub->GetGameplayBeatIntervalMS() / 1000.f;

			float TargetAirTime = Sub->GetTimeUntilNextGameplayBeat();
			if (TargetAirTime < GameplayInterval * 0.25f) TargetAirTime += GameplayInterval;

			const float PeakHeight  = Preset.PeakHeightCM;
			const float TimeToPeak  = TargetAirTime * 0.5f;

			if (JumpCurve)
			{
				JumpCurveTimer      = 0.f;
				JumpCurveTotalTime  = TargetAirTime;
				JumpCurvePeakHeight = PeakHeight;
				JumpCurveLaunchZ    = (CharacterOwner) ? CharacterOwner->GetActorLocation().Z : 0.f;

				GravityScale        = 0.f;
				bUsingJumpCurve     = true;

				const float StepT    = 0.001f;
				const float H0       = JumpCurve->GetFloatValue(0.f)    * PeakHeight;
				const float H1       = JumpCurve->GetFloatValue(StepT)  * PeakHeight;
				Velocity.Z           = (H1 - H0) / (StepT * TargetAirTime);

				SetMovementMode(MOVE_Falling);
				return;
			}

			const float RequiredGravity = (2.f * PeakHeight) / (TimeToPeak * TimeToPeak);
			const float BaseGravity     = FMath::Abs(GetWorld()->GetDefaultGravityZ());
			GravityScale                = RequiredGravity / BaseGravity;
			Velocity.Z                  = RequiredGravity * TimeToPeak;
			SetMovementMode(MOVE_Falling);
			return;
		}
	}

	Velocity.Z = FMath::Max(Velocity.Z, JumpZVelocity);
	SetMovementMode(MOVE_Falling);
}

void UPcQPlayerMovementComponent::ExitCurveJump()
{
	if (!bUsingJumpCurve) return;
	bUsingJumpCurve = false;
	GravityScale = 1.f;
}

// =============================================================================
//  PROCESS LANDED
// =============================================================================

void UPcQPlayerMovementComponent::ProcessLanded(const FHitResult& Hit, float remainingTime, int32 Iterations)
{
	if (BhopState == EBhopState::WallCushioned) {
		BhopState = EBhopState::Active; 
	}

	if (BhopState == EBhopState::GroundPounding)
	{
		BhopState = EBhopState::Active;
		FVector WishDir = Acceleration.GetSafeNormal2D();
		if (WishDir.IsZero() && CharacterOwner)
			WishDir = CharacterOwner->GetActorForwardVector().GetSafeNormal2D();

		Velocity.X = WishDir.X * GroundPoundDashSpeed;
		Velocity.Y = WishDir.Y * GroundPoundDashSpeed;
		ExitCurveJump();
		ApplyJumpVelocity();
		return;
	}

	ExitCurveJump();
	Super::ProcessLanded(Hit, remainingTime, Iterations);

	if (BhopState == EBhopState::Active && bJumpQueuedForBeat)
	{
		bJumpQueuedForBeat = false;
		ApplyJumpVelocity();
		OnBhopLanded.Broadcast(GetHorizontalSpeed());
	}
}

// =============================================================================
//  TICK
// =============================================================================

void UPcQPlayerMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	// ── MAGNETIC CUSHION DETECTION ──────────────────────────────────────────
	if (MovementMode == MOVE_Falling && BhopState != EBhopState::GroundPounding)
	{
		FVector Start = CharacterOwner->GetActorLocation();
		// Blend velocity and input so we detect walls if they strafe into them
		FVector SweepDir = (Velocity + Acceleration).GetSafeNormal2D();
		if (SweepDir.IsZero()) SweepDir = CharacterOwner->GetActorForwardVector().GetSafeNormal2D();

		FVector End = Start + SweepDir * WallCushionThickness;
		FHitResult Hit;
		FCollisionQueryParams Params;
		Params.AddIgnoredActor(CharacterOwner);
		
		if (GetWorld()->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, ECC_Visibility, CharacterOwner->GetCapsuleComponent()->GetCollisionShape(), Params))
		{
			if (FMath::Abs(Hit.ImpactNormal.Z) < 0.2f) // Verify it is a steep wall
			{
				BhopState = EBhopState::WallCushioned;
				MagneticWallNormal = Hit.ImpactNormal;
				MagneticWallDistance = Hit.Distance;
				ExitCurveJump(); // Cancel beat-curves so we don't glitch out
			}
			else if (BhopState == EBhopState::WallCushioned) {
				BhopState = EBhopState::Active;
			}
		}
		else if (BhopState == EBhopState::WallCushioned) {
			BhopState = EBhopState::Active;
		}
	}

	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const float CurrentSpeed = GetHorizontalSpeed();
	PreviousFrameSpeed = CurrentSpeed;

	if (UPcMusicAnalysisSubsystem* Sub = GetWorld()->GetSubsystem<UPcMusicAnalysisSubsystem>())
	{
		if (Sub->IsReadyForPlayback())
			MaxWalkSpeed = Sub->GetCurrentPulsePreset().MaxGroundSpeed;
	}

	if (bUsingJumpCurve && JumpCurve && JumpCurveTotalTime > 0.f)
	{
		JumpCurveTimer = FMath::Min(JumpCurveTimer + DeltaTime, JumpCurveTotalTime);

		const float T     = JumpCurveTimer / JumpCurveTotalTime;
		const float TNext = FMath::Min((JumpCurveTimer + DeltaTime) / JumpCurveTotalTime, 1.f);

		const float HeightNow  = JumpCurve->GetFloatValue(T)     * JumpCurvePeakHeight;
		const float HeightNext = JumpCurve->GetFloatValue(TNext) * JumpCurvePeakHeight;

		Velocity.Z = (HeightNext - HeightNow) / DeltaTime;

		if (JumpCurveTimer >= JumpCurveTotalTime)
		{
			const float BackStep     = 0.005f;
			const float HTerminal    = JumpCurve->GetFloatValue(1.f)            * JumpCurvePeakHeight;
			const float HPreTerminal = JumpCurve->GetFloatValue(1.f - BackStep) * JumpCurvePeakHeight;
			const float TerminalVelZ = (HTerminal - HPreTerminal) / (BackStep * JumpCurveTotalTime);

			ExitCurveJump(); 
			Velocity.Z = FMath::Min(TerminalVelZ, -80.f);
		}
	}

	if (BhopState == EBhopState::Charging)
	{
		ChargeTimer += DeltaTime;
		OnBhopChargeUpdated.Broadcast(GetChargeAlpha());
		if (UPcMusicAnalysisSubsystem* Sub = GetWorld()->GetSubsystem<UPcMusicAnalysisSubsystem>())
		{
			if (Sub->IsReadyForPlayback() && ChargeTimer >= Sub->GetCurrentPulsePreset().ChargeTime)
				ActivateAutoBhop();
		}
	}

	if (bJumpQueuedForBeat && (BeatQueueTimer -= DeltaTime) <= 0.f)
		bJumpQueuedForBeat = false;
}

// =============================================================================
//  PHYS WALKING
// =============================================================================

void UPcQPlayerMovementComponent::PhysWalking(float deltaTime, int32 Iterations)
{
	if (deltaTime < MIN_TICK_TIME) return;

	float TargetSpeed = MaxWalkSpeed;
	if (UPcMusicAnalysisSubsystem* Sub = GetWorld()->GetSubsystem<UPcMusicAnalysisSubsystem>())
		if (Sub->IsReadyForPlayback()) TargetSpeed = Sub->GetCurrentPulsePreset().MaxGroundSpeed;

	const FVector WishDir     = Acceleration.GetSafeNormal2D();
	FVector       CurrentVel2D(Velocity.X, Velocity.Y, 0.f);
	const float   CurrentSpd  = CurrentVel2D.Size();

	const float   EffTarget   = (CurrentSpd > TargetSpeed && !WishDir.IsZero()) ? CurrentSpd : TargetSpeed;
	const FVector TargetVel2D = WishDir * EffTarget;
	const float   InterpSpd   = WishDir.IsZero() ? CustomGroundFriction : CustomGroundAcceleration;
	const FVector NewVel2D    = FMath::VInterpTo(CurrentVel2D, TargetVel2D, deltaTime, InterpSpd);

	Velocity.X = NewVel2D.X; Velocity.Y = NewVel2D.Y;

	FVector SavedAccel = Acceleration; Acceleration = FVector::ZeroVector;
	Super::PhysWalking(deltaTime, Iterations);
	Acceleration = SavedAccel;
}

// =============================================================================
//  PHYS FALLING
// =============================================================================

void UPcQPlayerMovementComponent::PhysFalling(float deltaTime, int32 Iterations)
{
	if (deltaTime < MIN_TICK_TIME) return;

	if (BhopState == EBhopState::WallCushioned)
	{
		// 1. Calculate how deep we are in the cushion (0.0 = edge, 1.0 = touching physical wall)
		float CushionAlpha = 1.0f - FMath::Clamp(MagneticWallDistance / WallCushionThickness, 0.0f, 1.0f);
		
		// 2. Soft-Brake the velocity heading INTO the wall (Spring compression)
		float InwardVel = FVector::DotProduct(Velocity, -MagneticWallNormal);
		if (InwardVel > 0.f)
		{
			// The deeper we get, the harder the magnetic field pushes back
			float BrakeForce = InwardVel * CushionAlpha * WallCushionStiffness * deltaTime;
			Velocity += MagneticWallNormal * BrakeForce;
		}

		// 3. Float Field: Dynamically reduce gravity the deeper we get (down to 10%)
		// This creates the "hang-time" without taking away upward/downward momentum
		float CustomGravityScale = FMath::Lerp(1.0f, 0.1f, CushionAlpha);
		float OldGravityScale = GravityScale;
		GravityScale *= CustomGravityScale;

		// 4. Call Super to process standard Air Control, Air Friction, and Gravity!
		// Because we didn't zero out Acceleration, the player can still steer.
		Super::PhysFalling(deltaTime, Iterations);

		// Restore gravity for next frame
		GravityScale = OldGravityScale;
		return;
	}

	if (BhopState == EBhopState::GroundPounding)
	{
		Acceleration = FVector::ZeroVector;
		Velocity.X   = 0.f;
		Velocity.Y   = 0.f;
		Super::PhysFalling(deltaTime, Iterations);
		return;
	}

	float TargetAirSpeed = MaxWalkSpeed;
	if (UPcMusicAnalysisSubsystem* Sub = GetWorld()->GetSubsystem<UPcMusicAnalysisSubsystem>())
		if (Sub->IsReadyForPlayback()) TargetAirSpeed = Sub->GetCurrentPulsePreset().MaxAirSpeed;

	const FVector WishDir     = Acceleration.GetSafeNormal2D();
	const FVector CurrentVel2D(Velocity.X, Velocity.Y, 0.f);
	const FVector TargetVel2D = WishDir * TargetAirSpeed;
	const float   InterpSpd   = WishDir.IsZero() ? CustomAirFriction : CustomAirAcceleration;
	const FVector NewVel2D    = FMath::VInterpTo(CurrentVel2D, TargetVel2D, deltaTime, InterpSpd);

	Velocity.X = NewVel2D.X; Velocity.Y = NewVel2D.Y;

	FVector SavedAccel = Acceleration; Acceleration = FVector::ZeroVector;
	Super::PhysFalling(deltaTime, Iterations);
	Acceleration = SavedAccel;
}