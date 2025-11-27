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

	// Root Component (Scene)
	UPROPERTY(VisibleAnywhere, Category = "Planet")
	USceneComponent* SceneRoot;

	// Visual Mesh
	UPROPERTY(VisibleAnywhere, Category = "Planet")
	UStaticMeshComponent* PlanetMesh;

	// Logic Trigger (Attached to Mesh)
	UPROPERTY(VisibleAnywhere, Category = "Planet")
	USphereComponent* InfluenceZone;

	// The Radius the player walks on (Visual Radius)
	UPROPERTY(EditAnywhere, Category = "Planet")
	float SurfaceRadius = 2000.0f;

	// Math Helpers
	FVector GetGravityDirection(FVector Location) const;
	float GetAltitude(FVector Location) const;
};