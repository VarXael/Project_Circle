#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PcWeapon.generated.h"

class USkeletalMeshComponent;
class UNiagaraSystem;
class APcPlayerCharacter;
class APcProjectile;

UCLASS()
class PROJECT_CIRCLE_API APcWeapon : public AActor
{
	GENERATED_BODY()
	
public:	
	APcWeapon();
	virtual void Tick(float DeltaTime) override;
	virtual void BeginPlay() override;

	void AttachToPlayer(APcPlayerCharacter* TargetPlayer);
	void ApplyInputForSway(FVector2D LookInput);

	// --- COMBAT INTERFACE ---
	void StartPrimaryFire();
	void StopPrimaryFire();
	
	// RESTORED LASER
	void FireLaserAttack();

protected:
	UPROPERTY(VisibleAnywhere)
	USkeletalMeshComponent* WeaponMesh;

	UPROPERTY(VisibleAnywhere)
	USceneComponent* MuzzleLocation;

	// --- CONFIG PROJECTILE ---
	UPROPERTY(EditAnywhere, Category = "Combat")
	TSubclassOf<APcProjectile> ProjectileClass;

	UPROPERTY(EditAnywhere, Category = "Combat")
	float FireRate = 0.12f;

	// --- CONFIG LASER ---
	UPROPERTY(EditAnywhere, Category = "Combat | Laser")
	UNiagaraSystem* LaserBeamFX;

	UPROPERTY(EditAnywhere, Category = "Combat | Laser")
	float LaserMaxRange = 5000.0f;

	UPROPERTY(EditAnywhere, Category = "Combat | Laser")
	float LaserCooldownDuration = 2.0f;

	// --- ANIMATION CONFIG ---
	UPROPERTY(EditAnywhere, Category = "Animation")
	float SwayAmount = 2.0f;
	UPROPERTY(EditAnywhere, Category = "Animation")
	float SwaySpeed = 5.0f;

private:
	APcPlayerCharacter* OwningPlayer;
	FTimerHandle TimerHandle_AutoFire;
	FTimerHandle TimerHandle_LaserCooldown;
	bool bIsLaserReady = true;

	// Animation State
	FVector CurrentRecoilLoc;
	FRotator CurrentRecoilRot;
	FVector CurrentSwayLoc;
	FRotator CurrentSwayRot;
	FVector2D CurrentLookInput;

	void PerformStandardShot();
	void ResetLaserCooldown();
};