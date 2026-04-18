#include "PcQPlayerMovementComponent.h"
#include "GameFramework/Character.h"
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

// =============================================================================
//  PURE QUERIES
// =============================================================================

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
		ExitCurveJump(); // abort curve if mid-air
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
//
//  Two paths depending on whether a JumpCurve is assigned:
//
//  CURVE PATH  — Stores the beat-sync timing and peak height, sets
//                GravityScale = 0, and lets TickComponent drive Velocity.Z
//                frame-by-frame from the curve's position derivative.
//                The character traces the exact shape the designer drew.
//
//  DEFAULT PATH — Existing constant-gravity parabola.  GravityScale is tuned
//                 so the apex is reached at half the target air time, which
//                 guarantees landing exactly on the next gameplay beat.
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

			// ── CURVE PATH ───────────────────────────────────────────────────
			if (JumpCurve)
			{
				JumpCurveTimer      = 0.f;
				JumpCurveTotalTime  = TargetAirTime;
				JumpCurvePeakHeight = PeakHeight;
				JumpCurveLaunchZ    = (CharacterOwner) ? CharacterOwner->GetActorLocation().Z : 0.f;

				// Disable engine gravity — we drive Z from the curve derivative.
				GravityScale        = 0.f;
				bUsingJumpCurve     = true;

				// Seed an initial upward velocity so the first frame looks right.
				// Sample a tiny step ahead on the curve to get the derivative at t=0.
				const float StepT    = 0.001f;
				const float H0       = JumpCurve->GetFloatValue(0.f)    * PeakHeight;
				const float H1       = JumpCurve->GetFloatValue(StepT)  * PeakHeight;
				Velocity.Z           = (H1 - H0) / (StepT * TargetAirTime);

				SetMovementMode(MOVE_Falling);
				return;
			}

			// ── DEFAULT PARABOLA ─────────────────────────────────────────────
			const float RequiredGravity = (2.f * PeakHeight) / (TimeToPeak * TimeToPeak);
			const float BaseGravity     = FMath::Abs(GetWorld()->GetDefaultGravityZ());
			GravityScale                = RequiredGravity / BaseGravity;
			Velocity.Z                  = RequiredGravity * TimeToPeak;
			SetMovementMode(MOVE_Falling);
			return;
		}
	}

	// Fallback when music system isn't ready.
	Velocity.Z = FMath::Max(Velocity.Z, JumpZVelocity);
	SetMovementMode(MOVE_Falling);
}

// =============================================================================
//  EXIT CURVE JUMP
//  Cleans up curve state and restores gravity.  Safe to call at any time.
// =============================================================================

void UPcQPlayerMovementComponent::ExitCurveJump()
{
	if (!bUsingJumpCurve) return;
	bUsingJumpCurve = false;
	// Restore engine gravity.  The next ApplyJumpVelocity will overwrite this
	// with the beat-tuned value anyway, so 1.0 is a safe resting state.
	GravityScale = 1.f;
}

// =============================================================================
//  PROCESS LANDED
// =============================================================================

void UPcQPlayerMovementComponent::ProcessLanded(const FHitResult& Hit, float remainingTime, int32 Iterations)
{
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

	// If a curve jump was still running when we landed (early platform, etc.),
	// exit cleanly before the base class processes the landing.
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

void UPcQPlayerMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                                 FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ── Wipe detection ───────────────────────────────────────────────────────
	const float CurrentSpeed = GetHorizontalSpeed();
	if (PreviousFrameSpeed > 300.f && CurrentSpeed < 50.f)
		UE_LOG(LogTemp, Error, TEXT("[DEBUG-BHOP] WIPE DETECTED!"));
	PreviousFrameSpeed = CurrentSpeed;

	// ── Sync max speed from music system ─────────────────────────────────────
	if (UPcMusicAnalysisSubsystem* Sub = GetWorld()->GetSubsystem<UPcMusicAnalysisSubsystem>())
	{
		if (Sub->IsReadyForPlayback())
			MaxWalkSpeed = Sub->GetCurrentPulsePreset().MaxGroundSpeed;
	}

	// ── Curve jump — drive Velocity.Z from position derivative ───────────────
	//
	//  Sample the curve at the current and next normalised time, compute the
	//  height difference, and express it as a velocity.  Because GravityScale
	//  is 0 the engine won't fight us.
	//
	//  We deliberately do NOT touch Velocity.X/Y here — horizontal movement
	//  still goes through the normal PhysWalking / PhysFalling paths so air
	//  control and momentum feel exactly the same as a standard jump.
	//
	if (bUsingJumpCurve && JumpCurve && JumpCurveTotalTime > 0.f)
	{
		JumpCurveTimer = FMath::Min(JumpCurveTimer + DeltaTime, JumpCurveTotalTime);

		const float T     = JumpCurveTimer / JumpCurveTotalTime;
		const float TNext = FMath::Min((JumpCurveTimer + DeltaTime) / JumpCurveTotalTime, 1.f);

		const float HeightNow  = JumpCurve->GetFloatValue(T)     * JumpCurvePeakHeight;
		const float HeightNext = JumpCurve->GetFloatValue(TNext) * JumpCurvePeakHeight;

		// Express the height delta as a frame velocity.  This is sampled at the
		// CURRENT time so the driving signal is always ahead of the physics step.
		Velocity.Z = (HeightNext - HeightNow) / DeltaTime;

		// Curve complete — hand back to normal gravity so the engine can settle
		// the character onto the floor cleanly.
		if (JumpCurveTimer >= JumpCurveTotalTime)
		{
			ExitCurveJump();
			// Give a tiny downward nudge so the engine registers the character
			// as falling and triggers a landing event.
			Velocity.Z = FMath::Min(Velocity.Z, -10.f);
		}
	}

	// ── Charge timer ─────────────────────────────────────────────────────────
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
//  PHYS WALKING  (custom momentum carry-over, unchanged)
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