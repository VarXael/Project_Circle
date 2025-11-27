#include "PcPlayerCharacter.h"
#include "Project_Circle/Planet/PcPlanet.h"
#include "Project_Circle/Weapon/PcWeapon.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/GameplayStatics.h"

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

	// 1. SPAWN WEAPON
	if (StartingWeaponClass)
	{
		FActorSpawnParameters P;
		P.Owner = this;
		P.Instigator = this;

		CurrentWeapon = GetWorld()->SpawnActor<APcWeapon>(StartingWeaponClass, GetActorTransform(), P);
		if (CurrentWeapon)
		{
			CurrentWeapon->AttachToPlayer(this);
		}
	}

	// 2. FIND PLANET (Auto-detect start)
	TArray<AActor*> Planets;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), APcPlanet::StaticClass(), Planets);

	for (AActor* Actor : Planets)
	{
		if (APcPlanet* P = Cast<APcPlanet>(Actor))
		{
			float Dist = FVector::Dist(GetActorLocation(), P->GetActorLocation());
			// If inside influence (safe large number)
			if (Dist < 10000.0f)
			{
				CurrentPlanet = P;
				break;
			}
		}
	}
}

// ==============================================================================
// INPUTS
// ==============================================================================

void APcPlayerCharacter::Input_Move(FVector2D Value) { CurrentInput = FVector(Value.X, Value.Y, 0.0f); }

void APcPlayerCharacter::Input_Look(FVector2D Value)
{
	if (Value.X != 0.0f) AddActorLocalRotation(FRotator(0, Value.X, 0));
	if (Value.Y != 0.0f)
	{
		FRotator Rot = CameraComp->GetRelativeRotation();
		Rot.Pitch = FMath::Clamp(Rot.Pitch + Value.Y, -85.0f, 85.0f);
		CameraComp->SetRelativeRotation(Rot);
	}

	if (CurrentWeapon) CurrentWeapon->ApplyInputForSway(Value);
}

void APcPlayerCharacter::Input_StartAttack()
{
	if (CurrentWeapon) CurrentWeapon->StartPrimaryFire();
}

void APcPlayerCharacter::Input_StopAttack()
{
	if (CurrentWeapon) CurrentWeapon->StopPrimaryFire();
}

// ==============================================================================
// JUMP & HIT LOGIC
// ==============================================================================

void APcPlayerCharacter::Input_JumpTrigger()
{
	if (!bIsWaveActive)
	{
		bIsWaveActive = true;
		WavePhase = 0.0f;
		// Initial Jump Height
		float SpeedRatio = (CurrentSpeed - BaseMoveSpeed) / (MaxSkimSpeed - BaseMoveSpeed);
		CurrentJumpPeak = FMath::Lerp(MinJumpHeight, MaxJumpHeight, SpeedRatio);
		return;
	}

	float AirEnd = PI;
	float CoyoteZone = AirEnd - (AirEnd * CoyoteThreshold);
	bool bInRhythmWindow = WavePhase > CoyoteZone;

	if (bInRhythmWindow)
	{
		// SUCCESS: Boost Speed
		CurrentSpeed += JumpBoostAmount;
		CurrentSpeed = FMath::Min(CurrentSpeed, MaxSkimSpeed);

		// Update Height for next chain
		float SpeedRatio = (CurrentSpeed - BaseMoveSpeed) / (MaxSkimSpeed - BaseMoveSpeed);
		CurrentJumpPeak = FMath::Lerp(MinJumpHeight, MaxJumpHeight, SpeedRatio);

		WavePhase = 0.0f;
	}
}

// Add this function body
void APcPlayerCharacter::Input_FireLaser()
{
	if (CurrentWeapon) CurrentWeapon->FireLaserAttack();
}

void APcPlayerCharacter::TakeHit() { OnHitReceived(); }

