#include "PcPlayerCharacter.h"
#include "Project_Circle/Planet/PcPlanet.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/KismetMathLibrary.h"

APcPlayerCharacter::APcPlayerCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	CameraComp = CreateDefaultSubobject<UCameraComponent>(TEXT("CameraComp"));
	CameraComp->SetupAttachment(GetCapsuleComponent());
	CameraComp->SetRelativeLocation(FVector(0, 0, 60.0f));
	CameraComp->bUsePawnControlRotation = false; 
	bUseControllerRotationYaw = false;

	if (GetCharacterMovement())
	{
		GetCharacterMovement()->GravityScale = 0.0f;
		GetCharacterMovement()->DefaultLandMovementMode = MOVE_Flying;
	}
	
	GetCapsuleComponent()->SetCollisionResponseToAllChannels(ECR_Block);
}

// ==============================================================================
// INPUT HANDLERS
// ==============================================================================

void APcPlayerCharacter::Input_Pulse()
{
	float TimeNow = GetWorld()->GetTimeSeconds();
	
	// 1. Rhythm Check
	if (TimeNow - LastPulseTime < PulseCooldown) return;
	LastPulseTime = TimeNow;
	LastActionTime = TimeNow; // Reset decay timer

	// 2. ECONOMY: Step Up
	CurrentMultiplier += PulseMultiplierGain;
	
	// Round to nearest whole number to keep tiers clean (e.g. 1.0 -> 2.0)
	CurrentMultiplier = FMath::RoundToFloat(CurrentMultiplier);
	CurrentMultiplier = FMath::Min(CurrentMultiplier, MaxMultiplier);
	HighestMultiplier = CurrentMultiplier;

	// 3. STATE TRANSITION: Enter Slide
	CurrentState = EMoveState::Sliding;
	SlideStateTimer = 0.0f; // Reset timer

	// 4. PHYSICS: The Boost
	FVector BoostDir;
	if (!CurrentInput.IsZero())
	{
		// Boost in input direction
		FVector CamFwd = CameraComp->GetForwardVector();
		FVector CamRight = CameraComp->GetRightVector();
		BoostDir = (CamFwd * CurrentInput.X + CamRight * CurrentInput.Y).GetSafeNormal();
		BoostDir = FVector::VectorPlaneProject(BoostDir, GetActorUpVector()).GetSafeNormal();
		
		// Snap velocity rotation to new direction immediately (Snappy feel)
		float CurrentSpeed = Velocity.Size();
		Velocity = BoostDir * (CurrentSpeed + PulseSpeedBoost);
	}
	else
	{
		// Boost forward if no input
		BoostDir = FVector::VectorPlaneProject(CameraComp->GetForwardVector(), GetActorUpVector()).GetSafeNormal();
		Velocity += BoostDir * PulseSpeedBoost;
	}

	// Juice
	if (PulseCameraShake)
	{
		if (APlayerController* PC = Cast<APlayerController>(GetController()))
		{
			PC->ClientStartCameraShake(PulseCameraShake);
		}
	}
}

void APcPlayerCharacter::Input_SlideHold(bool bIsHolding)
{
	bIsSlideKeyDown = bIsHolding;
}

void APcPlayerCharacter::Input_Jump()
{
	if (CurrentPlanet) 
	{ 
		VerticalStrength = 20.0f; 
		Velocity += -CurrentPlanet->GetGravityDirection(GetActorLocation()) * 800.0f; 
		CurrentState = EMoveState::Air;
	}
}

void APcPlayerCharacter::Input_Move(FVector2D Value) { CurrentInput.X = Value.X; CurrentInput.Y = Value.Y; }
void APcPlayerCharacter::Input_Look(FVector2D Value) 
{
	if (Value.X != 0.0f) AddActorLocalRotation(FRotator(0, Value.X, 0));
	if (Value.Y != 0.0f) {
		FRotator Rot = CameraComp->GetRelativeRotation();
		Rot.Pitch = FMath::Clamp(Rot.Pitch + Value.Y, -85.0f, 85.0f);
		CameraComp->SetRelativeRotation(Rot);
	}
}

