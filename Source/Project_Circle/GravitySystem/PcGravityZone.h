#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PcGravityZone.generated.h"

class USphereComponent;

UENUM(BlueprintType)
enum class EGravityShape : uint8
{
	Radial      UMETA(DisplayName = "Radial (Planet)"),
	Directional UMETA(DisplayName = "Directional (Flat Room)")
};

UCLASS()
class PROJECT_CIRCLE_API APcGravityZone : public AActor
{
	GENERATED_BODY()
	
public:	
	APcGravityZone();

	// --- COMPONENTS ---
	// Visual representation and logic bounds
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USphereComponent* ZoneBounds;

	// --- SETTINGS ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gravity Settings")
	EGravityShape GravityType = EGravityShape::Radial;

	/** If true, gravity pulls AWAY from the center (Inside a sphere). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gravity Settings")
	bool bIsHollow = true;

	/** 
	 * Objects inside this radius (near center) experience Zero Gravity 
	 * to prevent mathematical singularities.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gravity Settings")
	float SingularityRadius = 500.0f;

	// --- API ---
	
	/** 
	 * Calculates the gravity direction for a specific location.
	 * Returns FVector::ZeroVector if inside the Singularity (Zero G).
	 */
	UFUNCTION(BlueprintPure, Category = "Gravity Math")
	FVector GetGravityDirection(FVector TargetLocation) const;

	/** Returns the ideal mathematical surface point (for trace targeting). */
	UFUNCTION(BlueprintPure, Category = "Gravity Math")
	FVector GetIdealSurfaceLocation(FVector TargetLocation) const;

	/** Returns the center of the zone. */
	UFUNCTION(BlueprintPure, Category = "Gravity Math")
	FVector GetZoneCenter() const;
};