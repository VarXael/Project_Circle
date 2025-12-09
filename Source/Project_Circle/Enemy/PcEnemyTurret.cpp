// ==========================================
// FILE: PcEnemyTurret.cpp
// ==========================================
#include "PcEnemyTurret.h"
#include "Kismet/GameplayStatics.h"
#include "Project_Circle/GravitySystem/PcGravityMovementComponent.h"
#include "Project_Circle/PcPlayer/PcProjectile.h"

APcEnemyTurret::APcEnemyTurret()
{
	PrimaryActorTick.bCanEverTick = true;

	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
	RootComponent = MeshComp;

	MuzzleLoc = CreateDefaultSubobject<USceneComponent>(TEXT("MuzzleLoc"));
	MuzzleLoc->SetupAttachment(MeshComp);
	MuzzleLoc->SetRelativeLocation(FVector(50, 0, 20)); 

	GravityComp = CreateDefaultSubobject<UPcGravityMovementComponent>(TEXT("GravityComp"));
	
	// Defaults
	GravityComp->PivotOffset = 0.0f; 
	GravityComp->VerticalSmoothing = 10.0f; 
	GravityComp->MovementMode = EPcMovementMode::GroundUnit;
}

void APcEnemyTurret::BeginPlay()
{
	Super::BeginPlay();
}

void APcEnemyTurret::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!GravityComp) return;

	APawn* Player = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (Player)
	{
		float Dist = FVector::Dist(GetActorLocation(), Player->GetActorLocation());
		
		// 1. Calculate Directions
		FVector ToPlayer = Player->GetActorLocation() - GetActorLocation();
		FVector SurfaceNormal = GravityComp->GetSurfaceNormal();

		// Flatten the "ToPlayer" vector onto the floor plane so we don't look into the ground
		FVector FlatDirectionToPlayer = FVector::VectorPlaneProject(ToPlayer, SurfaceNormal).GetSafeNormal();

		if (Dist > StopDistance)
		{
			// --- STATE: MOVING ---
			// Let the component handle rotation based on velocity (bOrientRotationToMovement)
			GravityComp->bOrientRotationToMovement = true;
			GravityComp->AddInputVector(FlatDirectionToPlayer);
		}
		else
		{
			// --- STATE: AIMING (Stationary) ---
			// 1. Stop Moving
			GravityComp->bOrientRotationToMovement = false; // Stop physics rotation
			
			// 2. Construct the Target Rotation
			// We want:
			// X (Forward) = Pointing at Player (Flat)
			// Z (Up)      = Pointing away from Gravity (Surface Normal)
			if (!FlatDirectionToPlayer.IsZero() && !SurfaceNormal.IsZero())
			{
				FRotator TargetRot = FRotationMatrix::MakeFromXZ(FlatDirectionToPlayer, SurfaceNormal).Rotator();

				// 3. Smoothly Interpolate
				FRotator NewRot = FMath::RInterpTo(GetActorRotation(), TargetRot, DeltaTime, TurretRotationSpeed);
				
				SetActorRotation(NewRot);
			}
		}
	}
}

void APcEnemyTurret::Shoot()
{
	if (!ProjectileClass) return;

	APawn* Player = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!Player) return;

	FVector SpawnLoc = MuzzleLoc->GetComponentLocation();
	FVector TargetDir = (Player->GetActorLocation() - SpawnLoc).GetSafeNormal();
	FRotator SpawnRot = TargetDir.Rotation();

	// FIX: Only set Owner. Do not set Instigator (because 'this' is not a Pawn).
	FActorSpawnParameters P; 
	P.Owner = this; 
	
	auto* Proj = GetWorld()->SpawnActor<APcProjectile>(ProjectileClass, SpawnLoc, SpawnRot, P);
	if (Proj)
	{
		Proj->InitializeProjectile(TargetDir, nullptr, false);
	}
}