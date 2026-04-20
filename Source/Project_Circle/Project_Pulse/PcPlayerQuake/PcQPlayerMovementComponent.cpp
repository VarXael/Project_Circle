#include "PcQPlayerMovementComponent.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "Project_Circle/MusicSystem/MusicGameplaySystem/PcMusicAnalysisSubsystem.h"

// =============================================================================
//  CONSTRUCTOR
// =============================================================================

UPcQPlayerMovementComponent::UPcQPlayerMovementComponent()
{
	PrimaryComponentTick.bCanEverTick         = true;
	BrakingFrictionFactor                     = 0.f;
	GroundFriction                            = 0.f;
	bMaintainHorizontalGroundVelocity         = true;
	AirControl                                = 0.f;
	GravityScale                              = 1.0f;
	MaxWalkSpeed                              = 900.f;
	JumpZVelocity                             = 600.f;
	BrakingDecelerationWalking                = 0.f;
	BrakingDecelerationFalling                = 0.f;
	bUseSeparateBrakingFriction               = false;
	BrakingFriction                           = 0.f;
}

// =============================================================================
//  QUERIES
// =============================================================================

float UPcQPlayerMovementComponent::GetHorizontalSpeed() const
{
	return FVector(Velocity.X, Velocity.Y, 0.f).Size();
}

bool UPcQPlayerMovementComponent::IsInBhopChain() const
{
	return GetHorizontalSpeed() > MaxWalkSpeed * 1.05f;
}

// Symmetric beat window: catches actions both OnBeatWindowMS before AND after a beat.
bool UPcQPlayerMovementComponent::IsOnBeat() const
{
	UPcMusicAnalysisSubsystem* Sub = GetWorld()->GetSubsystem<UPcMusicAnalysisSubsystem>();
	if (!Sub || !Sub->IsReadyForPlayback()) return false;
	const int32 CurrentMS  = Sub->GetCurrentPlaybackTimeMS();
	const int32 NextBeatMS = Sub->GetNextGameplayBeatTimeMS();
	const int32 IntervalMS = FMath::RoundToInt(Sub->GetGameplayBeatIntervalMS());
	const int32 PrevBeatMS = NextBeatMS - IntervalMS;
	// Checks distance to NEXT beat (before it) AND to PREVIOUS beat (after it)
	const int32 MinDist = FMath::Min(FMath::Abs(NextBeatMS - CurrentMS),
	                                 FMath::Abs(CurrentMS  - PrevBeatMS));
	return MinDist <= OnBeatWindowMS;
}

// =============================================================================
//  BEAT ACTION FLASH
//
//  Call this whenever a successful active on-beat action occurs.
//  Sets the timer that the HUD reads to draw the ring pulse.
//  Also broadcasts OnActiveBeatAction so the character can auto-fire.
// =============================================================================

void UPcQPlayerMovementComponent::TriggerOnBeatFlash()
{
	OnBeatFlashTimer = OnBeatFlashDuration;
	OnActiveBeatAction.Broadcast();
}

// =============================================================================
//  TRIGGER BEAT JUMP
//
//  Priority:
//    1. WallSwim  → horizontal eject
//    2. Sliding   → check GP continuation flag; decrement phase or bounce
//    3. On ground → auto-bounce (+ bonus if flagged)
//    4. In air ↑  → skip (still rising)
//    5. In air ↓  → coyote queue
// =============================================================================

