#include "PcPlayerCharacter.h"
#include "Project_Circle/Planet/PcPlanet.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Project_Circle/Weapon/PcWeapon.h"

APcPlayerCharacter::APcPlayerCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	CameraComp = CreateDefaultSubobject<UCameraComponent>(TEXT("CameraComp"));
	CameraComp->SetupAttachment(GetCapsuleComponent());
	CameraComp->SetRelativeLocation(FVector(0, 0, 60.0f));
	CameraComp->bUsePawnControlRotation = false;

	if (GetCharacterMovement())
	{
		GetCharacterMovement()->GravityScale = 0.0f;
		GetCharacterMovement()->DefaultLandMovementMode = MOVE_Flying;
	}

	GetCapsuleComponent()->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
}

void APcPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();
	CurrentSpeed = BaseMoveSpeed;

	// SPAWN AND ATTACH WEAPON
	if (StartingWeaponClass)
	{
		FActorSpawnParameters P;
		P.Owner = this;
		P.Instigator = this;

		// 1. Spawn
		CurrentWeapon = GetWorld()->SpawnActor<APcWeapon>(StartingWeaponClass, GetActorTransform(), P);

		// 2. Attach
		if (CurrentWeapon)
		{
			CurrentWeapon->AttachToPlayer(this);
		}
	}
}

// --- INPUTS ---

void APcPlayerCharacter::Input_Move(FVector2D Value) { CurrentInput = FVector(Value.X, Value.Y, 0.0f); }

void APcPlayerCharacter::Input_Look(FVector2D Value) 
{
	// 1. Rotate Character/Camera
	if (Value.X != 0.0f) AddActorLocalRotation(FRotator(0, Value.X, 0));
	if (Value.Y != 0.0f) 
	{
		FRotator Rot = CameraComp->GetRelativeRotation();
		Rot.Pitch = FMath::Clamp(Rot.Pitch + Value.Y, -85.0f, 85.0f);
		CameraComp->SetRelativeRotation(Rot);
	}

	// 2. Send Input to Weapon for Sway
	if (CurrentWeapon)
	{
		CurrentWeapon->ApplyInputForSway(Value);
	}
}

// --- WEAPON INPUT DELEGATION ---

void APcPlayerCharacter::Input_StartAttack()
{
	if (CurrentWeapon) CurrentWeapon->StartPrimaryFire();
}

void APcPlayerCharacter::Input_StopAttack()
{
	if (CurrentWeapon) CurrentWeapon->StopPrimaryFire();
}

void APcPlayerCharacter::Input_FireLaser()
{
	if (CurrentWeapon) CurrentWeapon->FireLaserAttack();
}

void APcPlayerCharacter::TakeHit()
{
	OnHitReceived();
}

// --- SKATER PHYSICS LOGIC (Preserved) ---

void APcPlayerCharacter::Input_JumpTrigger() 
{ 
	// 1. Kickstart from Mud (First Jump)
	if (!bIsWaveActive)
	{
		bIsWaveActive = true;
		WavePhase = 0.0f;

		// Calculate height based on entry speed
		float SpeedRatio = CurrentSpeed / MaxSkimSpeed;
		CurrentJumpPeak = FMath::Lerp(MinJumpHeight, MaxJumpHeight, SpeedRatio);
		return;
	}
	
	// 2. Rhythm Boost (The Fix)
	// A full wave is 2*PI. 
	// We want the window to be at the very END of the cycle (resurfacing).
	float WaveEnd = 2.0f * PI;
	float WindowSize = WaveEnd * CoyoteThreshold; // e.g. 10% of the wave
	float WindowStart = WaveEnd - WindowSize;

	// Check if we are in that final slice of time
	bool bInRhythmWindow = (WavePhase > WindowStart);

	if (bInRhythmWindow)
	{
		// PERFECT JUMP
		CurrentSpeed += JumpBoostAmount;
		CurrentSpeed = FMath::Min(CurrentSpeed, MaxSkimSpeed);

		// Recalculate height for the next chain
		float SpeedRatio = CurrentSpeed / MaxSkimSpeed;
		CurrentJumpPeak = FMath::Lerp(MinJumpHeight, MaxJumpHeight, SpeedRatio);

		WavePhase = 0.0f; // Reset Wave
	}
	else
	{
		// EARLY / LATE PRESS
		// If you press too early (while high in air), we ignore it.
		// If you press while deep in the sink but not ready to surface, we ignore it.
		// Punishment comes from Gravity/Drag naturally if you miss the window.
	}
}


void APcPlayerCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	ApplySkaterMovement(DeltaTime);
}

void APcPlayerCharacter::ApplySkaterMovement(float DeltaTime)
{
	// 1. FLOOR & ORIENTATION
	FVector ActorLoc = GetActorLocation();
	FVector UpVector = GetActorUpVector();
	FHitResult GroundHit; FCollisionQueryParams P; P.AddIgnoredActor(this);
	
	FVector TraceStart = ActorLoc + (UpVector * 500.0f); 
	FVector TraceEnd = ActorLoc - (UpVector * 500.0f);
	bool bFoundGround = GetWorld()->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, ECC_WorldStatic, P);

	if (!bFoundGround) return;
	FVector SurfaceNormal = GroundHit.Normal;

	FQuat CurrentRot = GetActorQuat();
	FQuat TargetRot = FQuat::FindBetweenNormals(GetActorUpVector(), SurfaceNormal) * CurrentRot;
	SetActorRotation(FQuat::Slerp(CurrentRot, TargetRot, 20.0f * DeltaTime));

	// --------------------------------------------------------
	// SCREEN-RELATIVE PHYSICS
	// --------------------------------------------------------

	// A. INPUT CALC
	FVector CamFwd = FVector::VectorPlaneProject(CameraComp->GetForwardVector(), SurfaceNormal).GetSafeNormal();
	FVector CamRight = FVector::VectorPlaneProject(CameraComp->GetRightVector(), SurfaceNormal).GetSafeNormal();
	FVector InputDir = (CamFwd * CurrentInput.X) + (CamRight * CurrentInput.Y);
	InputDir.Normalize();

	if (HorizontalVelocity.IsZero() && !InputDir.IsZero())
	{
		HorizontalVelocity = InputDir * BaseMoveSpeed;
		CurrentSpeed = BaseMoveSpeed; 
	}

	FVector CurrentDir = HorizontalVelocity.GetSafeNormal();

	// B. CONTROL CURVE
	float SpeedRatio = CurrentSpeed / MaxSkimSpeed;
	SpeedRatio = FMath::Clamp(SpeedRatio, 0.0f, 1.0f);
	CurrentSteeringRate = FMath::Lerp(MinSteeringRate, MaxSteeringRate, SpeedRatio);

	// --- AIR STEERING BOOST ---
	// If in air, we allow tighter turns to help snaking
	if (bIsWaveActive) 
	{
		CurrentSteeringRate *= 1.5f; 
	}

	// C. STEERING
	if (!InputDir.IsZero())
	{
		FVector NewDir = FMath::VInterpNormalRotationTo(CurrentDir, InputDir, DeltaTime, CurrentSteeringRate);
		HorizontalVelocity = NewDir * HorizontalVelocity.Size();
		CurrentDir = NewDir;
	}

	// --------------------------------------------------------
	// D. ACCELERATION LOGIC
	// --------------------------------------------------------

	float Dot = (InputDir | CurrentDir); 
	float CarveFactor = 0.0f;

	if (!InputDir.IsZero() && Dot > 0.0f)
	{
		// Forgiving Carve Range
		CarveFactor = FMath::GetMappedRangeValueClamped(FVector2D(1.0f, 0.96f), FVector2D(0.0f, 1.0f), Dot);
	}

	CarveIntensity = FMath::FInterpTo(CarveIntensity, CarveFactor, DeltaTime, 5.0f);
	
	// Physics Shared Calculations
	float MudFactor = 1.0f - SpeedRatio; 
	
	// Base Thrust from Carving
	float ExponentialBonus = (SpeedRatio * SpeedRatio * MomentumMultiplier);
	float ThrustForce = CarveAcceleration * CarveFactor * (0.5f + ExponentialBonus);

	if (bIsWaveActive)
	{
		// --- AIR LOGIC (UPDATED) ---
		
		// 1. Air Acceleration:
		// We ALLOW carving in the air now!
		// In fact, it's slightly more efficient because there is no drag fighting you.
		if (CarveFactor > 0.0f)
		{
			// Add speed while snaking in air
			CurrentSpeed += ThrustForce * DeltaTime; 
		}
		
		// 2. Minimal Air Drag (Just to cap infinite speed)
		CurrentSpeed -= 10.0f * MudFactor * DeltaTime;
	}
	else
	{
		// --- GROUND LOGIC ---
		
		if (InputDir.IsZero() || Dot <= 0.0f)
		{
			// Hard Stop
			CurrentSpeed -= StraightLineDrag * 2.0f * DeltaTime;
		}
		else
		{
			// The Fight: Thrust vs Water Drag
			CurrentSpeed += (ThrustForce - StraightLineDrag) * DeltaTime;
		}
	}

	CurrentSpeed = FMath::Clamp(CurrentSpeed, 0.0f, MaxSkimSpeed);
	
	// Stop completely if slow
	if (CurrentSpeed < 10.0f && InputDir.IsZero()) HorizontalVelocity = FVector::ZeroVector;
	else HorizontalVelocity = CurrentDir * CurrentSpeed;


	// --------------------------------------------------------
	// E. WAVE / BUOYANCY
	// --------------------------------------------------------
	float TargetAltitude = 0.0f;

	if (bIsWaveActive)
	{
		float PhaseSpeed = (2.0f * PI) / WaveDuration;
		WavePhase += PhaseSpeed * DeltaTime;
		float RawSine = FMath::Sin(WavePhase);
		if (RawSine >= 0.0f) TargetAltitude = RawSine * CurrentJumpPeak; 
		else TargetAltitude = RawSine * MudDepth; 
		
		if (WavePhase >= 2.0f * PI) 
		{ 
			bIsWaveActive = false; 
			WavePhase = 0.0f; 
		}
	}
	else
	{
		float Lift = CarveIntensity * LiftSensitivity * MudDepth; 
		TargetAltitude = -MudDepth + Lift;
		TargetAltitude = FMath::Min(TargetAltitude, 0.0f); 
	}

	SmoothedAltitude = FMath::FInterpTo(SmoothedAltitude, TargetAltitude, DeltaTime, 10.0f);

	FVector NewGroundPos = GroundHit.Location + (HorizontalVelocity * DeltaTime);
	SetActorLocation(NewGroundPos + (SurfaceNormal * SmoothedAltitude));
}

FString APcPlayerCharacter::GetDebugInfo() const
{
	return FString::Printf(TEXT("SPEED: %.0f\nSTEER RATE: %.0f\nJUMP PEAK: %.0f"),
	                       CurrentSpeed, CurrentSteeringRate, CurrentJumpPeak);
}

bool APcPlayerCharacter::IsInRhythmWindow() const
{
	if (!bIsWaveActive) return false;
	
	float WaveEnd = 2.0f * PI;
	float WindowSize = WaveEnd * CoyoteThreshold;
	float WindowStart = WaveEnd - WindowSize;
	
	return WavePhase > WindowStart;
}

void APcPlayerCharacter::NotifyActorBeginOverlap(AActor* Other)
{
	if (APcPlanet* P = Cast<APcPlanet>(Other)) CurrentPlanet = P;
}

void APcPlayerCharacter::NotifyActorEndOverlap(AActor* Other) { if (Other == CurrentPlanet) CurrentPlanet = nullptr; }
