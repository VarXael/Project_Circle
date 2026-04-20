#include "PcQPlayerMovementComponent.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "Project_Circle/MusicSystem/MusicGameplaySystem/PcMusicAnalysisSubsystem.h"

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

bool UPcQPlayerMovementComponent::IsOnBeat() const
{
	UPcMusicAnalysisSubsystem* Sub = GetWorld()->GetSubsystem<UPcMusicAnalysisSubsystem>();
	if (!Sub || !Sub->IsReadyForPlayback()) return false;
	const int32 Now      = Sub->GetCurrentPlaybackTimeMS();
	const int32 Next     = Sub->GetNextGameplayBeatTimeMS();
	const int32 Interval = FMath::RoundToInt(Sub->GetGameplayBeatIntervalMS());
	return FMath::Min(FMath::Abs(Next - Now), FMath::Abs(Now - (Next - Interval))) <= OnBeatWindowMS;
}

float UPcQPlayerMovementComponent::GetBeatSnappedDuration(float BaseSec) const
{
	UPcMusicAnalysisSubsystem* Sub = GetWorld()->GetSubsystem<UPcMusicAnalysisSubsystem>();
	if (!Sub || !Sub->IsReadyForPlayback()) return BaseSec;
	const float IntervalSec    = Sub->GetGameplayBeatIntervalMS() / 1000.f;
	const float TimeToNext     = Sub->GetTimeUntilNextGameplayBeat();
	const float AlreadyElapsed = IntervalSec - TimeToNext;
	const int32 WholeBeats     = FMath::CeilToInt(BaseSec / IntervalSec);
	return FMath::Max((float)WholeBeats * IntervalSec - AlreadyElapsed, IntervalSec);
}

float UPcQPlayerMovementComponent::GetBoostCooldownAlpha() const
{
	return BoostCooldown <= 0.f ? 0.f : FMath::Clamp(BoostCooldown / FMath::Max(BoostBaseCooldownSec, 0.01f), 0.f, 1.f);
}

float UPcQPlayerMovementComponent::GetDoubleJumpCooldownAlpha() const
{
	if (DoubleJumpCooldown <= 0.f) return bDoubleJumpUsed ? 1.f : 0.f;
	return FMath::Clamp(DoubleJumpCooldown / FMath::Max(DoubleJumpBaseCooldownSec, 0.01f), 0.f, 1.f);
}

void UPcQPlayerMovementComponent::TriggerOnBeatFlash()
{
	OnBeatFlashTimer = OnBeatFlashDuration;
	OnActiveBeatAction.Broadcast();
}

// =============================================================================
//  POWER BOOST
// =============================================================================

void UPcQPlayerMovementComponent::ActivateBoost()
{
	BhopState     = EBhopState::PowerBoost;
	BoostTimer    = GetBeatSnappedDuration(BoostBaseDurationSec);
	BoostCooldown = 0.f;

	FVector Dir2D = FVector(Velocity.X, Velocity.Y, 0.f).GetSafeNormal();
	if (Dir2D.IsZero() && CharacterOwner)
		Dir2D = CharacterOwner->GetActorForwardVector().GetSafeNormal2D();

	Velocity.X = Dir2D.X * MaxWalkSpeed * BoostSpeedMultiplier;
	Velocity.Y = Dir2D.Y * MaxWalkSpeed * BoostSpeedMultiplier;
	Velocity.Z = 0.f;

	TriggerOnBeatFlash();
}

void UPcQPlayerMovementComponent::ExitBoost()
{
	BhopState     = EBhopState::Active;
	BoostTimer    = 0.f;
	BoostCooldown = GetBeatSnappedDuration(BoostBaseCooldownSec);
}

void UPcQPlayerMovementComponent::NotifyGunFired()
{
	if (BhopState == EBhopState::PowerBoost && BoostTimer > 0.f)
		BoostTimer = FMath::Min(BoostTimer + BoostExtendPerShot, GetBeatSnappedDuration(BoostBaseDurationSec) * 2.f);
}

// =============================================================================
//  DO JUMP
//
//  Single entry point for all player-initiated jumps.
//  Reads GP combo flags to apply chain bonuses, then clears them.
//  Preserves horizontal velocity — the arc only sets Z.
// =============================================================================

