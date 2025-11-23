#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PcPlanet.generated.h"

class USphereComponent;

UCLASS()
class PROJECT_CIRCLE_API APcPlanet : public AActor
{
	GENERATED_BODY()
	
public:	
	APcPlanet();

	// Returns the Gravity Direction (Normalized)
	UFUNCTION(BlueprintCallable, Category = "Project Circle")
	FVector GetGravityDirection(const FVector& TargetLocation) const;

	// Returns the signed distance to the surface.
	// Positive (+) = Above Ground (Air)
	// Negative (-) = Below Ground (Sinking/Water)
	UFUNCTION(BlueprintCallable, Category = "Project Circle")
	float GetAltitude(const FVector& TargetLocation) const;

public:
	UPROPERTY(VisibleAnywhere, Category = "Components")
	USphereComponent* InfluenceZone;

	// The radius of the PHYSICAL floor (the "Water Surface")
	UPROPERTY(EditAnywhere, Category = "Gravity Settings")
	float SurfaceRadius = 3000.0f;

	// TRUE = Void (Walking inside). FALSE = Planet (Walking outside).
	UPROPERTY(EditAnywhere, Category = "Gravity Settings")
	bool bIsVoidInside = true;
};