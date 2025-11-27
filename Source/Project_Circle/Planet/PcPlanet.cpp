#include "PcPlanet.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"

APcPlanet::APcPlanet()
{
	PrimaryActorTick.bCanEverTick = false;

	// 1. Root
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	// 2. Mesh (Attached to Root)
	PlanetMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlanetMesh"));
	PlanetMesh->SetupAttachment(RootComponent);

	// 3. Trigger (Attached to Mesh)
	InfluenceZone = CreateDefaultSubobject<USphereComponent>(TEXT("InfluenceZone"));
	InfluenceZone->SetupAttachment(RootComponent);
	
	InfluenceZone->SetSphereRadius(4000.0f);
	InfluenceZone->SetCollisionProfileName(TEXT("Trigger"));
}

FVector APcPlanet::GetGravityDirection(FVector Location) const
{
	// Returns vector pointing FROM Player TO Planet Center
	return (GetActorLocation() - Location).GetSafeNormal();
}

float APcPlanet::GetAltitude(FVector Location) const
{
	float Dist = FVector::Dist(Location, GetActorLocation());
	return Dist - SurfaceRadius;
}