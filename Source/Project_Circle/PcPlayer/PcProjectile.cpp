#include "PcProjectile.h"
#include "Project_Circle/GravitySystem/PcGravityMovementComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "PcPlayerCharacter.h"
#include "Project_Circle/Enemy/PcEnemyTurret.h"

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

	// Create the Motor
	MovementComp = CreateDefaultSubobject<UPcGravityMovementComponent>(TEXT("MovementComp"));
}

void APcProjectile::BeginPlay()
{
	Super::BeginPlay();
	
	// Configure for Bullet behavior
	if (MovementComp)
	{
		MovementComp->MovementMode = EPcMovementMode::Projectile;
		MovementComp->MaxSpeed = 10000.0f; // High cap
		
		// Important: Projectiles don't need to hover relative to feet. 
		// They fly where they are spawned.
		MovementComp->PivotOffset = 0.0f; 
		MovementComp->HoverHeight = 0.0f; 
	}
}

void APcProjectile::InitializeProjectile(FVector ShootDirection, APcPlanet* InPlanet, bool bIsPlayerOwned)
{
	bIsPlayerProjectile = bIsPlayerOwned;

	if (MovementComp)
	{
		// Set the velocity immediately
		FVector LaunchVelocity = ShootDirection.GetSafeNormal() * Speed;
		MovementComp->SetVelocity(LaunchVelocity);
	}
}

void APcProjectile::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 1. LIFETIME MANAGEMENT
	TimeAlive += DeltaTime;
	if (TimeAlive > LifeSpan) 
	{
		Destroy();
		return;
	}

	// 2. VISUALS
	// The Component moves the actor. We just align the mesh to the velocity.
	if (MovementComp && !MovementComp->GetCurrentVelocity().IsZero())
	{
		SetActorRotation(MovementComp->GetCurrentVelocity().Rotation());
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
			// Example: Knockback test using the Component
			if (MovementComp)
			{
				Enemy->GravityComp->AddImpulse(MovementComp->GetCurrentVelocity().GetSafeNormal() * 2000.0f);
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