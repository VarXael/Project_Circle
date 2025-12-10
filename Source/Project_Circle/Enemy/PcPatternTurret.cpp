// ==========================================
// FILE: PcPatternTurret.cpp
// PATH: Source/Project_Circle/Enemy/PcPatternTurret.cpp
// ==========================================
#include "PcPatternTurret.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Project_Circle/GravitySystem/PcGravityMovementComponent.h"
#include "Project_Circle/PcPlayer/PcProjectile.h"

APcPatternTurret::APcPatternTurret()
{
	PrimaryActorTick.bCanEverTick = false; // Logic is event-driven

	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
	RootComponent = MeshComp;

	MuzzleLoc = CreateDefaultSubobject<USceneComponent>(TEXT("MuzzleLoc"));
	MuzzleLoc->SetupAttachment(MeshComp);
	MuzzleLoc->SetRelativeLocation(FVector(0, 0, 50)); 

	GravityComp = CreateDefaultSubobject<UPcGravityMovementComponent>(TEXT("GravityComp"));
	// Ensure it sticks to walls but doesn't try to slide
	GravityComp->MovementMode = EPcMovementMode::GroundUnit; 
}

void APcPatternTurret::BeginPlay()
{
	Super::BeginPlay();

	if (bAutoFireTest)
	{
		GetWorldTimerManager().SetTimer(TimerHandle_TestFire, this, &APcPatternTurret::TriggerBeatShot, FireRate, true);
	}
}

void APcPatternTurret::TriggerBeatShot()
{
	if (!GravityComp) return;

	// 1. GET COORDINATE SYSTEM
	// On a sphere, "Up" is the surface normal.
	// "Forward" is an arbitrary tangent we pick (Mesh Forward).
	FVector UpVector = GravityComp->GetSurfaceNormal();
	FVector BaseForward = GetActorForwardVector();
	
	// Flatten BaseForward to be perfectly tangent to the surface
	BaseForward = FVector::VectorPlaneProject(BaseForward, UpVector).GetSafeNormal();

	// 2. CALCULATE PATTERN
	switch (PatternType)
	{
	case EBulletPattern::Spiral:
		{
			// Rotate the Forward vector around the Up vector
			// Angle = ShotIndex * Step
			float CurrentAngle = ShotCounter * AngleStepPerShot;
			
			FQuat Rotator = FQuat(UpVector, FMath::DegreesToRadians(CurrentAngle));
			FVector FireDir = Rotator.RotateVector(BaseForward);
			
			SpawnBullet(FireDir);
		}
		break;

	case EBulletPattern::DoubleHelix:
		{
			// Fire two bullets, 180 degrees apart, rotating over time
			float CurrentAngle = ShotCounter * AngleStepPerShot;
			
			// Stream A
			FQuat RotA = FQuat(UpVector, FMath::DegreesToRadians(CurrentAngle));
			SpawnBullet(RotA.RotateVector(BaseForward));

			// Stream B (Offset by 180)
			FQuat RotB = FQuat(UpVector, FMath::DegreesToRadians(CurrentAngle + 180.0f));
			SpawnBullet(RotB.RotateVector(BaseForward));
		}
		break;

	case EBulletPattern::Ring:
		{
			// Fire N bullets in a full circle instantly
			// Does NOT rotate over time (unless you add ShotCounter to BaseAngle)
			float AnglePerBullet = 360.0f / FMath::Max(1, BulletsPerPulse);
			
			for (int32 i = 0; i < BulletsPerPulse; ++i)
			{
				float Angle = i * AnglePerBullet;
				// Optional: Add spin? -> float Angle = (i * AnglePerBullet) + (ShotCounter * 10.0f);
				
				FQuat Rot = FQuat(UpVector, FMath::DegreesToRadians(Angle));
				SpawnBullet(Rot.RotateVector(BaseForward));
			}
		}
		break;
		
	case EBulletPattern::Shotgun:
		{
			// Fire towards player, but spread out based on ShotCounter?
			// Or just a rhythmic blast in a generic direction?
			// Let's do a "Sweeping Fan" logic based on ShotIndex
			
			// PingPong between -45 and 45 degrees
			float Sine = FMath::Sin(ShotCounter * 0.2f); // Slow oscillation
			float FanAngle = Sine * 45.0f;
			
			FQuat Rot = FQuat(UpVector, FMath::DegreesToRadians(FanAngle));
			SpawnBullet(Rot.RotateVector(BaseForward));
		}
		break;
	}

	// 3. INCREMENT COUNTER
	// This is the heartbeat of the pattern
	ShotCounter++;
}

void APcPatternTurret::SpawnBullet(FVector Direction)
{
	if (!ProjectileClass || !MuzzleLoc) return;

	FVector SpawnLoc = MuzzleLoc->GetComponentLocation();
	FRotator SpawnRot = Direction.Rotation();

	FActorSpawnParameters P;
	P.Owner = this;
	// No Instigator because this is an Actor, not a Pawn

	auto* Proj = GetWorld()->SpawnActor<APcProjectile>(ProjectileClass, SpawnLoc, SpawnRot, P);
	if (Proj)
	{
		// FALSE = Enemy Projectile (hurts player)
		// Planet = nullptr (Auto-detected)
		Proj->InitializeProjectile(Direction, nullptr, false);
	}
}