// ==============================================================================
// PHYSICS LOOP
// ==============================================================================

void APcPlayerCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	FVector Location = GetActorLocation();
	FVector GravityDir = FVector(0, 0, -1);
	float Altitude = 0.0f;

	// 1. Query Planet Data
	if (CurrentPlanet)
	{
		GravityDir = CurrentPlanet->GetGravityDirection(Location);
		Altitude = CurrentPlanet->GetAltitude(Location);
	}
	FVector TargetUp = -GravityDir;

	// 2. Orientation
	FQuat CurrentRot = GetActorQuat();
	FQuat TargetRot = FQuat::FindBetweenNormals(GetActorUpVector(), TargetUp) * CurrentRot;
	SetActorRotation(FQuat::Slerp(CurrentRot, TargetRot, 15.0f * DeltaTime));

	// 3. Rail / Vertical Logic
	VerticalStrength = FMath::FInterpTo(VerticalStrength, 0.0f, DeltaTime, StrengthDecay);
	
	bool bRailCheck = false;
	if (CurrentPlanet) bRailCheck = (FMath::Abs(Altitude) < 100.0f);
	else 
	{
		FHitResult H; FCollisionQueryParams P; P.AddIgnoredActor(this);
		bRailCheck = GetWorld()->LineTraceSingleByChannel(H, Location, Location + (GravityDir * 150.0f), ECC_WorldStatic, P);
	}

	bool bIsSnapped = (VerticalStrength < 5.0f) && bRailCheck;

	FVector VerticalVel = (Velocity | GravityDir) * GravityDir;
	FVector HorizontalVel = Velocity - VerticalVel;

	if (bIsSnapped)
	{
		bIsGrounded = true;
		if (CurrentState == EMoveState::Air) CurrentState = EMoveState::Cruising; // Landed

		if (CurrentPlanet)
		{
			FVector RadialDir = (Location - CurrentPlanet->GetActorLocation()).GetSafeNormal();
			FVector SurfacePoint = CurrentPlanet->GetActorLocation() + (RadialDir * CurrentPlanet->SurfaceRadius);
			SetActorLocation(FMath::VInterpTo(Location, SurfacePoint, DeltaTime, 20.0f), true);
			VerticalVel = FVector::ZeroVector;
		}
		else
		{
			Velocity += GravityDir * 500.0f * DeltaTime;
		}
	}
	else
	{
		bIsGrounded = false;
		CurrentState = EMoveState::Air;
		float TetherForce = (Altitude > 0) ? 980.0f : -2000.0f; 
		if (VerticalStrength > 5.0f) TetherForce *= 0.5f;
		VerticalVel += GravityDir * TetherForce * DeltaTime;
	}

	// 4. Horizontal Movement (Slide vs Cruise)
	Velocity = HorizontalVel + VerticalVel; 
	UpdateMovementPhysics(DeltaTime, TargetUp);

	// 5. Move & Slide
	FHitResult MoveHit;
	AddActorWorldOffset(Velocity * DeltaTime, true, &MoveHit);

	if (MoveHit.IsValidBlockingHit())
	{
		Velocity = SlideAlongSurface(Velocity, MoveHit.Normal);
		if (MoveHit.PenetrationDepth > 0.0f) AddActorWorldOffset(MoveHit.Normal * MoveHit.PenetrationDepth);
	}
	
	// --- JUICE (FOV & TILT) ---
	float SpeedRatio = Velocity.Size() / (BaseSpeed * MaxMultiplier);
	SpeedRatio = FMath::Clamp(SpeedRatio, 0.0f, 1.0f);

	float TargetFOV = FMath::Lerp(BaseFOV, SpeedFOV, SpeedRatio);
	CameraComp->SetFieldOfView(FMath::FInterpTo(CameraComp->FieldOfView, TargetFOV, DeltaTime, 5.0f));

	float TargetTilt = (!CurrentInput.IsZero()) ? CurrentInput.Y * MaxCameraTilt : 0.0f;
	FRotator RelativeRot = CameraComp->GetRelativeRotation();
	RelativeRot.Roll = FMath::FInterpTo(RelativeRot.Roll, TargetTilt, DeltaTime, 10.0f);
	CameraComp->SetRelativeRotation(RelativeRot);
}