void UPcQPlayerMovementComponent::TriggerBeatJump()
{
	// 1. Wall swim eject
	if (BhopState == EBhopState::WallSwim)
	{
		EjectFromWall();
		return;
	}

	// 2. Slide — 2-beat immunity system
	if (BhopState == EBhopState::Sliding && IsMovingOnGround())
	{
		if (bSlideContinueRequested)
		{
			// Player pressed GP near this beat — reset to full speed, both phases
			bSlideContinueRequested = false;
			SlidePhase    = 2;
			bSlidePerfect = true;

			FVector Dir = FVector(Velocity.X, Velocity.Y, 0.f).GetSafeNormal();
			if (Dir.IsZero() && CharacterOwner)
				Dir = CharacterOwner->GetActorForwardVector().GetSafeNormal2D();

			Velocity.X = Dir.X * SlideSpeed;
			Velocity.Y = Dir.Y * SlideSpeed;
			Velocity.Z = 0.f;

			TriggerOnBeatFlash();  // auto-fire + ring pulse
			UE_LOG(LogTemp, Log, TEXT("[Slide] Continued — phase reset to 2, perfect."));
		}
		else
		{
			// No GP press — decrement phase
			SlidePhase--;

			if (SlidePhase <= 0)
			{
				// Time expired — world bounces the player out of the slide
				BhopState     = EBhopState::Active;
				bSlidePerfect = false;
				ApplyJumpVelocity();
				OnBhopLanded.Broadcast(GetHorizontalSpeed());
				UE_LOG(LogTemp, Log, TEXT("[Slide] Expired — bouncing."));
			}
			else
			{
				// Phase 1 (second beat): mark as decaying
				bSlidePerfect = false;
				UE_LOG(LogTemp, Log, TEXT("[Slide] Phase → %d (decaying)."), SlidePhase);
			}
		}
		return;
	}

	// 3–5. Standard active bounce
	if (BhopState != EBhopState::Active) return;

	if (IsMovingOnGround())
	{
		ApplyJumpVelocity();

		if (bBonusHopRequested)
		{
			Velocity.X *= BonusHopMultiplier;
			Velocity.Y *= BonusHopMultiplier;
			bBonusHopRequested = false;
			TriggerOnBeatFlash();  // auto-fire + ring pulse
			UE_LOG(LogTemp, Log, TEXT("[BonusHop] Fired! Speed=%.0f"), GetHorizontalSpeed());
		}

		OnBhopLanded.Broadcast(GetHorizontalSpeed());
	}
	else if (IsFalling() && Velocity.Z > 0.f)
	{
		return;  // still rising
	}
	else
	{
		bJumpQueuedForBeat = true;
		BeatQueueTimer     = BeatCoyoteWindow;
	}
}

// =============================================================================
//  INPUT
// =============================================================================

void UPcQPlayerMovementComponent::OnJumpPressed()
{
	if (BhopState == EBhopState::WallSwim)       return;
	if (BhopState == EBhopState::GroundPounding) return;
	if (BhopState == EBhopState::Sliding)        return;  // slide absorbs jump input

	if (IsOnBeat())
	{
		// On-beat press: flag the bonus. TriggerBeatJump consumes it on the next bounce.
		// If the beat fires essentially simultaneously, the flag is already set.
		bBonusHopRequested = true;
		UE_LOG(LogTemp, Log, TEXT("[BonusHop] Flagged on beat."));
	}
	else
	{
		// Off-beat: fixed 1-beat manual jump. No penalty, no sync adjustment.
		if (IsMovingOnGround())
			ApplyFixedBeatJump();
	}
}

void UPcQPlayerMovementComponent::OnJumpReleased() { /* no-op */ }

void UPcQPlayerMovementComponent::OnGroundPoundPressed()
{
	if (BhopState == EBhopState::WallSwim) return;

	// GP while sliding → request continuation for this beat
	if (BhopState == EBhopState::Sliding && IsMovingOnGround())
	{
		bSlideContinueRequested = true;
		return;
	}

	if (IsMovingOnGround())
	{
		// Ground GP → enter slide
		// On-beat: perfect slide (both phases full speed)
		// Off-beat: standard slide (phase 2 full, phase 1 decays)
		BhopState     = EBhopState::Sliding;
		SlidePhase    = 2;
		bSlidePerfect = IsOnBeat();
		ExitCurveJump();

		FVector Dir = Acceleration.GetSafeNormal2D();
		if (Dir.IsZero() && CharacterOwner)
			Dir = CharacterOwner->GetActorForwardVector().GetSafeNormal2D();

		Velocity.X = Dir.X * SlideSpeed;
		Velocity.Y = Dir.Y * SlideSpeed;
		Velocity.Z = 0.f;

		UE_LOG(LogTemp, Log, TEXT("[Slide] Entered from ground. Perfect=%d"), bSlidePerfect ? 1 : 0);
	}
	else if (IsFalling() && BhopState != EBhopState::GroundPounding)
	{
		// Air GP → slam down; ProcessLanded will enter Sliding
		ExitCurveJump();
		BhopState  = EBhopState::GroundPounding;
		Velocity.X = 0.f; Velocity.Y = 0.f; Velocity.Z = GroundPoundSlamSpeed;
	}
}

