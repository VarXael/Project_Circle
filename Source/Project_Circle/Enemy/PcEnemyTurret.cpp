#include "PcEnemyTurret.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/GameplayStatics.h"
// Include the new component header
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

	// Create the Universal Movement Component
	GravityComp = CreateDefaultSubobject<UPcGravityMovementComponent>(TEXT("GravityComp"));
	
	// "GroundUnit" gives high friction (snappy movement)
	GravityComp->MovementMode = EPcMovementMode::GroundUnit;
		
	GravityComp->MaxSpeed = MovementSpeed;
	GravityComp->Acceleration = 2000.0f; // Fast start
	GravityComp->Deceleration = 2000.0f; // Fast stop
		
	// IMPORTANT: Adjust this if your enemy sinks into the ground
	// 0 = Pivot at feet. 90 = Pivot in center (for 180cm tall unit).
	GravityComp->PivotOffset = 0.0f; 
		
	// 5.0f = Heavy water feel. 15.0f = Solid ground feel.
	GravityComp->VerticalSmoothing = 10.0f; 
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

	// --- AI LOGIC ---
	APawn* Player = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (Player)
	{
		float Dist = FVector::Dist(GetActorLocation(), Player->GetActorLocation());
		
		// Calculate Direction to Player
		// We don't need to worry about "Sphere Math" here. 
		// The component handles wrapping this vector around the world.
		FVector DirToPlayer = (Player->GetActorLocation() - GetActorLocation()).GetSafeNormal();

		// Move if too far
		if (Dist > StopDistance)
		{
			GravityComp->AddInputVector(DirToPlayer);
		}
		
		// The Component handles Rotation automatically when moving (GroundUnit mode).
		// If we are stopped, we might want to manually rotate to face player:
		if (Dist <= StopDistance)
		{
			// Manual aiming when standing still
			FVector Up = GravityComp->GetSurfaceNormal();
			FVector FlatDir = FVector::VectorPlaneProject(DirToPlayer, Up).GetSafeNormal();
			FRotator LookRot = UKismetMathLibrary::MakeRotFromXZ(FlatDir, Up);
			
			SetActorRotation(FMath::RInterpTo(GetActorRotation(), LookRot, DeltaTime, 10.0f));
		}
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
		// Pass nullptr for Planet. 
		// Ideally, APcProjectile should be updated to use UPcGravityMovementComponent too!
		// For now, it will use its legacy logic.
		Proj->InitializeProjectile(Forward, nullptr, false);
	}
}

void APcEnemyTurret::DebugLaunch()
{
	if (GravityComp)
	{
		// Launch UP (Inwards) + Random Direction
		FVector Up = GravityComp->GetSurfaceNormal();
		FVector RandDir = FMath::VRand();
        
		FVector LaunchForce = (Up * 3000.0f) + (RandDir * 1000.0f);
        
		GravityComp->AddImpulse(LaunchForce);
	}
}