void APcPlayerCharacter::UpdateMovementPhysics(float DeltaTime, FVector SurfaceNormal)
{
	// Separate Horizontal Component
	FVector VerticalVel = (Velocity | SurfaceNormal) * SurfaceNormal;
	FVector HorizontalVel = Velocity - VerticalVel;
	float CurrentSpeed = HorizontalVel.Size();

	float TargetFriction = Friction;
	float TargetSteeringAlpha = 0.0f; // 0 = Heavy, 1 = Snappy

	// --- STATE MACHINE LOGIC ---
	if (CurrentState == EMoveState::Sliding)
	{
		UpdateSlideLogic(DeltaTime, TargetFriction, TargetSteeringAlpha);
	}
	else if (CurrentState == EMoveState::Cruising)
	{
		UpdateCruisingLogic(DeltaTime, TargetFriction, TargetSteeringAlpha);
	}

	// --- APPLY STEERING ---
	float FinalSteeringRate = FMath::Lerp(MinSteeringRate, MaxSteeringRate, TargetSteeringAlpha);

	// Determine Input Direction Projected on Surface
	FVector CamFwd = CameraComp->GetForwardVector();
	FVector CamRight = CameraComp->GetRightVector();
	FVector InputDir = (CamFwd * CurrentInput.X + CamRight * CurrentInput.Y).GetSafeNormal();
	InputDir = FVector::VectorPlaneProject(InputDir, SurfaceNormal).GetSafeNormal();

	if (!InputDir.IsZero() && CurrentSpeed > 10.0f)
	{
		FVector CurrentDir = HorizontalVel.GetSafeNormal();
		FVector NewDir = FMath::VInterpNormalRotationTo(CurrentDir, InputDir, DeltaTime, FinalSteeringRate * 10.0f);
		HorizontalVel = NewDir * CurrentSpeed;
	}

	// --- APPLY ACCEL & FRICTION ---
	float SpeedCap = BaseSpeed * CurrentMultiplier;

	// Acceleration (Only if holding input and under cap)
	if (!CurrentInput.IsZero() && CurrentSpeed < SpeedCap)
	{
		// Kickstart if stationary
		if (HorizontalVel.IsZero()) HorizontalVel = InputDir * 100.0f;
		HorizontalVel += InputDir * Acceleration * DeltaTime;
	}
	
	// Friction / Drag
	// We apply friction if: We are overspeeding OR we are not inputting (drag to stop)
	// BUT: If Sliding (Phase 1), Friction is 0, so this multiplier becomes 1.0 (No drag)
	float DragFactor = FMath::Max(0.0f, 1.0f - (TargetFriction * DeltaTime));
	HorizontalVel *= DragFactor;
	
	// Reassemble
	Velocity = HorizontalVel + VerticalVel;
}

