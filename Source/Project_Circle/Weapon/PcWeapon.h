#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PcWeapon.generated.h"

class UNiagaraSystem;
class APcProjectile;
class APcPlayerCharacter;

UCLASS()
class PROJECT_CIRCLE_API APcWeapon : public AActor
{
	GENERATED_BODY()
	
public:	
	APcWeapon();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

public:
	// --- ACTIONS ---
	void StartPrimaryFire();
	void StopPrimaryFire();
	void FireLaserAttack();

	// Called by Player Input
	void ApplyInputForSway(FVector2D LookInput);

	// Called by Player BeginPlay
	void AttachToPlayer(APcPlayerCharacter* TargetPlayer);

protected:
	void PerformStandardShot();
	void ResetLaserCooldown(); 

public:
	// --- COMPONENTS ---
	UPROPERTY(VisibleAnywhere, Category = "Weapon")
	USkeletalMeshComponent* WeaponMesh;

	UPROPERTY(VisibleAnywhere, Category = "Weapon")
	USceneComponent* MuzzleLocation;

	// --- CONFIGURATION ---
	UPROPERTY(EditAnywhere, Category = "Combat")
	TSubclassOf<APcProjectile> ProjectileClass;

	UPROPERTY(EditAnywhere, Category = "Combat")
	float FireRate = 0.12f;

	// --- LASER COOLDOWN ---
	UPROPERTY(EditAnywhere, Category = "Combat|Laser")
	float LaserCooldownDuration = 2.0f; 

	UPROPERTY(EditAnywhere, Category = "Combat|Laser")
	float LaserInputBuffer = 0.2f; 

	// --- RECOIL SETTINGS ---
	UPROPERTY(EditAnywhere, Category = "Recoil|Standard")
	FVector StandardRecoilPos = FVector(-10.0f, 0.0f, 2.0f); 
	UPROPERTY(EditAnywhere, Category = "Recoil|Standard")
	FRotator StandardRecoilRot = FRotator(5.0f, 0.0f, 0.0f); 
	UPROPERTY(EditAnywhere, Category = "Recoil|Standard")
	float StandardRecoverySpeed = 15.0f; 

	UPROPERTY(EditAnywhere, Category = "Recoil|Laser")
	FVector LaserRecoilPos = FVector(-40.0f, 0.0f, 15.0f); 
	UPROPERTY(EditAnywhere, Category = "Recoil|Laser")
	FRotator LaserRecoilRot = FRotator(25.0f, 0.0f, 0.0f); 
	
	// Note: Laser Recovery Speed is now calculated automatically!

	// --- WEAPON SWAY SETTINGS ---
	UPROPERTY(EditAnywhere, Category = "Sway")
	float SwayAmount = 3.0f; // How far the gun moves when looking
	UPROPERTY(EditAnywhere, Category = "Sway")
	float SwayRotationAmount = 2.0f; // How much it tilts
	UPROPERTY(EditAnywhere, Category = "Sway")
	float SwaySpeed = 5.0f; // How fast it catches up to the camera
	UPROPERTY(EditAnywhere, Category = "Sway")
	float SwayMax = 10.0f; // Clamp to prevent clipping
	
	// --- VFX ---
	
	UPROPERTY(EditAnywhere, Category = "Combat|Laser")
	UNiagaraSystem* LaserBeamFX;

	UPROPERTY(EditAnywhere, Category = "Combat|Laser")
	float LaserMaxRange = 50000.0f;

private:
	UPROPERTY()
	APcPlayerCharacter* OwningPlayer;

	FTimerHandle TimerHandle_AutoFire;
	FTimerHandle TimerHandle_LaserCooldown; 

	bool bIsLaserReady = true; 

	// Procedural Animation Variables
	FVector CurrentRecoilLoc = FVector::ZeroVector;
	FRotator CurrentRecoilRot = FRotator::ZeroRotator;
	float CurrentRecoverySpeed = 10.0f; 

	// Sway Variables
	FVector2D CurrentLookInput = FVector2D::ZeroVector;
	FVector CurrentSwayLoc = FVector::ZeroVector;
	FRotator CurrentSwayRot = FRotator::ZeroRotator;
};