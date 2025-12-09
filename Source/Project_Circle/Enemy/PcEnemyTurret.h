// ==========================================
// FILE: PcEnemyTurret.h
// ==========================================
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PcEnemyTurret.generated.h"

class APcProjectile;
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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UPcGravityMovementComponent* GravityComp;

	// --- COMBAT ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	TSubclassOf<APcProjectile> ProjectileClass;

	UPROPERTY(EditAnywhere, Category = "Combat")
	float FireRate = 2.0f;

	// --- AI SETTINGS ---
	UPROPERTY(EditAnywhere, Category = "AI")
	float StopDistance = 800.0f; 

	UPROPERTY(EditAnywhere, Category = "AI")
	float TurretRotationSpeed = 5.0f;

private:
	
	UFUNCTION(BlueprintCallable)
	void Shoot();
};