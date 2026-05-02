#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "PcQPlayerMovementComponent.h"
#include "PcPlayerConfiguration.h"
#include "PcQPlayerCharacter.generated.h"

class UCameraComponent;
class USkeletalMeshComponent;
struct FInputActionValue;
class UPcQHealthComponent;

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
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UCameraComponent* CameraComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USkeletalMeshComponent* WeaponMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UPcQPlayerMovementComponent* MoveComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UPcQHealthComponent* HealthComp;

	// ── Master Configuration ──────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	UPcPlayerConfiguration* PlayerConfig;

	// ── Combat ────────────────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat") float BaseDamage = 25.f;
	UFUNCTION(BlueprintPure) float GetPistolCooldownAlpha() const { return 0.f; }

	// ── Procedural Weapon Sway (Epic/Reaver Style) ────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sway") FVector BaseWeaponLocation = FVector(35.f, 15.f, -15.f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sway") FRotator BaseWeaponRotation = FRotator(0.f, 0.f, 0.f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sway") float SwayRotMultiplier = -1.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sway") float SwaySmoothness = 15.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sway") float RecoilRecoverySpeed = 20.f;

	// ── Camera / Feedback ─────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boost|Camera") float BoostCameraDropZ = 22.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boost|Camera") float BoostFOVGain     =  8.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boost|Camera") float BoostCameraSpeed =  8.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Feedback")     float CameraBeatPunch  =  3.f;

private:
	void Input_Move(const FInputActionValue& Value);
	void Input_Look(const FInputActionValue& Value);
	void Input_JumpPressed();
	void Input_JumpReleased();
	void Input_GroundPound();
	void Input_Dash();
	void Input_AirHop();
	void Input_Fire();
	void TryFire();
	bool IsOnBeat() const;

	UFUNCTION() void OnGameplayBeat(float BeatTimestamp);
	UFUNCTION() void OnActiveBeatAction_Handler();

	void UpdateCameraEffects(float DeltaTime);
	void UpdateWeaponSway(float DeltaTime);

	FVector2D CurrentLookDelta;
	FRotator CurrentSwayRot;
	FVector CurrentSwayLoc;
	FRotator CurrentRecoilRot;
	FVector CurrentRecoilLoc;

	float DefaultCameraZ    = 60.f;
	float DefaultFOV        = 90.f;
	float CurrentBoostAlpha = 0.f;
	float BeatFOVOffset     = 0.f;
};