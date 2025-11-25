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
	// 1. Kickstart from Mud
	if (!bIsWaveActive)
	{
		bIsWaveActive = true;
		WavePhase = 0.0f;

		float SpeedRatio = (CurrentSpeed - BaseMoveSpeed) / (MaxSkimSpeed - BaseMoveSpeed);
		CurrentJumpPeak = FMath::Lerp(MinJumpHeight, MaxJumpHeight, SpeedRatio);
		return;
	}

	// 2. Rhythm Boost
	float AirEnd = PI;
	float CoyoteZone = AirEnd - (AirEnd * CoyoteThreshold);
	bool bInRhythmWindow = WavePhase > CoyoteZone;

	if (bInRhythmWindow)
	{
		CurrentSpeed += JumpBoostAmount;
		CurrentSpeed = FMath::Min(CurrentSpeed, MaxSkimSpeed);

		float SpeedRatio = (CurrentSpeed - BaseMoveSpeed) / (MaxSkimSpeed - BaseMoveSpeed);
		CurrentJumpPeak = FMath::Lerp(MinJumpHeight, MaxJumpHeight, SpeedRatio);

		WavePhase = 0.0f;
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
	FHitResult GroundHit;
	FCollisionQueryParams P;
	P.AddIgnoredActor(this);

	FVector TraceStart = ActorLoc + (UpVector * 500.0f);
	FVector TraceEnd = ActorLoc - (UpVector * 500.0f);
	bool bFoundGround = GetWorld()->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, ECC_WorldStatic, P);

	if (!bFoundGround) return;
	FVector SurfaceNormal = GroundHit.Normal;

	FQuat CurrentRot = GetActorQuat();
	FQuat TargetRot = FQuat::FindBetweenNormals(GetActorUpVector(), SurfaceNormal) * CurrentRot;
	SetActorRotation(FQuat::Slerp(CurrentRot, TargetRot, 20.0f * DeltaTime));

	// 2. SKATER PHYSICS
	FVector CamFwd = FVector::VectorPlaneProject(CameraComp->GetForwardVector(), SurfaceNormal).GetSafeNormal();
	FVector CamRight = FVector::VectorPlaneProject(CameraComp->GetRightVector(), SurfaceNormal).GetSafeNormal();
	FVector InputDir = (CamFwd * CurrentInput.X) + (CamRight * CurrentInput.Y);
	InputDir.Normalize();

	if (HorizontalVelocity.IsZero() && !InputDir.IsZero())
		HorizontalVelocity = InputDir * 100.0f;

	FVector CurrentDir = HorizontalVelocity.GetSafeNormal();

	// Speed Ratio
	float SpeedRatio = (CurrentSpeed - BaseMoveSpeed) / (MaxSkimSpeed - BaseMoveSpeed);
	SpeedRatio = FMath::Clamp(SpeedRatio, 0.0f, 1.0f);

	CurrentSteeringRate = FMath::Lerp(MinSteeringRate, MaxSteeringRate, SpeedRatio);

	// Steer
	if (!InputDir.IsZero())
	{
		FVector NewDir = FMath::VInterpNormalRotationTo(CurrentDir, InputDir, DeltaTime, CurrentSteeringRate);
		HorizontalVelocity = NewDir * HorizontalVelocity.Size();
		CurrentDir = NewDir;
	}

	// Carve
	float Dot = (InputDir | CurrentDir);
	bool bIsTurning = !InputDir.IsZero() && (Dot < 0.98f) && (Dot > 0.0f);
	CarveIntensity = FMath::FInterpTo(CarveIntensity, (bIsTurning ? 1.0f : 0.0f), DeltaTime, 5.0f);

	float MudFactor = 1.0f - SpeedRatio;

	if (bIsWaveActive)
	{
		CurrentSpeed -= 10.0f * MudFactor * DeltaTime;
	}
	else
	{
		if (bIsTurning)
		{
			float Accel = CarveAcceleration * (1.0f + SpeedRatio);
			CurrentSpeed += Accel * DeltaTime;
		}
		else if (InputDir.IsZero() || Dot <= 0.0f)
		{
			CurrentSpeed -= BaseDrag * 400.0f * DeltaTime;
		}
		else
		{
			CurrentSpeed -= BaseDrag * 100.0f * MudFactor * DeltaTime;
		}
	}

	CurrentSpeed = FMath::Clamp(CurrentSpeed, BaseMoveSpeed, MaxSkimSpeed);
	HorizontalVelocity = CurrentDir * CurrentSpeed;

	// 3. BUOYANCY / JUMP
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
	float AirEnd = PI;
	float CoyoteZone = AirEnd - (AirEnd * CoyoteThreshold);
	return WavePhase > CoyoteZone;
}

void APcPlayerCharacter::NotifyActorBeginOverlap(AActor* Other)
{
	if (APcPlanet* P = Cast<APcPlanet>(Other)) CurrentPlanet = P;
}

void APcPlayerCharacter::NotifyActorEndOverlap(AActor* Other) { if (Other == CurrentPlanet) CurrentPlanet = nullptr; }
