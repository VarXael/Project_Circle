#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PcEnemyTurret.generated.h"

class APcPlanet;
class APcProjectile;

UCLASS()
class PROJECT_CIRCLE_API APcEnemyTurret : public AActor
{
	GENERATED_BODY()
	
public:	
	APcEnemyTurret();

protected:
	virtual void Tick(float DeltaTime) override;
	virtual void BeginPlay() override;

public:
	// For Projectile to call
	UFUNCTION(BlueprintImplementableEvent)
	void OnHitReceived();

	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* MeshComp;

	UPROPERTY(VisibleAnywhere)
	USceneComponent* MuzzleLoc;

	UPROPERTY(EditAnywhere, Category = "Combat")
	TSubclassOf<APcProjectile> ProjectileClass;

	UPROPERTY(EditAnywhere, Category = "Combat")
	float FireRate = 2.0f;

	UPROPERTY(VisibleAnywhere, Category = "Gravity")
	APcPlanet* CurrentPlanet;

private:
	FTimerHandle TimerHandle_Shoot;
	UFUNCTION(BlueprintCallable)
	void Shoot();
};