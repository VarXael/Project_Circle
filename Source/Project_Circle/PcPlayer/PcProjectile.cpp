#include "PcProjectile.h"
#include "Project_Circle/GravitySystem/PcGravityMovementComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Project_Circle/Enemy/PcEnemyTurret.h"
#include "Project_Circle/PcPlayer/PcPlayerCharacter.h"

APcProjectile::APcProjectile()
{
	PrimaryActorTick.bCanEverTick = true;

	CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("SphereComp"));
	CollisionComp->InitSphereRadius(15.0f);
	CollisionComp->SetCollisionProfileName("OverlapAllDynamic"); 
	CollisionComp->OnComponentBeginOverlap.AddDynamic(this, &APcProjectile::OnOverlap);
	RootComponent = CollisionComp;

	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
	MeshComp->SetupAttachment(CollisionComp);
	MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	MovementComp = CreateDefaultSubobject<UPcGravityMovementComponent>(TEXT("MovementComp"));
}

void APcProjectile::BeginPlay()
{
	Super::BeginPlay();
	
	// Configure Component for "Space Flight"
	if (MovementComp)
	{
		MovementComp->MovementMode = EPcMovementMode::Projectile;
		MovementComp->MaxSpeed = 10000.0f; 
		MovementComp->PivotOffset = 0.0f; 
		MovementComp->HoverHeight = 0.0f; 
	}
}

void APcProjectile::InitializeProjectile(FVector ShootDirection, APcPlanet* InPlanet, bool bIsPlayerOwned)
{
	bIsPlayerProjectile = bIsPlayerOwned;

	if (MovementComp)
	{
		// 1. SET VELOCITY
		MovementComp->SetVelocity(ShootDirection.GetSafeNormal() * Speed);
		
		// 2. LOCK ORBIT (CRITICAL FIX)
		// This tells the component: "Whatever distance I am from the center right now, stay there."
		// This prevents the projectile from sinking or drifting.
		MovementComp->LockCurrentAltitudeAsOrbit();
	}
}

void APcProjectile::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Lifetime
	TimeAlive += DeltaTime;
	if (TimeAlive > LifeSpan) 
	{
		Destroy();
		return;
	}
}

void APcProjectile::OnOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!OtherActor || OtherActor == this || OtherActor == GetOwner()) return;

	if (bIsPlayerProjectile)
	{
		// HIT ENEMY
		if (auto* Enemy = Cast<APcEnemyTurret>(OtherActor))
		{
			// Apply Physics Impulse to Enemy
			if (MovementComp && Enemy->GravityComp)
			{
				FVector ImpactDir = MovementComp->GetCurrentVelocity().GetSafeNormal();
				Enemy->GravityComp->AddImpulse(ImpactDir * 2000.0f);
			}
			Destroy();
		}
	}
	else
	{
		// HIT PLAYER
		if (auto* Player = Cast<APcPlayerCharacter>(OtherActor))
		{
			if (!Player->IsInRhythmWindow()) 
			{
				Player->TakeHit(); 
				Destroy();
			}
		}
	}
}