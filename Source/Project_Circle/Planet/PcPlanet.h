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

	UPROPERTY(VisibleAnywhere, Category = "Planet")
	USceneComponent* SceneRoot;

	UPROPERTY(VisibleAnywhere, Category = "Planet")
	UStaticMeshComponent* PlanetMesh;

	UPROPERTY(VisibleAnywhere, Category = "Planet")
	USphereComponent* InfluenceZone;

	// The Radius of the hollow shell
	UPROPERTY(EditAnywhere, Category = "Planet")
	float SurfaceRadius = 2000.0f;

	// Gravity pulls OUTWARDS (Centrifuge)
	FVector GetGravityDirection(FVector Location) const;
};