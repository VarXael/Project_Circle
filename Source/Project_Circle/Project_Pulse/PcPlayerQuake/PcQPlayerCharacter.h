#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "PcQPlayerMovementComponent.h"
#include "PcQPlayerCharacter.generated.h"

class UCameraComponent;
class UInputMappingContext;
class UInputAction;
struct FInputActionValue;
class UPcQHealthComponent;
class USkeletalMeshComponent;
class APcQEnemyBase;

UCLASS()
class PROJECT_CIRCLE_API APcQPlayerCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	APcQPlayerCharacter(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") UCameraComponent* CameraComp;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") USkeletalMeshComponent* WeaponMesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") USkeletalMeshComponent* SwordMesh;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") UPcQPlayerMovementComponent* MoveComp;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") UPcQHealthComponent* HealthComp;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input") UInputMappingContext* DefaultMappingContext;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input") UInputAction* IA_Move;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input") UInputAction* IA_Look;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input") UInputAction* IA_Jump;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input") UInputAction* IA_GroundPound;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input") UInputAction* IA_Fire;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input") UInputAction* IA_Melee;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input") float LookSensitivityX = 0.4f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input") float LookSensitivityY = 0.4f;

	// ── Combat: Gun Variables ──────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Gun") float GunBaseDamage = 25.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Gun") float GunHeadshotMultiplier = 3.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Gun") float GunBeatMultiplier = 2.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Gun") float GunCooldownSec = 0.2f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Gun") float GunBulletRadius = 25.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Gun") int32 MaxAmmo = 6;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Gun") float ReloadDuration = 1.2f;

	// ── Combat: Sword Variables ────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Sword") float SwordDamage = 45.f; 

	UFUNCTION(BlueprintPure) float GetPistolCooldownAlpha() const;
	UFUNCTION(BlueprintPure) int32 GetCurrentAmmo() const { return CurrentAmmo; }
	UFUNCTION(BlueprintPure) bool  IsReloading() const { return bIsReloading; }
	
	UFUNCTION() void HandleGroundPulseHit();
	UFUNCTION() void HandleSwordHitEnemy(APcQEnemyBase* Enemy);

	// ── Camera & Sway Settings ────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Beat") float CameraBeatPunch  = 3.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Slide") float SlideCameraDropZ = 18.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Slide") float SlideFOVGain     =  8.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Slide") float SlideCameraSpeed =  8.f;

	UPROPERTY(EditAnywhere, Category = "Combat|Sway") FVector BaseWeaponLocation = FVector(20.f, 15.f, -10.f);
	UPROPERTY(EditAnywhere, Category = "Combat|Sway") FRotator BaseWeaponRotation = FRotator(0.f, 0.f, 0.f);
	
	UPROPERTY(EditAnywhere, Category = "Combat|Sway") FVector BaseSwordLocation = FVector(20.f, -18.f, -8.f);
	UPROPERTY(EditAnywhere, Category = "Combat|Sway") FRotator BaseSwordRotation = FRotator(0.f, 0.f, 0.f); // NORMAL GRIP

	UPROPERTY(EditAnywhere, Category = "Combat|Sway") float SwayRotMultiplier = -1.5f;
	UPROPERTY(EditAnywhere, Category = "Combat|Sway") float SwaySmoothness = 12.f;
	UPROPERTY(EditAnywhere, Category = "Combat|Sway") float RecoilRecoverySpeed = 15.f;

private:
	void Input_Move(const FInputActionValue& Value);
	void Input_Look(const FInputActionValue& Value);
	void Input_JumpPressed();
	void Input_JumpReleased();
	void Input_GroundPound();
	void Input_Fire();
	void Input_Melee();

	void TryFire();
	void TryMelee();
	bool IsOnBeat() const;

	UFUNCTION() void OnGameplayBeat(float BeatTimestamp);
	void UpdateCameraEffects(float DeltaTime);
	void UpdateWeaponSway(float DeltaTime);

	float DefaultCameraZ      = 60.f;
	float DefaultFOV          = 90.f;
	float CurrentSlideAlpha   = 0.f; 
	float PistolCooldown      = 0.f;
	float BeatFOVOffset       = 0.f;

	int32 CurrentAmmo = 6;
	float ReloadTimer = 0.f;
	bool  bIsReloading = false;

	float SwordCooldownTimer = 0.f;
	float SwordStrikeTimer = 0.f;
	float SwordStrikeMaxTime = 0.15f; 

	FVector2D CurrentLookDelta;
	FRotator CurrentSwayRot;
	FVector CurrentSwayLoc;
	FRotator CurrentRecoilRot;
	FVector CurrentRecoilLoc;

	FRotator CurrentSwordSwayRot;
	FVector CurrentSwordSwayLoc;
	FRotator SwordStrikeRotOffset;
	FVector SwordStrikeLocOffset;
};