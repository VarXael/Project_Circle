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
	
	if (MovementComp)
	{
		// 1. Set Mode
		// "Projectile" mode disables Friction, so it slides forever once it hits the ground.
		MovementComp->MovementMode = EPcMovementMode::Projectile;
		MovementComp->MaxSpeed = 10000.0f; 
		
		// 2. FIX SINKING (Auto-Size)
		// We set the pivot offset to exactly the radius of the sphere collision.
		// This tells the physics engine: "The floor is 15 units below my center."
		if (CollisionComp)
		{
			MovementComp->PivotOffset = CollisionComp->GetScaledSphereRadius();
		}
		
		MovementComp->HoverHeight = 0.0f; 
	}
}

void APcProjectile::InitializeProjectile(FVector ShootDirection, APcPlanet* InPlanet, bool bIsPlayerOwned)
{
	bIsPlayerProjectile = bIsPlayerOwned;

	if (MovementComp)
	{
		float ExtraSpeed = 0.0f;
		if (auto* PC = Cast<APcPlayerCharacter>(GetOwner()))
		{
			// Get the scalar speed (Length of the velocity vector)
			if (PC->GravityComp) ExtraSpeed = PC->GravityComp->GetCurrentVelocity().Size();
		}

		// Multiply the Aim Direction by (Base Speed + Player Current Speed)
		MovementComp->SetVelocity(ShootDirection.GetSafeNormal() * (Speed + ExtraSpeed));
	}
}

void APcProjectile::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	TimeAlive += DeltaTime;
	if (TimeAlive > LifeSpan) 
	{
		Destroy();
		return;
	}

	// Visual Rotation: Spin or face velocity?
	// The component handles "Face Velocity" automatically if MovementMode is Projectile.
}

void APcProjectile::OnOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!OtherActor || OtherActor == this || OtherActor == GetOwner()) return;

	if (bIsPlayerProjectile)
	{
		// Hit Enemy
		if (auto* Enemy = Cast<APcEnemyTurret>(OtherActor))
		{
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
		// Hit Player
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