void UPcQPlayerMovementComponent::DoJump()
{
	// Snapshot horizontal before the arc calculation
	const float PreJumpH  = GetHorizontalSpeed();
	const FVector Dir2D   = FVector(Velocity.X, Velocity.Y, 0.f).GetSafeNormal();

	ApplyFixedBeatJump();  // sets Z, adjusts GravityScale

	// After arc sets Z, restore or improve horizontal based on context:
	// 1. Basic: never let the jump reduce horizontal speed below pre-jump value
	if (!Dir2D.IsZero() && GetHorizontalSpeed() < PreJumpH)
	{
		Velocity.X = Dir2D.X * PreJumpH;
		Velocity.Y = Dir2D.Y * PreJumpH;
	}

	// 2. GP combo bonus
	if (bGPLandedRecently && GPComboTimer > 0.f)
	{
		const FVector WishDir = Acceleration.GetSafeNormal2D().IsZero() ? Dir2D : Acceleration.GetSafeNormal2D();
		float Bonus = GPJumpHorizBoost;

		if (bBoostActiveOnGPLand)
			Bonus *= GPBoostJumpHorizMult;

		Velocity.X += WishDir.X * Bonus;
		Velocity.Y += WishDir.Y * Bonus;

		bGPLandedRecently    = false;
		bBoostActiveOnGPLand = false;
		GPComboTimer         = 0.f;

		TriggerOnBeatFlash();
		UE_LOG(LogTemp, Log, TEXT("[Chain] GP combo jump. Bonus=%.0f BoostChain=%d"), Bonus, bBoostActiveOnGPLand ? 1 : 0);
	}

	// Bonus hop (on-beat jump press)
	if (bBonusHopRequested)
	{
		const FVector WD = Acceleration.GetSafeNormal2D().IsZero() ? Dir2D : Acceleration.GetSafeNormal2D();
		Velocity.X += WD.X * BonusHopSpeedBoost;
		Velocity.Y += WD.Y * BonusHopSpeedBoost;
		bBonusHopRequested = false;
		TriggerOnBeatFlash();
	}

	// Clamp to hard speed cap
	const float HardCap = MaxWalkSpeed * HardSpeedCapMult;
	const float FinalH  = GetHorizontalSpeed();
	if (FinalH > HardCap)
	{
		const FVector FinalDir = FVector(Velocity.X, Velocity.Y, 0.f).GetSafeNormal();
		Velocity.X = FinalDir.X * HardCap;
		Velocity.Y = FinalDir.Y * HardCap;
	}
}

// =============================================================================
//  TRIGGER BEAT JUMP — called every gameplay beat
// =============================================================================

void UPcQPlayerMovementComponent::TriggerBeatJump()
{
	// Wall spring: beat fires during compression → mark pending, eject happens in Tick
	if (BhopState == EBhopState::WallSwim)
	{
		bWallBeatPending = true;
		return;
	}

	if (BhopState != EBhopState::Active && BhopState != EBhopState::PowerBoost) return;

	if (IsMovingOnGround())
	{
		if (bAutoJumpEnabled || bBonusHopRequested || BhopState == EBhopState::PowerBoost)
		{
			if (bAutoJumpEnabled || bBonusHopRequested)
			{
				// Exit boost before jump so DoJump reads the state correctly
				const bool bWasBoosting = (BhopState == EBhopState::PowerBoost);
				if (bWasBoosting) ExitBoost();

				DoJump();
				OnBhopLanded.Broadcast(GetHorizontalSpeed());
			}
		}
		// Even if no jump fires: tell listeners the beat hit the ground
		if (!bAutoJumpEnabled && !bBonusHopRequested && BhopState != EBhopState::PowerBoost)
			OnBhopLanded.Broadcast(GetHorizontalSpeed());
	}
	else if (!IsFalling() || Velocity.Z <= 0.f)
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
	if (BhopState == EBhopState::GroundPounding) return;

	// Wall spring: jump during compression = immediate eject
	if (BhopState == EBhopState::WallSwim)
	{
		EjectFromWall(false, true);
		return;
	}

	if (IsOnBeat()) bBonusHopRequested = true;

	if (IsMovingOnGround())
	{
		DoJump();
		OnBhopLanded.Broadcast(GetHorizontalSpeed());
	}
	else if (IsFalling())
	{
		if (DJumpReady() || IsOnBeat())
		{
			bDoubleJumpUsed    = true;
			DoubleJumpCooldown = GetBeatSnappedDuration(DoubleJumpBaseCooldownSec);
			DoJump();
			if (IsOnBeat()) TriggerOnBeatFlash();
		}
		else
		{
			bJumpInputBuffered   = true;
			JumpInputBufferTimer = JumpInputBufferWindow;
		}
	}
}

