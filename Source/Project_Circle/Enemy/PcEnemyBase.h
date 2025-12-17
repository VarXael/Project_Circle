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
class USceneComponent;

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

	UPROPERTY(VisibleAnywhere, Category = "Components")
	USphereComponent* HitBox;

	// NEW: A dedicated point to spawn floating text (move this in BP!)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* ScoreSpawnLoc;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UPcGravityMovementComponent* GravityComp;

	// --- CONFIG ---
	UPROPERTY(EditAnywhere, Category = "Enemy Config")
	float ScoreReward = 10.0f; 

	UPROPERTY(EditAnywhere, Category = "Enemy Config")
	float ChargeReward = 0.2f;

	// --- API ---
	virtual void HandleHit();

protected:
	UPROPERTY()
	UMaterialInstanceDynamic* DynamicMat;
	
	FTimerHandle TimerHandle_ColorReset;
	void ResetColor();
};