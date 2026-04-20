#include "PcQPlayerMovementComponent.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"

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

// =============================================================================
//  BEAT FLASH
// =============================================================================

void UPcQPlayerMovementComponent::TriggerOnBeatFlash()
{
	OnBeatFlashTimer = OnBeatFlashDuration;
	OnActiveBeatAction.Broadcast();
}

// =============================================================================
//  INPUT
// =============================================================================

void UPcQPlayerMovementComponent::OnJumpPressed()
{
	if (MoveState == EPlayerMoveState::WallSwim)
	{
		// Jump while in a wall → manual eject, no beat required
		EjectFromWall(false);
		return;
	}

	if (MoveState == EPlayerMoveState::Sliding)
	{
		// Jump cancels slide and launches normally
		ExitSlide();
	}

	if (MoveState == EPlayerMoveState::GroundPounding) return;

	if (IsMovingOnGround())
	{
		ApplyJumpArc(JumpPeakHeightCM, JumpAirTimeSec);
		GPBufferTimer = GPBufferSec;  // start the GP lockout window
	}
}

void UPcQPlayerMovementComponent::OnJumpReleased() { /* no-op */ }

void UPcQPlayerMovementComponent::OnGroundPoundPressed()
{
	if (MoveState == EPlayerMoveState::WallSwim)    return;
	if (MoveState == EPlayerMoveState::GroundPounding) return;

	if (IsMovingOnGround())
	{
		// Ground GP: same as slide input, but with a strong initial boost
		EnterSlide(ESlideStrength::Strong, StrongSlideMinSpeed);
		TriggerOnBeatFlash();
		return;
	}

	// Air GP — only allowed after buffer expires
	if (GPBufferTimer > 0.f)
	{
		UE_LOG(LogTemp, Log, TEXT("[GP] Blocked by buffer (%.2fs remaining)"), GPBufferTimer);
		return;
	}

	if (IsFalling())
	{
		ExitCurveJump();
		MoveState  = EPlayerMoveState::GroundPounding;
		Velocity.X = 0.f; Velocity.Y = 0.f; Velocity.Z = GPSlamSpeed;
	}
}

void UPcQPlayerMovementComponent::OnSlidePressed()
{
	bSlideHeld = true;

	if (MoveState == EPlayerMoveState::WallSwim)    return;
	if (MoveState == EPlayerMoveState::GroundPounding) return;

	if (IsMovingOnGround() && MoveState != EPlayerMoveState::Sliding)
	{
		// Simple ground slide: weak, just preserves current momentum
		EnterSlide(ESlideStrength::Weak, GetHorizontalSpeed());
	}
}

void UPcQPlayerMovementComponent::OnSlideReleased()
{
	bSlideHeld = false;

	// Releasing slide exits the slide early (player chose to stand up)
	if (MoveState == EPlayerMoveState::Sliding)
		ExitSlide();
}

// =============================================================================
//  SLIDE
// =============================================================================

void UPcQPlayerMovementComponent::EnterSlide(ESlideStrength Strength, float EntrySpeed)
{
	MoveState            = EPlayerMoveState::Sliding;
	CurrentSlideStrength = Strength;

	// Set slide velocity in WASD direction (or forward if no input)
	FVector Dir = Acceleration.GetSafeNormal2D();
	if (Dir.IsZero() && CharacterOwner)
		Dir = CharacterOwner->GetActorForwardVector().GetSafeNormal2D();

	const float Speed  = Strength == ESlideStrength::Strong
	                   ? FMath::Max(EntrySpeed, StrongSlideMinSpeed)
	                   : FMath::Min(EntrySpeed, WeakSlideMaxSpeed);

	Velocity.X = Dir.X * Speed;
	Velocity.Y = Dir.Y * Speed;
	Velocity.Z = 0.f;

	UE_LOG(LogTemp, Log, TEXT("[Slide] Entered %s. Speed=%.0f"),
	       Strength == ESlideStrength::Strong ? TEXT("STRONG") : TEXT("WEAK"), Speed);
}

