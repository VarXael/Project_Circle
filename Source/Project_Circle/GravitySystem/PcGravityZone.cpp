#include "PcGravityZone.h"
#include "Components/SphereComponent.h"

APcGravityZone::APcGravityZone()
{
	PrimaryActorTick.bCanEverTick = false;

	ZoneBounds = CreateDefaultSubobject<USphereComponent>(TEXT("ZoneBounds"));
	ZoneBounds->InitSphereRadius(2000.0f);
	ZoneBounds->SetCollisionProfileName(TEXT("Trigger"));
	RootComponent = ZoneBounds;
}

FVector APcGravityZone::GetGravityDirection(FVector TargetLocation) const
{
	if (GravityType == EGravityShape::Radial)
	{
		FVector Center = GetActorLocation();
		float Dist = FVector::Dist(TargetLocation, Center);

		// SINGULARITY CHECK:
		// If too close to center, return Zero (No Gravity / Floating).
		if (Dist < SingularityRadius)
		{
			return FVector::ZeroVector; 
		}

		FVector Dir = (TargetLocation - Center).GetSafeNormal();

		// If Hollow: Gravity pushes OUT (Dir).
		// If Solid: Gravity pulls IN (-Dir).
		return bIsHollow ? Dir : -Dir;
	}
	else // Directional
	{
		// Simple "Down" vector relative to the actor rotation
		return GetActorUpVector() * -1.0f;
	}
}

FVector APcGravityZone::GetIdealSurfaceLocation(FVector TargetLocation) const
{
	// Used to help the Trace find the floor even if the trace misses geometry
	if (GravityType == EGravityShape::Radial)
	{
		FVector Center = GetActorLocation();
		FVector Dir = (TargetLocation - Center).GetSafeNormal();
		
		// Use the SCALED radius of the sphere component
		float Radius = ZoneBounds->GetScaledSphereRadius();
		
		return Center + (Dir * Radius);
	}
	
	// Fallback for directional: Project to Z plane
	return FVector(TargetLocation.X, TargetLocation.Y, GetActorLocation().Z);
}

FVector APcGravityZone::GetZoneCenter() const
{
	return GetActorLocation();
}