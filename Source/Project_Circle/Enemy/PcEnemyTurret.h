#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PcEnemyTurret.generated.h"

class APcProjectile;
// KEY CHANGE: We use the new Movement Component
class UPcGravityMovementComponent; 

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
	// --- COMPONENTS ---
	UPROPERTY(VisibleAnywhere, Category = "Components")
	UStaticMeshComponent* MeshComp;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	USceneComponent* MuzzleLoc;

	/** The Universal Movement Motor */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UPcGravityMovementComponent* GravityComp;

	// --- COMBAT ---
	UPROPERTY(EditAnywhere, Category = "Combat")
	TSubclassOf<APcProjectile> ProjectileClass;

	UPROPERTY(EditAnywhere, Category = "Combat")
	float FireRate = 2.0f;

	// --- AI SETTINGS ---
	UPROPERTY(EditAnywhere, Category = "AI")
	float MovementSpeed = 400.0f;

	UPROPERTY(EditAnywhere, Category = "AI")
	float StopDistance = 800.0f; // Stop moving if close to player

	// Add this temporary function
	UFUNCTION(BlueprintCallable, Category = "Debug")
	void DebugLaunch();
	
private:
	FTimerHandle TimerHandle_Shoot;
	
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void Shoot();
	
	
};