// =============================================================================
//  APPLY JUMP VELOCITY  — beat-synced, targets the next gameplay beat
// =============================================================================

void UPcQPlayerMovementComponent::ApplyJumpVelocity()
{
	if (CharacterOwner) CharacterOwner->JumpCurrentCount = 0;

	if (UPcMusicAnalysisSubsystem* Sub = GetWorld()->GetSubsystem<UPcMusicAnalysisSubsystem>())
	{
		if (Sub->IsReadyForPlayback())
		{
			const FPcMovementPreset& Preset  = Sub->GetCurrentPulsePreset();
			const float GameplayInterval     = Sub->GetGameplayBeatIntervalMS() / 1000.f;
			float TargetAirTime              = Sub->GetTimeUntilNextGameplayBeat();
			if (TargetAirTime < GameplayInterval * 0.25f) TargetAirTime += GameplayInterval;
			ApplyArcWithAirTime(Preset.PeakHeightCM, TargetAirTime);
			return;
		}
	}
	Velocity.Z = FMath::Max(Velocity.Z, JumpZVelocity);
	SetMovementMode(MOVE_Falling);
}

// =============================================================================
//  APPLY FIXED BEAT JUMP  — manual off-beat jump, always exactly 1 beat long
// =============================================================================

void UPcQPlayerMovementComponent::ApplyFixedBeatJump()
{
	if (CharacterOwner) CharacterOwner->JumpCurrentCount = 0;

	if (UPcMusicAnalysisSubsystem* Sub = GetWorld()->GetSubsystem<UPcMusicAnalysisSubsystem>())
	{
		if (Sub->IsReadyForPlayback())
		{
			const FPcMovementPreset& Preset = Sub->GetCurrentPulsePreset();
			const float AirTime             = Sub->GetGameplayBeatIntervalMS() / 1000.f;
			ApplyArcWithAirTime(Preset.PeakHeightCM, AirTime);
			return;
		}
	}
	Velocity.Z = FMath::Max(Velocity.Z, JumpZVelocity);
	SetMovementMode(MOVE_Falling);
}

void UPcQPlayerMovementComponent::ApplyArcWithAirTime(float PeakHeightCM, float AirTimeSec)
{
	if (CharacterOwner) CharacterOwner->JumpCurrentCount = 0;
	const float TimeToPeak = AirTimeSec * 0.5f;

	if (JumpCurve)
	{
		JumpCurveTimer      = 0.f;
		JumpCurveTotalTime  = AirTimeSec;
		JumpCurvePeakHeight = PeakHeightCM;
		JumpCurveLaunchZ    = CharacterOwner ? CharacterOwner->GetActorLocation().Z : 0.f;
		GravityScale        = 0.f;
		bUsingJumpCurve     = true;
		const float H0      = JumpCurve->GetFloatValue(0.f)    * PeakHeightCM;
		const float H1      = JumpCurve->GetFloatValue(0.001f) * PeakHeightCM;
		Velocity.Z          = (H1 - H0) / (0.001f * AirTimeSec);
		SetMovementMode(MOVE_Falling);
		return;
	}

	const float G  = (2.f * PeakHeightCM) / (TimeToPeak * TimeToPeak);
	GravityScale   = G / FMath::Abs(GetWorld()->GetDefaultGravityZ());
	Velocity.Z     = G * TimeToPeak;
	SetMovementMode(MOVE_Falling);
}

