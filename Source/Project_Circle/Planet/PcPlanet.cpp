#include "PcPlanet.h"
#include "Components/SphereComponent.h"

APcPlanet::APcPlanet()
{
	PrimaryActorTick.bCanEverTick = false;

	InfluenceZone = CreateDefaultSubobject<USphereComponent>(TEXT("InfluenceZone"));
	InfluenceZone->SetSphereRadius(4000.0f); // Bigger than surface to catch player early
	InfluenceZone->SetCollisionProfileName(TEXT("Trigger"));
	RootComponent = InfluenceZone;
}

FVector APcPlanet::GetGravityDirection(const FVector& TargetLocation) const
{
	FVector Center = GetActorLocation();
	FVector Direction = (TargetLocation - Center).GetSafeNormal();
	return bIsVoidInside ? Direction : -Direction;
}

float APcPlanet::GetAltitude(const FVector& TargetLocation) const
{
	float DistToCenter = FVector::Dist(TargetLocation, GetActorLocation());
	
	if (bIsVoidInside)
	{
		// Void: Surface is at Radius. We are "Above" if closer to center (Dist < Radius)
		// Wait, for Void:
		// Center (0) ... Air ... Surface (3000) ... Rock (4000)
		// So "Altitude" is Distance from Rock.
		// If Dist = 2900, Altitude = 100 (Air).
		// If Dist = 3100, Altitude = -100 (Underwater).
		return SurfaceRadius - DistToCenter;
	}
	else
	{
		// Planet: Surface is at Radius. We are "Above" if further away (Dist > Radius)
		return DistToCenter - SurfaceRadius;
	}
}