void UPcQPlayerMovementComponent::OnJumpReleased() {}

void UPcQPlayerMovementComponent::OnGroundPoundPressed()
{
	if (BhopState == EBhopState::WallSwim) return;

	if (IsMovingOnGround())
	{
		// Slide: on-beat → boost + start slide. Off-beat → slide at current speed.
		BhopState = EBhopState::PowerBoost;  // reuse boost state for the slide
		BoostTimer = GetBeatSnappedDuration(BoostBaseDurationSec);
		BoostCooldown = 0.f;

		const bool bOnBeat = IsOnBeat();
		FVector Dir2D = FVector(Velocity.X, Velocity.Y, 0.f).GetSafeNormal();
		if (Dir2D.IsZero() && CharacterOwner)
			Dir2D = CharacterOwner->GetActorForwardVector().GetSafeNormal2D();

		const float EntrySpd = GetHorizontalSpeed() + (bOnBeat ? SlideEntryBoost : 0.f);
		const float BoostSpd = MaxWalkSpeed * BoostSpeedMultiplier;
		// Start at whichever is higher: current speed+boost entry, or boost speed
		const float StartSpd = FMath::Max(EntrySpd, BoostSpd);
		Velocity.X = Dir2D.X * StartSpd;
		Velocity.Y = Dir2D.Y * StartSpd;
		Velocity.Z = 0.f;

		if (bOnBeat) TriggerOnBeatFlash();
	}
	else if (IsFalling() && BhopState != EBhopState::GroundPounding)
	{
		ExitCurveJump();
		BhopState  = EBhopState::GroundPounding;
		Velocity.X = 0.f; Velocity.Y = 0.f; Velocity.Z = GroundPoundSlamSpeed;
	}
}

void UPcQPlayerMovementComponent::OnGroundPoundReleased() {}

// =============================================================================
//  ARC MATH
// =============================================================================

void UPcQPlayerMovementComponent::ApplyJumpVelocity()
{
	if (CharacterOwner) CharacterOwner->JumpCurrentCount = 0;
	if (UPcMusicAnalysisSubsystem* Sub = GetWorld()->GetSubsystem<UPcMusicAnalysisSubsystem>())
	{
		if (Sub->IsReadyForPlayback())
		{
			const FPcMovementPreset& P = Sub->GetCurrentPulsePreset();
			const float Interval       = Sub->GetGameplayBeatIntervalMS() / 1000.f;
			float AirTime              = Sub->GetTimeUntilNextGameplayBeat();
			if (AirTime < Interval * 0.25f) AirTime += Interval;
			ApplyArcWithAirTime(P.PeakHeightCM, AirTime);
			return;
		}
	}
	Velocity.Z = FMath::Max(Velocity.Z, JumpZVelocity);
	SetMovementMode(MOVE_Falling);
}

void UPcQPlayerMovementComponent::ApplyFixedBeatJump()
{
	if (CharacterOwner) CharacterOwner->JumpCurrentCount = 0;
	if (UPcMusicAnalysisSubsystem* Sub = GetWorld()->GetSubsystem<UPcMusicAnalysisSubsystem>())
	{
		if (Sub->IsReadyForPlayback())
		{
			const FPcMovementPreset& P = Sub->GetCurrentPulsePreset();
			ApplyArcWithAirTime(P.PeakHeightCM, Sub->GetGameplayBeatIntervalMS() / 1000.f);
			return;
		}
	}
	Velocity.Z = FMath::Max(Velocity.Z, JumpZVelocity);
	SetMovementMode(MOVE_Falling);
}

void UPcQPlayerMovementComponent::ApplyArcWithAirTime(float PeakHeightCM, float AirTimeSec)
{
	if (CharacterOwner) CharacterOwner->JumpCurrentCount = 0;
	// NOTE: X/Y are NOT touched here. Callers preserve or boost horizontal separately.
	if (JumpCurve)
	{
		JumpCurveTimer = 0.f; JumpCurveTotalTime = AirTimeSec; JumpCurvePeakHeight = PeakHeightCM;
		JumpCurveLaunchZ = CharacterOwner ? CharacterOwner->GetActorLocation().Z : 0.f;
		GravityScale = 0.f; bUsingJumpCurve = true;
		const float H0 = JumpCurve->GetFloatValue(0.f)    * PeakHeightCM;
		const float H1 = JumpCurve->GetFloatValue(0.001f) * PeakHeightCM;
		Velocity.Z = (H1 - H0) / (0.001f * AirTimeSec);
		SetMovementMode(MOVE_Falling); return;
	}
	const float T  = AirTimeSec * 0.5f;
	const float G  = (2.f * PeakHeightCM) / (T * T);
	GravityScale   = G / FMath::Abs(GetWorld()->GetDefaultGravityZ());
	Velocity.Z     = G * T;
	SetMovementMode(MOVE_Falling);
}

