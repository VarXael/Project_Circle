#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PcProjectile.generated.h"

class USphereComponent;
class APcPlanet;

UCLASS()
class PROJECT_CIRCLE_API APcProjectile : public AActor
{
	GENERATED_BODY()
	
public:	
	APcProjectile();
	void InitializeProjectile(FVector ShootDirection, APcPlanet* InPlanet);

protected:
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(VisibleAnywhere)
	USphereComponent* CollisionComp;

	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* MeshComp;

	// --- CONFIG ---
	UPROPERTY(EditAnywhere, Category = "Projectile")
	float Speed = 2000.0f;

	UPROPERTY(EditAnywhere, Category = "Projectile")
	float LifeSpan = 5.0f; 

	UPROPERTY(EditAnywhere, Category = "Projectile")
	float HoverHeight = 32.0f; 

	/** Gravity applied when in the air. */
	UPROPERTY(EditAnywhere, Category = "Projectile")
	float GravityStrength = 1500.0f;

private:
	FVector Velocity;
	APcPlanet* CurrentPlanet;
	float TimeAlive;
	bool bIsAirborne = true; 

	void HandleAirMovement(float DeltaTime);
	void HandleSurfaceMovement(float DeltaTime);
	
	UFUNCTION()
	void OnOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
};