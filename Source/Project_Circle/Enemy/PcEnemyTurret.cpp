// ==========================================
// FILE: PcEnemyTurret.cpp
// PATH: Source/Project_Circle/Enemy/PcEnemyTurret.cpp
// ==========================================
#include "PcEnemyTurret.h"
#include "Kismet/GameplayStatics.h"
#include "Project_Circle/GravitySystem/PcGravityMovementComponent.h"
#include "Project_Circle/PcPlayer/PcProjectile.h"

APcEnemyTurret::APcEnemyTurret()
{
	PrimaryActorTick.bCanEverTick = true;

	// MeshComp, HitBox, GravityComp created in Base Class Constructor

	MuzzleLoc = CreateDefaultSubobject<USceneComponent>(TEXT("MuzzleLoc"));
	MuzzleLoc->SetupAttachment(MeshComp);
	MuzzleLoc->SetRelativeLocation(FVector(50, 0, 20)); 

	// Configure inherited Gravity Comp
	GravityComp->PivotOffset = 0.0f; 
	GravityComp->VerticalSmoothing = 10.0f; 
}

void APcEnemyTurret::BeginPlay()
{
	Super::BeginPlay();
	// Base::BeginPlay handles Material creation
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
			GravityComp->bOrientRotationToMovement = true;
			GravityComp->AddInputVector(FlatDirectionToPlayer);
		}
		else
		{
			// --- STATE: AIMING (Stationary) ---
			GravityComp->bOrientRotationToMovement = false; 
			
			if (!FlatDirectionToPlayer.IsZero() && !SurfaceNormal.IsZero())
			{
				FRotator TargetRot = FRotationMatrix::MakeFromXZ(FlatDirectionToPlayer, SurfaceNormal).Rotator();
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

	FActorSpawnParameters P; 
	P.Owner = this; 
	
	auto* Proj = GetWorld()->SpawnActor<APcProjectile>(ProjectileClass, SpawnLoc, SpawnRot, P);
	if (Proj)
	{
		Proj->InitializeProjectile(TargetDir, nullptr, false);
	}
}