void APcPlayerCharacter::UpdateSlideLogic(float DeltaTime, float& OutFriction, float& OutSteeringAlpha)
{
	SlideStateTimer += DeltaTime;

	// Reset Decay Timer while sliding
	LastActionTime = GetWorld()->GetTimeSeconds();

	// Check Exit Conditions (Time expired OR Key Released)
	float TotalSlideTime = SlideHydroplaneTime + SlideFadeTime;
	if (SlideStateTimer >= TotalSlideTime || !bIsSlideKeyDown)
	{
		CurrentState = EMoveState::Cruising;
		// Force physics back to standard immediately if exited early
		UpdateCruisingLogic(DeltaTime, OutFriction, OutSteeringAlpha); 
		return;
	}

	// --- PHASE 1: HYDROPLANE ---
	if (SlideStateTimer <= SlideHydroplaneTime)
	{
		OutFriction = 0.0f;         // Infinite glide
		OutSteeringAlpha = 1.0f;    // Maximum control
		
		// Passive Growth
		CurrentMultiplier += SlidePassiveGrowth * DeltaTime;
		CurrentMultiplier = FMath::Min(CurrentMultiplier, MaxMultiplier);
		HighestMultiplier = CurrentMultiplier;
	}
	// --- PHASE 2: THE FADE ---
	else
	{
		float FadeAlpha = (SlideStateTimer - SlideHydroplaneTime) / SlideFadeTime;
		
		// Blend Friction back to normal
		OutFriction = FMath::Lerp(0.0f, Friction, FadeAlpha);
		
		// Blend Control back to normal (based on current multiplier)
		float NormalControl = FMath::GetMappedRangeValueClamped(FVector2D(1.0f, MaxMultiplier), FVector2D(0.0f, 1.0f), CurrentMultiplier);
		OutSteeringAlpha = FMath::Lerp(1.0f, NormalControl, FadeAlpha);
	}
}

void APcPlayerCharacter::UpdateCruisingLogic(float DeltaTime, float& OutFriction, float& OutSteeringAlpha)
{
	// 1. Calculate Steering Feel (Reverse Inertia)
	// 1.0x = Heavy (0.0), MaxMult = Snappy (1.0)
	OutSteeringAlpha = FMath::GetMappedRangeValueClamped(FVector2D(1.0f, MaxMultiplier), FVector2D(0.0f, 1.0f), CurrentMultiplier);
	OutFriction = Friction;

	// 2. Economy Decay
	float TimeSinceAction = GetWorld()->GetTimeSeconds() - LastActionTime;
	bool bHasInput = !CurrentInput.IsZero();

	// Only decay if we are past the grace period
	if (TimeSinceAction > PulseGracePeriod)
	{
		// Determine how many tiers we have dropped
		float Overtime = TimeSinceAction - PulseGracePeriod;
		
		// We don't drop linearly, we target specific steps below the Peak
		// Example: Peak 3.0. 1 sec later -> Target 2.0.
		// However, if we are walking (HasInput), we don't drop below 1.0
		float FloorMult = bHasInput ? 1.0f : 0.0f;
		
		// Calculate steps dropped
		int32 Steps = FMath::FloorToInt(Overtime); // 1 step per second past grace
		float TargetTier = HighestMultiplier - (Steps * DropStepAmount);
		TargetTier = FMath::Max(TargetTier, FloorMult);

		// Interp down
		CurrentMultiplier = FMath::FInterpTo(CurrentMultiplier, TargetTier, DeltaTime, PulseDropSpeed);
	}
	else if (bHasInput && CurrentMultiplier < 1.0f)
	{
		// Recovery (Walking startup)
		CurrentMultiplier += 2.0f * DeltaTime;
	}

	CurrentMultiplier = FMath::Clamp(CurrentMultiplier, 0.0f, MaxMultiplier);
}

FVector APcPlayerCharacter::SlideAlongSurface(const FVector& InVel, const FVector& Normal) 
{ 
	return InVel - Normal * (InVel | Normal); 
}

void APcPlayerCharacter::NotifyActorBeginOverlap(AActor* Other) { if (APcPlanet* P = Cast<APcPlanet>(Other)) CurrentPlanet = P; }
void APcPlayerCharacter::NotifyActorEndOverlap(AActor* Other) { if (Other == CurrentPlanet) CurrentPlanet = nullptr; }

FString APcPlayerCharacter::GetDebugInfo() const 
{ 
	FString StateName = (CurrentState == EMoveState::Sliding) ? "SLIDING" : "CRUISING";
	return FString::Printf(TEXT("[%s]\nMult: %.2f (Peak: %.1f)\nSpeed: %.0f\nSlide: %.1fs"), 
		*StateName, CurrentMultiplier, HighestMultiplier, Velocity.Size(), SlideStateTimer); 
}