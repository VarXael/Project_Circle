#include "PcEnemyTurret.h"
#include "Project_Circle/Planet/PcPlanet.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Project_Circle/PcPlayer/PcProjectile.h"

APcEnemyTurret::APcEnemyTurret()
{
	PrimaryActorTick.bCanEverTick = true;

	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
	RootComponent = MeshComp;

	MuzzleLoc = CreateDefaultSubobject<USceneComponent>(TEXT("MuzzleLoc"));
	MuzzleLoc->SetupAttachment(MeshComp);
	MuzzleLoc->SetRelativeLocation(FVector(50, 0, 20)); 
}

void APcEnemyTurret::BeginPlay()
{
	Super::BeginPlay();
	
	// Try to find a planet, but don't panic if we don't
	AActor* PlanetActor = UGameplayStatics::GetActorOfClass(GetWorld(), APcPlanet::StaticClass());
	if (PlanetActor) CurrentPlanet = Cast<APcPlanet>(PlanetActor);

	// Start Shooting
	GetWorldTimerManager().SetTimer(TimerHandle_Shoot, this, &APcEnemyTurret::Shoot, FireRate, true);
}

void APcEnemyTurret::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Look at Player
	APawn* Player = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (Player)
	{
		FVector MyLoc = GetActorLocation();
		FVector TargetLoc = Player->GetActorLocation();
		
		// FALLBACK: If no planet, Up is Z (0,0,1)
		FVector UpVector = FVector::UpVector;
		if (CurrentPlanet)
		{
			UpVector = -CurrentPlanet->GetGravityDirection(MyLoc);
		}

		// Project direction onto the surface plane so it doesn't tilt up/down
		FVector DirToPlayer = (TargetLoc - MyLoc).GetSafeNormal();
		FVector FlatDir = FVector::VectorPlaneProject(DirToPlayer, UpVector).GetSafeNormal();

		// Use MakeRotFromXZ to ensure we stay upright relative to the floor
		FRotator LookRot = UKismetMathLibrary::MakeRotFromXZ(FlatDir, UpVector);
		SetActorRotation(LookRot);
	}
}

void APcEnemyTurret::Shoot()
{
	// FIX: Removed "!CurrentPlanet" check. 
	// The Projectile handles null planets automatically now.
	if (!ProjectileClass) return;

	FVector SpawnLoc = MuzzleLoc->GetComponentLocation();
	FRotator SpawnRot = MuzzleLoc->GetComponentRotation();
	FVector Forward = MuzzleLoc->GetForwardVector();

	// Spawn
	FActorSpawnParameters P; 
	P.Owner = this; 
	// Instigator removed to fix compile error
	
	auto* Proj = GetWorld()->SpawnActor<APcProjectile>(ProjectileClass, SpawnLoc, SpawnRot, P);
	if (Proj)
	{
		// IsPlayerOwned = false
		Proj->InitializeProjectile(Forward, CurrentPlanet, false);
	}
}