void UPcQPlayerMovementComponent::ExitCurveJump()
{
	if (!bUsingJumpCurve) return;
	bUsingJumpCurve = false;
	GravityScale    = 1.f;
}

// =============================================================================
//  WALL SWIM
// =============================================================================

void UPcQPlayerMovementComponent::EnterWallSwim(const FHitResult& Hit)
{
	if (BhopState == EBhopState::WallSwim) return;
	ExitCurveJump();
	BhopState       = EBhopState::WallSwim;
	SwimEntryNormal = Hit.ImpactNormal;
	Velocity       *= WallSwim_EntryVelocityRetain;

	if (UCapsuleComponent* Cap = CharacterOwner ? CharacterOwner->GetCapsuleComponent() : nullptr)
	{
		SwimPrevCollisionProfile = Cap->GetCollisionProfileName();
		Cap->SetCollisionProfileName(TEXT("NoCollision"));
	}

	SetMovementMode(MOVE_Flying);
	OnWallSwimChanged.Broadcast(1.f);
}

void UPcQPlayerMovementComponent::EjectFromWall()
{
	if (UCapsuleComponent* Cap = CharacterOwner ? CharacterOwner->GetCapsuleComponent() : nullptr)
		Cap->SetCollisionProfileName(SwimPrevCollisionProfile.IsNone() ? FName(TEXT("Pawn")) : SwimPrevCollisionProfile);

	BhopState = EBhopState::Active;

	FVector EjectDir = Acceleration.GetSafeNormal2D();
	if (EjectDir.IsNearlyZero()) EjectDir = SwimEntryNormal;

	const float EjectSpeed = FMath::Max(GetHorizontalSpeed(), MaxWalkSpeed) * WallSwim_EjectHorizMultiplier;
	Velocity.X   = EjectDir.X * EjectSpeed;
	Velocity.Y   = EjectDir.Y * EjectSpeed;
	Velocity.Z   = WallSwim_EjectUpKick;  // small fixed kick, normal gravity does the rest
	GravityScale = 1.f;
	SetMovementMode(MOVE_Falling);
	OnWallSwimChanged.Broadcast(0.f);
}

bool UPcQPlayerMovementComponent::IsAboveGround() const
{
	if (!CharacterOwner) return true;
	FVector Start = CharacterOwner->GetActorLocation();
	FVector End   = Start - FVector(0.f, 0.f, WallSwim_FloorCheckDist);
	FHitResult Hit;
	FCollisionQueryParams Params; Params.AddIgnoredActor(CharacterOwner);
	return !GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_WorldStatic, Params);
}

// =============================================================================
//  HANDLE IMPACT
// =============================================================================

void UPcQPlayerMovementComponent::HandleImpact(const FHitResult& Hit, float TimeSlice,
                                                const FVector& MoveDelta)
{
	if (MovementMode == MOVE_Falling
	    && BhopState != EBhopState::WallSwim
	    && BhopState != EBhopState::GroundPounding)
	{
		if (FMath::Abs(Hit.ImpactNormal.Z) < 0.4f
		    && GetHorizontalSpeed() >= WallSwim_EnterMinSpeed)
		{
			EnterWallSwim(Hit);
			return;
		}
	}
	Super::HandleImpact(Hit, TimeSlice, MoveDelta);
}

// =============================================================================
//  PROCESS LANDED
//
//  Ground pound landing → Sliding (NOT a jump).
//  Checks IsOnBeat() to determine if auto-fire should trigger.
// =============================================================================

