#include "PcPlanet.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"

APcPlanet::APcPlanet()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	PlanetMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlanetMesh"));
	PlanetMesh->SetupAttachment(RootComponent);

	InfluenceZone = CreateDefaultSubobject<USphereComponent>(TEXT("InfluenceZone"));
	InfluenceZone->SetupAttachment(PlanetMesh);
	
	InfluenceZone->SetSphereRadius(4000.0f);
	InfluenceZone->SetCollisionProfileName(TEXT("Trigger"));
}

FVector APcPlanet::GetGravityDirection(FVector Location) const
{
	// HOLLOW LOGIC:
	// Gravity pulls the player AWAY from the center (sticking them to the inside wall).
	// Vector: Center -> Player
	return (Location - GetActorLocation()).GetSafeNormal();
}