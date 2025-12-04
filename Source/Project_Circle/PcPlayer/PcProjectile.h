#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PcProjectile.generated.h"

class USphereComponent;
class UPcGravityMovementComponent; // The Universal Motor
class APcPlanet; // Kept for function signature compatibility

UCLASS()
class PROJECT_CIRCLE_API APcProjectile : public AActor
{
	GENERATED_BODY()
	
public:	
	APcProjectile();

	/** 
	 * Fired by Weapon. 
	 * @param ShootDirection: The world direction to fly.
	 * @param InPlanet: Ignored (Component finds Zone automatically).
	 */
	void InitializeProjectile(FVector ShootDirection, APcPlanet* InPlanet, bool bIsPlayerOwned);

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	USphereComponent* CollisionComp;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	UStaticMeshComponent* MeshComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UPcGravityMovementComponent* MovementComp;

	UPROPERTY(EditAnywhere, Category = "Projectile")
	float Speed = 2000.0f;

	UPROPERTY(EditAnywhere, Category = "Projectile")
	float LifeSpan = 5.0f; 

private:
	float TimeAlive = 0.0f;
	bool bIsPlayerProjectile = true;

	UFUNCTION()
	void OnOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
};