void UPcQPlayerMovementComponent::ProcessLanded(const FHitResult& Hit, float remainingTime,
                                                 int32 Iterations)
{
	if (BhopState == EBhopState::WallSwim)
	{
		if (UCapsuleComponent* Cap = CharacterOwner ? CharacterOwner->GetCapsuleComponent() : nullptr)
			Cap->SetCollisionProfileName(SwimPrevCollisionProfile.IsNone() ? FName(TEXT("Pawn")) : SwimPrevCollisionProfile);
		BhopState = EBhopState::Active;
		OnWallSwimChanged.Broadcast(0.f);
		Super::ProcessLanded(Hit, remainingTime, Iterations);
		return;
	}

	if (BhopState == EBhopState::GroundPounding)
	{
		// Enter slide on landing — NOT a jump.
		BhopState     = EBhopState::Sliding;
		SlidePhase    = 2;
		bSlidePerfect = false;  // air landing is never "perfect" — phase 1 full, phase 2 decays
		ExitCurveJump();

		// Direction: carry pre-slam horizontal, or use look direction if straight down
		FVector Dir = FVector(Velocity.X, Velocity.Y, 0.f).GetSafeNormal();
		if (Dir.IsZero() && CharacterOwner)
			Dir = CharacterOwner->GetActorForwardVector().GetSafeNormal2D();

		const float EntrySpeed = FMath::Max(GetHorizontalSpeed(), SlideSpeed * 0.5f);
		Velocity.X = Dir.X * EntrySpeed;
		Velocity.Y = Dir.Y * EntrySpeed;
		Velocity.Z = 0.f;

		// If landing was on-beat, fire the weapon
		if (IsOnBeat()) TriggerOnBeatFlash();

		Super::ProcessLanded(Hit, remainingTime, Iterations);
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

void UPcQPlayerMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                                 FActorComponentTickFunction* ThisTickFunction)
{
	// Wall swim physics
	if (BhopState == EBhopState::WallSwim)
	{
		const float HR = FMath::Pow(1.f - FMath::Clamp(WallSwim_Damping   * DeltaTime, 0.f, 0.99f), 1.f);
		const float UR = FMath::Pow(1.f - FMath::Clamp(WallSwim_UpDamping * DeltaTime, 0.f, 0.99f), 1.f);
		Velocity.X *= HR; Velocity.Y *= HR;
		Velocity.Z  = Velocity.Z < 0.f ? Velocity.Z * HR : Velocity.Z * UR;
		if (!IsAboveGround() && Velocity.Z < 0.f) Velocity.Z = 0.f;
	}

	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	PreviousFrameSpeed = GetHorizontalSpeed();

	if (UPcMusicAnalysisSubsystem* Sub = GetWorld()->GetSubsystem<UPcMusicAnalysisSubsystem>())
		if (Sub->IsReadyForPlayback())
			MaxWalkSpeed = Sub->GetCurrentPulsePreset().MaxGroundSpeed;

	// Curve jump tick
	if (bUsingJumpCurve && JumpCurve && JumpCurveTotalTime > 0.f)
	{
		JumpCurveTimer    = FMath::Min(JumpCurveTimer + DeltaTime, JumpCurveTotalTime);
		const float T     = JumpCurveTimer / JumpCurveTotalTime;
		const float TNext = FMath::Min((JumpCurveTimer + DeltaTime) / JumpCurveTotalTime, 1.f);
		const float HNow  = JumpCurve->GetFloatValue(T)     * JumpCurvePeakHeight;
		const float HNext = JumpCurve->GetFloatValue(TNext) * JumpCurvePeakHeight;
		Velocity.Z        = (HNext - HNow) / DeltaTime;

		if (JumpCurveTimer >= JumpCurveTotalTime)
		{
			const float BackStep = 0.005f;
			const float HT       = JumpCurve->GetFloatValue(1.f)            * JumpCurvePeakHeight;
			const float HPre     = JumpCurve->GetFloatValue(1.f - BackStep) * JumpCurvePeakHeight;
			ExitCurveJump();
			Velocity.Z = FMath::Min((HT - HPre) / (BackStep * JumpCurveTotalTime), -80.f);
		}
	}

	// Beat flash timer
	if (OnBeatFlashTimer > 0.f)
		OnBeatFlashTimer = FMath::Max(0.f, OnBeatFlashTimer - DeltaTime);

	if (bJumpQueuedForBeat && (BeatQueueTimer -= DeltaTime) <= 0.f)
		bJumpQueuedForBeat = false;
}

// =============================================================================
//  PHYS WALKING
//
//  During slide, gently brings speed down toward the phase target.
//  The EffTarget logic in the base preserves speed above MaxWalkSpeed,
//  which keeps the slide momentum without fighting the deceleration.
// =============================================================================

void UPcQPlayerMovementComponent::PhysWalking(float deltaTime, int32 Iterations)
{
	if (deltaTime < MIN_TICK_TIME) return;

	// Slide speed management
	if (BhopState == EBhopState::Sliding)
	{
		const float SlideSpeedTarget = bSlidePerfect ? SlideSpeed : SlideSpeed * SlideDecayFactor;
		const float CurrentH         = GetHorizontalSpeed();

		if (CurrentH > SlideSpeedTarget + 1.f)
		{
			// Gradually bring speed toward target (gently in decay phase, instant on reset)
			const float NewH   = FMath::FInterpTo(CurrentH, SlideSpeedTarget, deltaTime, SlideDecayInterpSpeed);
			const FVector Dir2D = FVector(Velocity.X, Velocity.Y, 0.f).GetSafeNormal();
			Velocity.X = Dir2D.X * NewH;
			Velocity.Y = Dir2D.Y * NewH;
		}
	}

	float TargetSpeed = MaxWalkSpeed;
	if (UPcMusicAnalysisSubsystem* Sub = GetWorld()->GetSubsystem<UPcMusicAnalysisSubsystem>())
		if (Sub->IsReadyForPlayback()) TargetSpeed = Sub->GetCurrentPulsePreset().MaxGroundSpeed;

	const FVector WishDir   = Acceleration.GetSafeNormal2D();
	FVector       Vel2D(Velocity.X, Velocity.Y, 0.f);
	const float   Spd       = Vel2D.Size();
	const float   EffTarget = (Spd > TargetSpeed && !WishDir.IsZero()) ? Spd : TargetSpeed;
	const FVector NewVel2D  = FMath::VInterpTo(Vel2D, WishDir * EffTarget, deltaTime,
	                          WishDir.IsZero() ? CustomGroundFriction : CustomGroundAcceleration);
	Velocity.X = NewVel2D.X; Velocity.Y = NewVel2D.Y;

	FVector Saved = Acceleration; Acceleration = FVector::ZeroVector;
	Super::PhysWalking(deltaTime, Iterations);
	Acceleration = Saved;
}

// =============================================================================
//  PHYS FALLING
// =============================================================================

void UPcQPlayerMovementComponent::PhysFalling(float deltaTime, int32 Iterations)
{
	if (deltaTime < MIN_TICK_TIME) return;

	if (BhopState == EBhopState::GroundPounding)
	{
		Acceleration = FVector::ZeroVector; Velocity.X = 0.f; Velocity.Y = 0.f;
		Super::PhysFalling(deltaTime, Iterations);
		return;
	}

	float TargetAirSpeed = MaxWalkSpeed;
	if (UPcMusicAnalysisSubsystem* Sub = GetWorld()->GetSubsystem<UPcMusicAnalysisSubsystem>())
		if (Sub->IsReadyForPlayback()) TargetAirSpeed = Sub->GetCurrentPulsePreset().MaxAirSpeed;

	const FVector WishDir  = Acceleration.GetSafeNormal2D();
	const FVector Vel2D(Velocity.X, Velocity.Y, 0.f);
	const FVector NewVel2D = FMath::VInterpTo(Vel2D, WishDir * TargetAirSpeed, deltaTime,
	                         WishDir.IsZero() ? CustomAirFriction : CustomAirAcceleration);
	Velocity.X = NewVel2D.X; Velocity.Y = NewVel2D.Y;

	FVector Saved = Acceleration; Acceleration = FVector::ZeroVector;
	Super::PhysFalling(deltaTime, Iterations);
	Acceleration = Saved;
}