void UPcQPlayerMovementComponent::ExitCurveJump()
{
	if (!bUsingJumpCurve) return;
	bUsingJumpCurve = false; GravityScale = 1.f;
}

// =============================================================================
//  WALL SPRING
// =============================================================================

void UPcQPlayerMovementComponent::EnterWallSpring(const FHitResult& Hit)
{
	if (BhopState == EBhopState::WallSwim) return;
	ExitCurveJump();

	BhopState            = EBhopState::WallSwim;
	WallEntryNormal      = Hit.ImpactNormal;
	WallCompressionTimer = 0.f;
	bWallBeatPending     = false;

	// Ghost the capsule so we briefly enter the wall surface
	if (UCapsuleComponent* Cap = CharacterOwner ? CharacterOwner->GetCapsuleComponent() : nullptr)
	{
		WallPrevCollisionProfile = Cap->GetCollisionProfileName();
		Cap->SetCollisionProfileName(TEXT("NoCollision"));
	}
	SetMovementMode(MOVE_Flying);
	OnWallSwimChanged.Broadcast(1.f);
	UE_LOG(LogTemp, Log, TEXT("[Wall] Entered. Speed=%.0f"), GetHorizontalSpeed());
}

void UPcQPlayerMovementComponent::EjectFromWall(bool bBeatBoost, bool bJumpEject)
{
	if (UCapsuleComponent* Cap = CharacterOwner ? CharacterOwner->GetCapsuleComponent() : nullptr)
		Cap->SetCollisionProfileName(WallPrevCollisionProfile.IsNone() ? FName(TEXT("Pawn")) : WallPrevCollisionProfile);

	BhopState = EBhopState::Active;

	// Eject direction: WASD input if pressed, otherwise reflect off wall normal
	FVector EjectDir = Acceleration.GetSafeNormal2D();
	if (EjectDir.IsNearlyZero())
	{
		// Reflect velocity off wall normal — ping-pong feel
		const FVector Vel2D  = FVector(Velocity.X, Velocity.Y, 0.f);
		const FVector Refl   = Vel2D - 2.f * FVector::DotProduct(Vel2D, -WallEntryNormal) * (-WallEntryNormal);
		EjectDir             = Refl.GetSafeNormal2D();
		if (EjectDir.IsZero()) EjectDir = WallEntryNormal;
	}

	float EjectSpd = Wall_EjectSpeed;
	if (bBeatBoost) EjectSpd += Wall_BeatEjectBoost;

	Velocity.X = EjectDir.X * EjectSpd;
	Velocity.Y = EjectDir.Y * EjectSpd;
	Velocity.Z = bJumpEject ? Wall_JumpEjectUpKick : 100.f;  // small kick on auto, strong on jump

	GravityScale = 1.f;
	SetMovementMode(MOVE_Falling);
	OnWallSwimChanged.Broadcast(0.f);

	if (bBeatBoost) TriggerOnBeatFlash();
	UE_LOG(LogTemp, Log, TEXT("[Wall] Ejected. Beat=%d Jump=%d Speed=%.0f"), bBeatBoost?1:0, bJumpEject?1:0, EjectSpd);
}

bool UPcQPlayerMovementComponent::IsAboveGround() const
{
	if (!CharacterOwner) return true;
	FVector Start = CharacterOwner->GetActorLocation();
	FHitResult Hit; FCollisionQueryParams P; P.AddIgnoredActor(CharacterOwner);
	return !GetWorld()->LineTraceSingleByChannel(Hit, Start, Start - FVector(0.f, 0.f, Wall_FloorCheckDist), ECC_WorldStatic, P);
}

// =============================================================================
//  HANDLE IMPACT
// =============================================================================

