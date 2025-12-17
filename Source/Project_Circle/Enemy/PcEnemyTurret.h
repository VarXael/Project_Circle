// ==========================================
// FILE: PcEnemyTurret.h
// PATH: Source/Project_Circle/Enemy/PcEnemyTurret.h
// ==========================================
#pragma once

#include "CoreMinimal.h"
#include "PcEnemyBase.h" // Inherit from Base
#include "PcEnemyTurret.generated.h"

class APcProjectile;

UCLASS()
class PROJECT_CIRCLE_API APcEnemyTurret : public APcEnemyBase
{
	GENERATED_BODY()
	
public:	
	APcEnemyTurret();

protected:
	virtual void Tick(float DeltaTime) override;
	virtual void BeginPlay() override;

public:
	// --- COMPONENTS (Mesh, HitBox, Gravity are now in Base) ---
	
	UPROPERTY(VisibleAnywhere, Category = "Components")
	USceneComponent* MuzzleLoc;

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