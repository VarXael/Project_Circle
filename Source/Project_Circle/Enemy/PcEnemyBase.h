// ==========================================
// FILE: PcEnemyBase.h
// PATH: Source/Project_Circle/Enemy/PcEnemyBase.h
// ==========================================
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PcEnemyBase.generated.h"

class UPcGravityMovementComponent;
class USphereComponent;
class UStaticMeshComponent;

UCLASS()
class PROJECT_CIRCLE_API APcEnemyBase : public AActor
{
	GENERATED_BODY()
	
public:	
	APcEnemyBase();

protected:
	virtual void BeginPlay() override;

public:	
	// --- SHARED COMPONENTS ---
	UPROPERTY(VisibleAnywhere, Category = "Components")
	UStaticMeshComponent* MeshComp;

	// Used for projectile collisions
	UPROPERTY(VisibleAnywhere, Category = "Components")
	USphereComponent* HitBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UPcGravityMovementComponent* GravityComp;

	// --- CONFIG ---
	UPROPERTY(EditAnywhere, Category = "Enemy Config")
	float ScoreReward = 100.0f;

	UPROPERTY(EditAnywhere, Category = "Enemy Config")
	float ChargeReward = 0.2f;

	// --- API ---
	// Called by Projectile
	virtual void HandleHit();

protected:
	// Visual Feedback
	UPROPERTY()
	UMaterialInstanceDynamic* DynamicMat;
	
	FTimerHandle TimerHandle_ColorReset;
	void ResetColor();
};