void UPcQPlayerMovementComponent::HandleImpact(const FHitResult& Hit, float TimeSlice, const FVector& MoveDelta)
{
	if (MovementMode == MOVE_Falling
	    && BhopState != EBhopState::WallSwim
	    && BhopState != EBhopState::GroundPounding)
	{
		if (FMath::Abs(Hit.ImpactNormal.Z) < 0.4f && GetHorizontalSpeed() >= Wall_EnterMinSpeed)
		{
			EnterWallSpring(Hit); return;
		}
	}
	Super::HandleImpact(Hit, TimeSlice, MoveDelta);
}

// =============================================================================
//  PROCESS LANDED
// =============================================================================

void UPcQPlayerMovementComponent::ProcessLanded(const FHitResult& Hit, float remainingTime, int32 Iterations)
{
	if (BhopState == EBhopState::WallSwim)
	{
		if (UCapsuleComponent* Cap = CharacterOwner ? CharacterOwner->GetCapsuleComponent() : nullptr)
			Cap->SetCollisionProfileName(WallPrevCollisionProfile.IsNone() ? FName(TEXT("Pawn")) : WallPrevCollisionProfile);
		BhopState = EBhopState::Active; OnWallSwimChanged.Broadcast(0.f);
		Super::ProcessLanded(Hit, remainingTime, Iterations); return;
	}

	if (BhopState == EBhopState::GroundPounding)
	{
		// GP landing: go to Active, open combo window
		BhopState             = EBhopState::Active;
		bGPLandedRecently     = true;
		bBoostActiveOnGPLand  = false;  // boost wasn't active (we came straight down)
		GPComboTimer          = GPComboWindowSec;
		ExitCurveJump();
		Super::ProcessLanded(Hit, remainingTime, Iterations);
		return;
	}

	// Boost landing
	if (BhopState == EBhopState::PowerBoost)
	{
		bBoostActiveOnGPLand = true;  // boost WAS active on this landing
		ExitBoost();                  // exits boost, starts cooldown
	}

	ExitCurveJump();
	// DJ fully resets on landing (in-air cooldown only)
	bDoubleJumpUsed    = false;
	DoubleJumpCooldown = 0.f;

	Super::ProcessLanded(Hit, remainingTime, Iterations);

	// Consume buffered jump
	if (BhopState == EBhopState::Active && bJumpInputBuffered && JumpInputBufferTimer > 0.f)
	{
		bJumpInputBuffered = false; JumpInputBufferTimer = 0.f;
		DoJump();
		OnBhopLanded.Broadcast(GetHorizontalSpeed());
		return;
	}

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
	// ── Wall spring compression ───────────────────────────────────────────────
	if (BhopState == EBhopState::WallSwim)
	{
		WallCompressionTimer += DeltaTime;

		// Rapid velocity decay (spring compressing)
		const float Decay = FMath::Pow(1.f - FMath::Clamp(Wall_CompressionDecay * DeltaTime, 0.f, 0.99f), 1.f);
		Velocity.X *= Decay; Velocity.Y *= Decay;
		// Keep slight upward momentum, kill downward
		if (Velocity.Z < 0.f) Velocity.Z *= Decay;

		if (!IsAboveGround() && Velocity.Z < 0.f) Velocity.Z = 0.f;

		// Beat fired during compression → eject with bonus
		if (bWallBeatPending)
		{
			bWallBeatPending = false;
			EjectFromWall(true, false);
		}
		// Auto-eject after compression window
		else if (WallCompressionTimer >= Wall_CompressionSec)
		{
			EjectFromWall(false, false);
		}
	}

	// ── Power boost tick ─────────────────────────────────────────────────────
	if (BhopState == EBhopState::PowerBoost && IsMovingOnGround())
	{
		const float BoostSpd  = MaxWalkSpeed * BoostSpeedMultiplier;
		const FVector WishDir = Acceleration.GetSafeNormal2D();

		if (!WishDir.IsZero())
		{
			const FVector Vel2D  = FVector(Velocity.X, Velocity.Y, 0.f);
			const FVector Target = WishDir * BoostSpd;
			const FVector New2D  = FMath::VInterpTo(Vel2D, Target, DeltaTime, CustomGroundAcceleration);
			Velocity.X = New2D.X; Velocity.Y = New2D.Y;
		}
		else
		{
			const float FR = FMath::Pow(1.f - FMath::Clamp(2.f * DeltaTime, 0.f, 0.99f), 1.f);
			Velocity.X *= FR; Velocity.Y *= FR;
		}

		BoostTimer -= DeltaTime;
		if (BoostTimer <= 0.f) ExitBoost();
	}

	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	PreviousFrameSpeed = GetHorizontalSpeed();

	if (UPcMusicAnalysisSubsystem* Sub = GetWorld()->GetSubsystem<UPcMusicAnalysisSubsystem>())
		if (Sub->IsReadyForPlayback())
			MaxWalkSpeed = Sub->GetCurrentPulsePreset().MaxGroundSpeed;

	// ── Overspeed decay ───────────────────────────────────────────────────────
	// Above MaxWalkSpeed: gentle drag pulls back toward soft cap.
	// Hard cap (HardSpeedCapMult) is enforced as an absolute ceiling.
	if (BhopState != EBhopState::PowerBoost)
	{
		const float CurH    = GetHorizontalSpeed();
		const float HardCap = MaxWalkSpeed * HardSpeedCapMult;

		if (CurH > MaxWalkSpeed)
		{
			// Proportional decay — stronger the further above the cap
			const float Excess    = CurH - MaxWalkSpeed;
			const float DecayThis = OverspeedDecayRate * DeltaTime * (Excess / MaxWalkSpeed);
			const float NewH      = FMath::Max(CurH - DecayThis, MaxWalkSpeed);
			const FVector Dir2D   = FVector(Velocity.X, Velocity.Y, 0.f).GetSafeNormal();
			if (!Dir2D.IsZero())
			{
				Velocity.X = Dir2D.X * FMath::Min(NewH, HardCap);
				Velocity.Y = Dir2D.Y * FMath::Min(NewH, HardCap);
			}
		}
	}

	// ── Curve jump ───────────────────────────────────────────────────────────
	if (bUsingJumpCurve && JumpCurve && JumpCurveTotalTime > 0.f)
	{
		JumpCurveTimer    = FMath::Min(JumpCurveTimer + DeltaTime, JumpCurveTotalTime);
		const float T     = JumpCurveTimer / JumpCurveTotalTime;
		const float TNext = FMath::Min((JumpCurveTimer + DeltaTime) / JumpCurveTotalTime, 1.f);
		Velocity.Z = (JumpCurve->GetFloatValue(TNext) - JumpCurve->GetFloatValue(T)) * JumpCurvePeakHeight / DeltaTime;
		if (JumpCurveTimer >= JumpCurveTotalTime)
		{
			const float BS = 0.005f;
			ExitCurveJump();
			Velocity.Z = FMath::Min(
				(JumpCurve->GetFloatValue(1.f) - JumpCurve->GetFloatValue(1.f-BS)) * JumpCurvePeakHeight / (BS * JumpCurveTotalTime),
				-80.f);
		}
	}

	// ── Timer ticks ──────────────────────────────────────────────────────────
	if (BoostCooldown      > 0.f) BoostCooldown      = FMath::Max(0.f, BoostCooldown      - DeltaTime);
	if (DoubleJumpCooldown > 0.f) DoubleJumpCooldown  = FMath::Max(0.f, DoubleJumpCooldown - DeltaTime);
	if (OnBeatFlashTimer   > 0.f) OnBeatFlashTimer    = FMath::Max(0.f, OnBeatFlashTimer   - DeltaTime);
	if (GPComboTimer       > 0.f) { GPComboTimer -= DeltaTime; if (GPComboTimer <= 0.f) { bGPLandedRecently = false; bBoostActiveOnGPLand = false; } }
	if (bJumpInputBuffered)       { JumpInputBufferTimer -= DeltaTime; if (JumpInputBufferTimer <= 0.f) bJumpInputBuffered = false; }
	if (bJumpQueuedForBeat)       { BeatQueueTimer -= DeltaTime; if (BeatQueueTimer <= 0.f) bJumpQueuedForBeat = false; }
}

// =============================================================================
//  PHYS WALKING
// =============================================================================

void UPcQPlayerMovementComponent::PhysWalking(float deltaTime, int32 Iterations)
{
	if (deltaTime < MIN_TICK_TIME) return;

	if (BhopState == EBhopState::PowerBoost)
	{
		// Boost steering handled in Tick; just let Super resolve floor
		FVector Saved = Acceleration; Acceleration = FVector::ZeroVector;
		Super::PhysWalking(deltaTime, Iterations);
		Acceleration = Saved;
		return;
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
		Super::PhysFalling(deltaTime, Iterations); return;
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