FString APcPlayerCharacter::GetDebugInfo() const
{
	FString PlanetStatus = CurrentPlanet ? TEXT("PLANET") : TEXT("FLAT");
	return FString::Printf(TEXT("[%s]\nSPEED: %.0f\nCARVE: %.2f\nALT: %.1f"),
	                       *PlanetStatus, CurrentSpeed, CarveIntensity, SmoothedAltitude);
}

bool APcPlayerCharacter::IsInRhythmWindow() const
{
	if (!bIsWaveActive) return false;
	float AirEnd = PI;
	float CoyoteZone = AirEnd - (AirEnd * CoyoteThreshold);
	return WavePhase > CoyoteZone;
}

// ==============================================================================
// PHYSICS LOOP
// ==============================================================================

void APcPlayerCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	ApplySkaterMovement(DeltaTime);
}

void APcPlayerCharacter::ApplySkaterMovement(float DeltaTime)
{
	FVector ActorLoc = GetActorLocation();
	FVector OldUpVector = GetActorUpVector(); 
	FVector SurfaceNormal = FVector::UpVector;

	// 1. DETERMINE SURFACE
	if (CurrentPlanet)
	{
		SurfaceNormal = -CurrentPlanet->GetGravityDirection(ActorLoc);
	}
	else
	{
		FHitResult GroundHit; 
		FCollisionQueryParams P; P.AddIgnoredActor(this);
		if (GetWorld()->LineTraceSingleByChannel(GroundHit, ActorLoc, ActorLoc - (OldUpVector * 2000.0f), ECC_WorldStatic, P))
		{
			SurfaceNormal = GroundHit.Normal;
		}
	}

	// 2. ALIGNMENT
	FQuat SurfaceRotation = FQuat::FindBetweenNormals(OldUpVector, SurfaceNormal);
	HorizontalVelocity = SurfaceRotation.RotateVector(HorizontalVelocity);

	FQuat CurrentRot = GetActorQuat();
	FQuat TargetRot = FQuat::FindBetweenNormals(GetActorUpVector(), SurfaceNormal) * CurrentRot;
	SetActorRotation(FQuat::Slerp(CurrentRot, TargetRot, 20.0f * DeltaTime));


	// 3. SKATER INPUT
	FVector CamFwd = FVector::VectorPlaneProject(CameraComp->GetForwardVector(), SurfaceNormal).GetSafeNormal();
	FVector CamRight = FVector::VectorPlaneProject(CameraComp->GetRightVector(), SurfaceNormal).GetSafeNormal();
	FVector InputDir = (CamFwd * CurrentInput.X) + (CamRight * CurrentInput.Y);
	InputDir.Normalize();

	// FIX: KICKSTART LOGIC
	// Only boost if stopped AND pressing Gas (Forward/W)
	if (HorizontalVelocity.IsZero() && CurrentInput.X > 0.1f)
	{
		HorizontalVelocity = InputDir * BaseMoveSpeed;
		CurrentSpeed = BaseMoveSpeed; 
	}

	// Tangent Project
	HorizontalVelocity = FVector::VectorPlaneProject(HorizontalVelocity, SurfaceNormal);
	
	FVector CurrentDir = HorizontalVelocity.GetSafeNormal();
	if (CurrentDir.IsZero()) CurrentDir = CamFwd;

	// 4. PHYSICS
	float SpeedRatio = CurrentSpeed / MaxSkimSpeed;
	SpeedRatio = FMath::Clamp(SpeedRatio, 0.0f, 1.0f);
	float CurrentSteeringRate = FMath::Lerp(MinSteeringRate, MaxSteeringRate, SpeedRatio);

	// Steering
	if (!InputDir.IsZero())
	{
		FVector NewDir = FMath::VInterpNormalRotationTo(CurrentDir, InputDir, DeltaTime, CurrentSteeringRate);
		HorizontalVelocity = NewDir * HorizontalVelocity.Size();
		CurrentDir = NewDir;
	}

	// Carve
	float Dot = (InputDir | CurrentDir); 
	float CarveFactor = 0.0f;
	if (!InputDir.IsZero() && Dot > 0.0f)
	{
		CarveFactor = FMath::GetMappedRangeValueClamped(FVector2D(1.0f, 0.96f), FVector2D(0.0f, 1.0f), Dot);
	}
	CarveIntensity = FMath::FInterpTo(CarveIntensity, CarveFactor, DeltaTime, 5.0f);

	float MudFactor = 1.0f - SpeedRatio; 

	if (bIsWaveActive)
	{
		CurrentSpeed -= 50.0f * MudFactor * DeltaTime;
	}
	else
	{
		if (InputDir.IsZero() || Dot <= 0.0f)
		{
			// Braking
			CurrentSpeed -= StraightLineDrag * 2.0f * DeltaTime;
		}
		else
		{
			// Fighting Water
			float DragForce = StraightLineDrag;
			float ExponentialBonus = (SpeedRatio * SpeedRatio * MomentumMultiplier);
			float ThrustForce = CarveAcceleration * CarveFactor * (0.5f + ExponentialBonus);
			CurrentSpeed += (ThrustForce - DragForce) * DeltaTime;
		}
	}

	// Clamp and Stop Logic
	CurrentSpeed = FMath::Clamp(CurrentSpeed, 0.0f, MaxSkimSpeed);
	
	// FIX: Clean Stop
	if (CurrentSpeed < 10.0f && InputDir.IsZero()) 
	{
		HorizontalVelocity = FVector::ZeroVector; 
		CurrentSpeed = 0.0f;
	}
	else 
	{
		HorizontalVelocity = CurrentDir * CurrentSpeed;
	}


	// 5. INTEGRATION
	float TargetAltitude = 0.0f;
	if (bIsWaveActive)
	{
		float PhaseSpeed = (2.0f * PI) / WaveDuration;
		WavePhase += PhaseSpeed * DeltaTime;
		float RawSine = FMath::Sin(WavePhase);
		if (RawSine >= 0.0f) TargetAltitude = RawSine * CurrentJumpPeak; 
		else TargetAltitude = RawSine * MudDepth; 
		if (WavePhase >= 2.0f * PI) { bIsWaveActive = false; WavePhase = 0.0f; }
	}
	else
	{
		float Lift = CarveIntensity * LiftSensitivity * MudDepth; 
		TargetAltitude = -MudDepth + Lift;
		TargetAltitude = FMath::Min(TargetAltitude, 0.0f); 
	}
	SmoothedAltitude = FMath::FInterpTo(SmoothedAltitude, TargetAltitude, DeltaTime, 10.0f);

	FVector NewBaseLocation;

	if (CurrentPlanet)
	{
		FVector MovedLoc = ActorLoc + (HorizontalVelocity * DeltaTime);
		FVector ToCenter = MovedLoc - CurrentPlanet->GetActorLocation();
		FVector RadialDir = ToCenter.GetSafeNormal(); // Outwards
		
		float SurfaceRadius = FMath::Max(CurrentPlanet->SurfaceRadius, 100.0f);
		NewBaseLocation = CurrentPlanet->GetActorLocation() + (RadialDir * SurfaceRadius);
		
		SurfaceNormal = -RadialDir; // Up is Inwards
	}
	else
	{
		NewBaseLocation = ActorLoc + (HorizontalVelocity * DeltaTime);
	}

	FVector FinalLocation = NewBaseLocation + (SurfaceNormal * SmoothedAltitude);
	SetActorLocation(FinalLocation);
}

void APcPlayerCharacter::NotifyActorBeginOverlap(AActor* Other)
{
	if (APcPlanet* P = Cast<APcPlanet>(Other)) CurrentPlanet = P;
}

void APcPlayerCharacter::NotifyActorEndOverlap(AActor* Other) { if (Other == CurrentPlanet) CurrentPlanet = nullptr; }
