#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PcProjectile.generated.h"

class USphereComponent;
class UPcGravityMovementComponent; // The new motor
class APcPlanet;

UCLASS()
class PROJECT_CIRCLE_API APcProjectile : public AActor
{
	GENERATED_BODY()
	
public:	
	APcProjectile();

	/** 
	 * Fired by Weapon. 
	 * @param ShootDirection: The world direction to fly.
	 * @param InPlanet: (Optional) The gravity component will find it anyway, but we keep the signature compatible.
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