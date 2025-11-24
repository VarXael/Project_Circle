#include "PcPlayerCharacter.h"
#include "Project_Circle/Planet/PcPlanet.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/KismetMathLibrary.h"

APcPlayerCharacter::APcPlayerCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	// Initialize Components
	CameraComp = CreateDefaultSubobject<UCameraComponent>(TEXT("CameraComp"));
	CameraComp->SetupAttachment(GetCapsuleComponent());
	CameraComp->SetRelativeLocation(FVector(0, 0, 60.0f));
	CameraComp->bUsePawnControlRotation = false; 
	bUseControllerRotationYaw = false;

	// Disable default Unreal physics (gravity/friction) as we handle it manually
	if (GetCharacterMovement())
	{
		GetCharacterMovement()->GravityScale = 0.0f;
		GetCharacterMovement()->DefaultLandMovementMode = MOVE_Flying;
	}
	
	// Ensure capsule blocks pawns but overlaps static geometry for the sinking mechanic
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

	// 2. BOOST & SNAP (The Fix)
	
	// Add the gain first (e.g., 1.29 + 0.5 = 1.79)
	float RawNewMultiplier = CurrentMultiplier + PulseMultiplierGain;

	// Snap to nearest step size (PulseMultiplierGain is 0.5)
	// Math: Round(1.79 / 0.5) -> Round(3.58) -> 4.0
	//       4.0 * 0.5 = 2.0. (Clean Tier)
	float SnappedMultiplier = FMath::RoundToFloat(RawNewMultiplier / PulseMultiplierGain) * PulseMultiplierGain;

	// Apply & Cap
	CurrentMultiplier = FMath::Min(SnappedMultiplier, MaxMultiplier);

	// Save Peak for the decay logic
	HighestMultiplier = CurrentMultiplier; 

	// 3. PHYSICS KICK
	if (!CurrentInput.IsZero())
	{
		FVector CamFwd = CameraComp->GetForwardVector();
		FVector CamRight = CameraComp->GetRightVector();
		FVector InputDir = (CamFwd * CurrentInput.X + CamRight * CurrentInput.Y).GetSafeNormal();
		InputDir = FVector::VectorPlaneProject(InputDir, GetActorUpVector()).GetSafeNormal();
		
		// Calculate Snap Turn Speed
		float CurrentSpeed = Velocity.Size();
		if (CurrentSpeed < 100.0f) CurrentSpeed = 0.0f;

		// Rotate Velocity Instantly
		Velocity = InputDir * (CurrentSpeed + PulseSpeedBoost);
	}
	else
	{
		// No Input Boost
		FVector Fwd = FVector::VectorPlaneProject(CameraComp->GetForwardVector(), GetActorUpVector());
		Velocity += Fwd.GetSafeNormal() * PulseSpeedBoost;
	}
	
	if (PulseCameraShake)
	{
		if (APlayerController* PC = Cast<APlayerController>(GetController()))
		{
			PC->ClientStartCameraShake(PulseCameraShake);
		}
	}
}

void APcPlayerCharacter::Input_Jump()
{
	// Only jump if we have a planet reference to know where "Up" is
	if (CurrentPlanet) 
	{ 
		// Break the rail lock
		VerticalStrength = 20.0f; 
		
		// Apply jump force
		Velocity += -CurrentPlanet->GetGravityDirection(GetActorLocation()) * 800.0f; 
	}
}

void APcPlayerCharacter::Input_Move(FVector2D Value) 
{ 
	CurrentInput.X = Value.X; 
	CurrentInput.Y = Value.Y; 
}

