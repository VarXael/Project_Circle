#include "PcQEnemyBase.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"

APcQEnemyBase::APcQEnemyBase()
{
	PrimaryActorTick.bCanEverTick = false;

	CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComp"));
	CollisionComp->InitSphereRadius(50.0f);
	CollisionComp->SetCollisionProfileName(TEXT("Pawn"));
	RootComponent = CollisionComp;

	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
	MeshComp->SetupAttachment(RootComponent);

	HealthComp = CreateDefaultSubobject<UPcQHealthComponent>(TEXT("HealthComp"));
}

void APcQEnemyBase::BeginPlay()
{
	Super::BeginPlay();
	if (HealthComp) {
		HealthComp->OnDied.AddDynamic(this, &APcQEnemyBase::OnDeath);
	}
}

void APcQEnemyBase::OnDeath()
{
	// Simple destroy for now. Later we can add ragdolls, sound effects, etc.
	Destroy();
}