void UPcQPlayerMovementComponent::ExitSlide()
{
	if (MoveState != EPlayerMoveState::Sliding) return;
	MoveState = EPlayerMoveState::Normal;
	UE_LOG(LogTemp, Log, TEXT("[Slide] Exited."));
}

float UPcQPlayerMovementComponent::GetCurrentSlideDecay() const
{
	return CurrentSlideStrength == ESlideStrength::Strong ? StrongSlideDecay : WeakSlideDecay;
}

// =============================================================================
//  JUMP ARC
//
//  Fixed peak height and air time — completely independent of the beat.
//  The pulsing floor uses the same function but passes its own values.
// =============================================================================

void UPcQPlayerMovementComponent::ApplyJumpArc(float PeakHeightCM, float AirTimeSec)
{
	if (CharacterOwner) CharacterOwner->JumpCurrentCount = 0;

	const float TimeToPeak = AirTimeSec * 0.5f;

	if (JumpCurve)
	{
		bUsingJumpCurve     = true;
		JumpCurveTimer      = 0.f;
		JumpCurveTotalTime  = AirTimeSec;
		JumpCurvePeakHeight = PeakHeightCM;
		GravityScale        = 0.f;
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
//  FLOOR PULSE CHECK
//
//  Called every tick while grounded.  Samples the actor beneath the player
//  and checks if it has a UPcBeatSurfaceComponent with WantsToLaunch().
//  If yes: consume the pulse, launch.
//
//  The player doesn't subscribe to anything.  The surface sets a flag and
//  the player reads it when they're ready.  No timing mismatch possible.
// =============================================================================

void UPcQPlayerMovementComponent::CheckFloorPulse()
{
	// Must be grounded and not already doing something beat-triggered
	if (!IsMovingOnGround()) return;
	if (MoveState == EPlayerMoveState::GroundPounding) return;

	// Don't launch the player out of a slide — let them keep sliding.
	// The beat surface can still boost their slide speed though (Boost type).
	const AActor* FloorActor = CurrentFloor.HitResult.GetActor();
	if (!FloorActor) return;

	UPcBeatSurfaceComponent* Surface = FloorActor->FindComponentByClass<UPcBeatSurfaceComponent>();
	if (!Surface || !Surface->WantsToLaunch()) return;

	Surface->ConsumePulse();

	if (Surface->SurfaceType == EPcBeatSurfaceType::Floor)
	{
		if (MoveState == EPlayerMoveState::Sliding)
		{
			// Beat fires under a sliding player: boost the slide instead of launching
			const FVector Dir2D = FVector(Velocity.X, Velocity.Y, 0.f).GetSafeNormal();
			const float Boost   = Surface->BoostSpeed > 0.f ? Surface->BoostSpeed : StrongSlideMinSpeed;
			Velocity.X = Dir2D.X * FMath::Max(GetHorizontalSpeed(), Boost);
			Velocity.Y = Dir2D.Y * FMath::Max(GetHorizontalSpeed(), Boost);
			TriggerOnBeatFlash();
			UE_LOG(LogTemp, Log, TEXT("[BeatSurface] Floor boost → slide."));
			return;
		}

		// Normal launch
		ExitSlide();
		ExitCurveJump();

		const float LaunchZ    = Surface->FloorLaunchOverride > 0.f
		                       ? Surface->FloorLaunchOverride : JumpZVelocity;
		const float PeakHeight = Surface->FloorLaunchOverride > 0.f
		                       ? 0.f : JumpPeakHeightCM; // 0 → use LaunchZ directly

		if (PeakHeight > 0.f)
			ApplyJumpArc(PeakHeight, JumpAirTimeSec);
		else
		{
			Velocity.Z   = LaunchZ;
			GravityScale = 1.f;
			SetMovementMode(MOVE_Falling);
		}

		TriggerOnBeatFlash();
		OnBhopLanded.Broadcast(GetHorizontalSpeed());
		UE_LOG(LogTemp, Log, TEXT("[BeatSurface] Floor launched player."));
	}
	else if (Surface->SurfaceType == EPcBeatSurfaceType::Boost && MoveState == EPlayerMoveState::Sliding)
	{
		const FVector Dir2D = FVector(Velocity.X, Velocity.Y, 0.f).GetSafeNormal();
		Velocity.X = Dir2D.X * (GetHorizontalSpeed() + Surface->BoostSpeed);
		Velocity.Y = Dir2D.Y * (GetHorizontalSpeed() + Surface->BoostSpeed);
		TriggerOnBeatFlash();
	}
}

// =============================================================================
//  WALL SWIM
// =============================================================================

void UPcQPlayerMovementComponent::EnterWallSwim(const FHitResult& Hit)
{
	if (MoveState == EPlayerMoveState::WallSwim) return;

	ExitCurveJump();
	ExitSlide();
	MoveState       = EPlayerMoveState::WallSwim;
	SwimEntryNormal = Hit.ImpactNormal;
	Velocity       *= WallSwim_EntryVelocityRetain;

	// Cache the beat surface if the wall has one — used for beat-eject
	SwimWallSurface = nullptr;
	if (AActor* WallActor = Hit.GetActor())
		SwimWallSurface = WallActor->FindComponentByClass<UPcBeatSurfaceComponent>();

	if (UCapsuleComponent* Cap = CharacterOwner ? CharacterOwner->GetCapsuleComponent() : nullptr)
	{
		SwimPrevCollisionProfile = Cap->GetCollisionProfileName();
		Cap->SetCollisionProfileName(TEXT("NoCollision"));
	}

	SetMovementMode(MOVE_Flying);
	OnWallSwimChanged.Broadcast(1.f);
	UE_LOG(LogTemp, Log, TEXT("[WallSwim] Entered. HasBeatSurface=%d"),
	       SwimWallSurface.IsValid() ? 1 : 0);
}

void UPcQPlayerMovementComponent::EjectFromWall(bool bBeatEject)
{
	if (UCapsuleComponent* Cap = CharacterOwner ? CharacterOwner->GetCapsuleComponent() : nullptr)
		Cap->SetCollisionProfileName(SwimPrevCollisionProfile.IsNone()
		                             ? FName(TEXT("Pawn")) : SwimPrevCollisionProfile);

	MoveState = EPlayerMoveState::Normal;

	FVector EjectDir = Acceleration.GetSafeNormal2D();
	if (EjectDir.IsNearlyZero()) EjectDir = SwimEntryNormal;

	if (bBeatEject && SwimWallSurface.IsValid())
	{
		// Beat eject: strong, horizontal-biased, uses surface parameters
		const UPcBeatSurfaceComponent* Surf = SwimWallSurface.Get();
		Velocity.X   = EjectDir.X * Surf->WallEjectSpeed;
		Velocity.Y   = EjectDir.Y * Surf->WallEjectSpeed;
		Velocity.Z   = Surf->WallEjectUpKick;
		TriggerOnBeatFlash();
		UE_LOG(LogTemp, Log, TEXT("[WallSwim] Beat eject. Speed=%.0f"), GetHorizontalSpeed());
	}
	else
	{
		// Manual eject (Jump pressed): normal jump speed, let arc take over
		Velocity.X   = EjectDir.X * WallSwim_ManualEjectSpeed;
		Velocity.Y   = EjectDir.Y * WallSwim_ManualEjectSpeed;
		Velocity.Z   = WallSwim_ManualEjectUpKick;
		UE_LOG(LogTemp, Log, TEXT("[WallSwim] Manual eject."));
	}

	GravityScale = 1.f;
	SetMovementMode(MOVE_Falling);
	SwimWallSurface = nullptr;
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
//  HANDLE IMPACT — wall swim entry point
// =============================================================================

void UPcQPlayerMovementComponent::HandleImpact(const FHitResult& Hit, float TimeSlice,
                                                const FVector& MoveDelta)
{
	if (MovementMode == MOVE_Falling
	    && MoveState != EPlayerMoveState::WallSwim
	    && MoveState != EPlayerMoveState::GroundPounding)
	{
		// Only enter swim if the wall has a UPcBeatSurfaceComponent of Wall type
		// OR if we want all steep walls to be swimmable (comment out the Surface check).
		const float WallDot = FMath::Abs(Hit.ImpactNormal.Z);
		if (WallDot < 0.4f && GetHorizontalSpeed() >= WallSwim_EnterMinSpeed)
		{
			// Check for beat surface — only walls tagged as beat walls trigger swim
			UPcBeatSurfaceComponent* Surface = Hit.GetActor()
			    ? Hit.GetActor()->FindComponentByClass<UPcBeatSurfaceComponent>() : nullptr;

			if (Surface && Surface->SurfaceType == EPcBeatSurfaceType::Wall)
			{
				EnterWallSwim(Hit);
				return;
			}
		}
	}

	Super::HandleImpact(Hit, TimeSlice, MoveDelta);
}

// =============================================================================
//  PROCESS LANDED
// =============================================================================

void UPcQPlayerMovementComponent::ProcessLanded(const FHitResult& Hit, float remainingTime,
                                                 int32 Iterations)
{
	// Landed while ghosted (fell through thin floor)
	if (MoveState == EPlayerMoveState::WallSwim)
	{
		if (UCapsuleComponent* Cap = CharacterOwner ? CharacterOwner->GetCapsuleComponent() : nullptr)
			Cap->SetCollisionProfileName(SwimPrevCollisionProfile.IsNone()
			                            ? FName(TEXT("Pawn")) : SwimPrevCollisionProfile);
		MoveState       = EPlayerMoveState::Normal;
		SwimWallSurface = nullptr;
		OnWallSwimChanged.Broadcast(0.f);
		Super::ProcessLanded(Hit, remainingTime, Iterations);
		return;
	}

	if (MoveState == EPlayerMoveState::GroundPounding)
	{
		// GP landing: slide if slide held, otherwise brief freeze
		ExitCurveJump();
		MoveState = EPlayerMoveState::Normal;

		Super::ProcessLanded(Hit, remainingTime, Iterations);

		if (bSlideHeld)
		{
			// Slide input was held → strong slide immediately, no freeze
			const float ImpactSpeed = FMath::Abs(Velocity.Z);  // use the fall speed
			EnterSlide(ESlideStrength::Strong, FMath::Max(ImpactSpeed * 0.6f, StrongSlideMinSpeed));
		}
		else
		{
			// No slide input → brief freeze ("thud" feel)
			bGPImpactPending = true;
			GPFreezeTimer    = GPImpactFreezeSec;
			Velocity.X = 0.f; Velocity.Y = 0.f; Velocity.Z = 0.f;
		}
		return;
	}

	ExitCurveJump();
	Super::ProcessLanded(Hit, remainingTime, Iterations);
}

// =============================================================================
//  TICK
// =============================================================================

void UPcQPlayerMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                                 FActorComponentTickFunction* ThisTickFunction)
{
	// ── GP freeze ────────────────────────────────────────────────────────────
	if (GPFreezeTimer > 0.f)
	{
		GPFreezeTimer -= DeltaTime;
		if (GPFreezeTimer <= 0.f)
		{
			GPFreezeTimer    = 0.f;
			bGPImpactPending = false;
		}
		else
		{
			// While frozen: zero velocity, skip normal physics
			Velocity = FVector::ZeroVector;
			// Call Super but return immediately after so we skip everything else
			Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
			return;
		}
	}

	// ── GP buffer countdown ───────────────────────────────────────────────────
	if (GPBufferTimer > 0.f) GPBufferTimer -= DeltaTime;

	// ── Wall swim physics ─────────────────────────────────────────────────────
	if (MoveState == EPlayerMoveState::WallSwim)
	{
		const float HR = FMath::Pow(1.f - FMath::Clamp(WallSwim_Damping   * DeltaTime, 0.f, 0.99f), 1.f);
		const float UR = FMath::Pow(1.f - FMath::Clamp(WallSwim_UpDamping * DeltaTime, 0.f, 0.99f), 1.f);
		Velocity.X *= HR; Velocity.Y *= HR;
		Velocity.Z  = Velocity.Z < 0.f ? Velocity.Z * HR : Velocity.Z * UR;
		if (!IsAboveGround() && Velocity.Z < 0.f) Velocity.Z = 0.f;

		// Check if the wall surface we're in wants to eject us
		if (SwimWallSurface.IsValid() && SwimWallSurface->WantsToLaunch())
		{
			SwimWallSurface->ConsumePulse();
			EjectFromWall(true);  // beat eject
		}
	}

	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ── Floor pulse check ─────────────────────────────────────────────────────
	// Runs AFTER Super so CurrentFloor is valid for this frame.
	CheckFloorPulse();

	// ── Beat flash decay ──────────────────────────────────────────────────────
	if (OnBeatFlashTimer > 0.f)
		OnBeatFlashTimer = FMath::Max(0.f, OnBeatFlashTimer - DeltaTime);

	// ── Curve jump tick ───────────────────────────────────────────────────────
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
}

// =============================================================================
//  PHYS WALKING
// =============================================================================

void UPcQPlayerMovementComponent::PhysWalking(float deltaTime, int32 Iterations)
{
	if (deltaTime < MIN_TICK_TIME) return;

	// Slide deceleration
	if (MoveState == EPlayerMoveState::Sliding)
	{
		const float CurrentH = GetHorizontalSpeed();
		if (CurrentH < SlideExitSpeed)
		{
			ExitSlide();  // too slow — stand up
		}
		else
		{
			// Apply decay toward exit speed
			const float NewH    = FMath::FInterpTo(CurrentH, SlideExitSpeed, deltaTime, GetCurrentSlideDecay());
			const FVector Dir2D = FVector(Velocity.X, Velocity.Y, 0.f).GetSafeNormal();
			Velocity.X = Dir2D.X * NewH;
			Velocity.Y = Dir2D.Y * NewH;
		}
	}

	const FVector WishDir   = Acceleration.GetSafeNormal2D();
	FVector       Vel2D(Velocity.X, Velocity.Y, 0.f);
	const float   Spd       = Vel2D.Size();
	const float   EffTarget = (Spd > MaxWalkSpeed && !WishDir.IsZero()) ? Spd : MaxWalkSpeed;

	// During slide, let the slide physics drive velocity — don't fight it with normal accel
	if (MoveState != EPlayerMoveState::Sliding)
	{
		const FVector NewVel2D = FMath::VInterpTo(Vel2D, WishDir * EffTarget, deltaTime,
		                         WishDir.IsZero() ? CustomGroundFriction : CustomGroundAcceleration);
		Velocity.X = NewVel2D.X; Velocity.Y = NewVel2D.Y;
	}

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

	if (MoveState == EPlayerMoveState::GroundPounding)
	{
		Acceleration = FVector::ZeroVector; Velocity.X = 0.f; Velocity.Y = 0.f;
		Super::PhysFalling(deltaTime, Iterations);
		return;
	}

	const FVector WishDir  = Acceleration.GetSafeNormal2D();
	const FVector Vel2D(Velocity.X, Velocity.Y, 0.f);
	const FVector NewVel2D = FMath::VInterpTo(Vel2D, WishDir * MaxWalkSpeed, deltaTime,
	                         WishDir.IsZero() ? CustomAirFriction : CustomAirAcceleration);
	Velocity.X = NewVel2D.X; Velocity.Y = NewVel2D.Y;

	FVector Saved = Acceleration; Acceleration = FVector::ZeroVector;
	Super::PhysFalling(deltaTime, Iterations);
	Acceleration = Saved;
}