void APcPlayerCharacter::Input_Look(FVector2D Value) 
{
	if (Value.X != 0.0f) 
	{
		AddActorLocalRotation(FRotator(0, Value.X, 0));
	}
	
	if (Value.Y != 0.0f) 
	{
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

	// 2. Update Orientation
	FQuat CurrentRot = GetActorQuat();
	FQuat TargetRot = FQuat::FindBetweenNormals(GetActorUpVector(), TargetUp) * CurrentRot;
	SetActorRotation(FQuat::Slerp(CurrentRot, TargetRot, 15.0f * DeltaTime));

	// 3. Rail / Tether Physics
	// Decay strength over time so the player eventually re-locks to the surface
	VerticalStrength = FMath::FInterpTo(VerticalStrength, 0.0f, DeltaTime, StrengthDecay);
	
	// Fallback raycast if no planet is defined (Flat ground testing)
	bool bRailCheck = false;
	if (CurrentPlanet) 
	{
		bRailCheck = (FMath::Abs(Altitude) < 100.0f);
	}
	else 
	{
		FHitResult H; 
		FCollisionQueryParams P; 
		P.AddIgnoredActor(this);
		bRailCheck = GetWorld()->LineTraceSingleByChannel(H, Location, Location + (GravityDir * 150.0f), ECC_WorldStatic, P);
	}

	// Determine if we should snap to the rail
	bool bIsSnapped = (VerticalStrength < 5.0f) && bRailCheck;

	FVector VerticalVel = (Velocity | GravityDir) * GravityDir;
	FVector HorizontalVel = Velocity - VerticalVel;

	if (bIsSnapped)
	{
		bIsGrounded = true;
		
		if (CurrentPlanet)
		{
			// Geometric Snap: Force position to the planet surface radius
			FVector RadialDir = (Location - CurrentPlanet->GetActorLocation()).GetSafeNormal();
			FVector SurfacePoint = CurrentPlanet->GetActorLocation() + (RadialDir * CurrentPlanet->SurfaceRadius);
			
			// Hard interp to lock position
			SetActorLocation(FMath::VInterpTo(Location, SurfacePoint, DeltaTime, 20.0f), true);
			
			// Clear vertical momentum to prevent bouncing
			VerticalVel = FVector::ZeroVector;
		}
		else
		{
			// Flat ground stick force
			Velocity += GravityDir * 500.0f * DeltaTime;
		}
	}
	else
	{
		bIsGrounded = false;
		
		// Apply Gravity / Tether Force
		// If altitude is positive, pull down. If negative (underground), pull up.
		float TetherForce = (Altitude > 0) ? 980.0f : -2000.0f; 
		
		// If we have high vertical strength (Jump), reduce gravity influence
		if (VerticalStrength > 5.0f) 
		{
			TetherForce *= 0.5f;
		}
		
		VerticalVel += GravityDir * TetherForce * DeltaTime;
	}

	// 4. Update Horizontal Movement (Steering & Multiplier)
	Velocity = HorizontalVel + VerticalVel; // Reassemble for calculation
	UpdateMovementPhysics(DeltaTime, TargetUp);

	// 5. Execute Move
	FHitResult MoveHit;
	AddActorWorldOffset(Velocity * DeltaTime, true, &MoveHit);

	// Handle blocking collisions (Walls)
	if (MoveHit.IsValidBlockingHit())
	{
		Velocity = SlideAlongSurface(Velocity, MoveHit.Normal);
		if (MoveHit.PenetrationDepth > 0.0f) 
		{
			AddActorWorldOffset(MoveHit.Normal * MoveHit.PenetrationDepth);
		}
	}
	
	// --- JUICE LOGIC (FOV & TILT) ---

	// 1. DYNAMIC FOV (Speed Warp)
	// Calculate ratio: 0.0 = Stopped, 1.0 = Max Possible Speed
	float SpeedRatio = Velocity.Size() / (BaseSpeed * MaxMultiplier);
	SpeedRatio = FMath::Clamp(SpeedRatio, 0.0f, 1.0f);

	float TargetFOV = FMath::Lerp(BaseFOV, SpeedFOV, SpeedRatio);
    
	// Smoothly interpolate FOV
	float CurrentFOV = CameraComp->FieldOfView;
	CameraComp->SetFieldOfView(FMath::FInterpTo(CurrentFOV, TargetFOV, DeltaTime, 5.0f));


	// 2. CAMERA TILT (Banking)
	// We tilt based on the "Right" input (CurrentInput.Y) or lateral velocity
	float TargetTilt = 0.0f;

	// If pressing keys, tilt into the turn
	if (!CurrentInput.IsZero())
	{
		TargetTilt = CurrentInput.Y * MaxCameraTilt; // Y is Left/Right (-1 to 1)
	}

	// Get current rotation relative to the capsule
	FRotator RelativeRot = CameraComp->GetRelativeRotation();
    
	// Smoothly interp the Roll (Tilt)
	RelativeRot.Roll = FMath::FInterpTo(RelativeRot.Roll, TargetTilt, DeltaTime, 10.0f);
    
	CameraComp->SetRelativeRotation(RelativeRot);
}

void APcPlayerCharacter::UpdateMovementPhysics(float DeltaTime, FVector SurfaceNormal)
{
	// --- A. MULTIPLIER TIERED DECAY ---
	
	bool bHasInput = !CurrentInput.IsZero();
	float TimeSincePulse = GetWorld()->GetTimeSeconds() - LastPulseTime;

	// Determine the floor multiplier (Walking vs Stopped)
	float FloorMultiplier = bHasInput ? 1.0f : 0.0f;

	// Calculate how many tiers we should have dropped based on time passed
	int32 Drops = FMath::FloorToInt(TimeSincePulse / PulseGracePeriod);
	
	// Calculate target tier relative to the peak of the current combo
	float TargetTier = HighestMultiplier - (Drops * DropStepAmount);
	
	// Clamp to floor
	TargetTier = FMath::Max(TargetTier, FloorMultiplier);

	// Apply the decay or recovery
	if (CurrentMultiplier > TargetTier)
	{
		// Collapse down to the next tier rapidly
		CurrentMultiplier = FMath::FInterpTo(CurrentMultiplier, TargetTier, DeltaTime, PulseDropSpeed);
	}
	else if (bHasInput && CurrentMultiplier < 1.0f)
	{
		// Recover from 0 to 1 (Walking startup)
		CurrentMultiplier += 0.5f * DeltaTime; 
	}

	// Hard clamp
	CurrentMultiplier = FMath::Clamp(CurrentMultiplier, 0.0f, MaxMultiplier);


	// --- B. STEERING CALCULATIONS ---
	
	// Map Multiplier to Steering responsiveness (Reverse Inertia)
	// 1.0x Speed = 0.0 Alpha (Heavy). 3.0x Speed = 1.0 Alpha (Snappy).
	float ControlAlpha = FMath::GetMappedRangeValueClamped(FVector2D(1.0f, MaxMultiplier), FVector2D(0.0f, 1.0f), CurrentMultiplier);
	
	// Override control if within Pulse window
	if (TimeSincePulse < PulseControlDuration) 
	{
		ControlAlpha = 1.0f;
	}

	float CurrentSteeringRate = FMath::Lerp(MinSteeringRate, MaxSteeringRate, ControlAlpha);


	// --- C. APPLY FORCES ---
	
	// Decompose velocity again for horizontal processing
	FVector VerticalVel = (Velocity | SurfaceNormal) * SurfaceNormal;
	FVector HorizontalVel = Velocity - VerticalVel;
	float CurrentSpeed = HorizontalVel.Size();
	float TargetSpeedMagnitude = BaseSpeed * CurrentMultiplier;

	// Calculate desired direction relative to surface
	FVector CamFwd = CameraComp->GetForwardVector();
	FVector CamRight = CameraComp->GetRightVector();
	FVector InputDir = (CamFwd * CurrentInput.X + CamRight * CurrentInput.Y).GetSafeNormal();
	InputDir = FVector::VectorPlaneProject(InputDir, SurfaceNormal).GetSafeNormal();

	// Apply Steering (Vector Rotation)
	if (!InputDir.IsZero() && CurrentSpeed > 10.0f)
	{
		FVector CurrentDir = HorizontalVel.GetSafeNormal();
		FVector NewDir = FMath::VInterpNormalRotationTo(CurrentDir, InputDir, DeltaTime, CurrentSteeringRate * 10.0f);
		HorizontalVel = NewDir * CurrentSpeed;
	}

	// Acceleration Logic
	if (CurrentSpeed < TargetSpeedMagnitude && bHasInput)
	{
		// Kickstart if stationary
		if (HorizontalVel.IsZero()) HorizontalVel = InputDir * 100.0f;
		
		// Apply Engine Force
		HorizontalVel += InputDir * Acceleration * DeltaTime;
	}
	else if (CurrentSpeed > TargetSpeedMagnitude || !bHasInput)
	{
		// Drag Logic (Slow down to match target)
		HorizontalVel *= FMath::Max(0.0f, 1.0f - (Friction * DeltaTime));
	}

	// Reassemble final velocity
	Velocity = HorizontalVel + VerticalVel;
}

// ==============================================================================
// HELPERS & EVENTS
// ==============================================================================

void APcPlayerCharacter::NotifyActorBeginOverlap(AActor* Other) 
{ 
	if (APcPlanet* P = Cast<APcPlanet>(Other)) 
	{
		CurrentPlanet = P;
	}
}

void APcPlayerCharacter::NotifyActorEndOverlap(AActor* Other) 
{ 
	if (Other == CurrentPlanet) 
	{
		CurrentPlanet = nullptr;
	}
}

FVector APcPlayerCharacter::SlideAlongSurface(const FVector& InVel, const FVector& Normal) 
{ 
	return InVel - Normal * (InVel | Normal); 
}

FString APcPlayerCharacter::GetDebugInfo() const 
{ 
	return FString::Printf(TEXT("Mult: %.2f\nSpeed: %.0f\nPeak: %.2f"), CurrentMultiplier, Velocity.Size(), HighestMultiplier); 
}