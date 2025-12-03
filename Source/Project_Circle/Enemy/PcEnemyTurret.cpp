#include "PcEnemyTurret.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Project_Circle/GravitySystem/PcGravityMovementComponent.h"
#include "Project_Circle/PcPlayer/PcProjectile.h"
#include "Engine/Engine.h" // For Debug Messages

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
	
	GetWorldTimerManager().SetTimer(TimerHandle_Shoot, this, &APcEnemyTurret::Shoot, FireRate, true);
}

void APcEnemyTurret::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!GravityComp) return;

	APawn* Player = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (Player)
	{
		float Dist = FVector::Dist(GetActorLocation(), Player->GetActorLocation());
		
		// DEBUG: Print status to screen to see why it stops
		if (GEngine) 
		{
			FString Status = (Dist > StopDistance) ? TEXT("MOVING") : TEXT("STOPPED (In Range)");
			FString VelInfo = FString::Printf(TEXT("Vel: %.1f"), GravityComp->GetCurrentVelocity().Size());
			GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Yellow, FString::Printf(TEXT("[%s] %s - Dist: %.0f - %s"), *GetName(), *Status, Dist, *VelInfo));
		}

		if (Dist > StopDistance)
		{
			// 1. Get Direction
			FVector RawDir = (Player->GetActorLocation() - GetActorLocation()).GetSafeNormal();

			// 2. Flatten Direction to Surface
			// This prevents trying to walk "through" the air if the player is far away
			FVector Up = GravityComp->GetSurfaceNormal();
			FVector GroundDir = FVector::VectorPlaneProject(RawDir, Up).GetSafeNormal();

			// 3. Move
			GravityComp->AddInputVector(GroundDir);
		}
		
		// DELETED: Manual SetActorRotation. 
		// The component handles this now (bOrientRotationToMovement = true).
	}
}

void APcEnemyTurret::Shoot()
{
	if (!ProjectileClass) return;

	FVector SpawnLoc = MuzzleLoc->GetComponentLocation();
	FRotator SpawnRot = MuzzleLoc->GetComponentRotation();
	FVector Forward = MuzzleLoc->GetForwardVector();

	FActorSpawnParameters P; P.Owner = this; 
	
	auto* Proj = GetWorld()->SpawnActor<APcProjectile>(ProjectileClass, SpawnLoc, SpawnRot, P);
	if (Proj)
	{
		Proj->InitializeProjectile(Forward, nullptr, false);
	}
}