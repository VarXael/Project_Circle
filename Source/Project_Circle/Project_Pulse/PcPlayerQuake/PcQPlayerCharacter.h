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
	// ── Components ────────────────────────────────────────────────────────────
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UCameraComponent* CameraComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UPcQPlayerMovementComponent* MoveComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UPcQHealthComponent* HealthComp;

	// ── Input Assets ──────────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input") UInputMappingContext* DefaultMappingContext;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input") UInputAction* IA_Move;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input") UInputAction* IA_Look;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input") UInputAction* IA_Jump;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input") UInputAction* IA_GroundPound;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input") UInputAction* IA_Fire;

	// ── Look sensitivity ──────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input") float LookSensitivityX = 0.4f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input") float LookSensitivityY = 0.4f;

	// ── Combat ────────────────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat") float BaseDamage           = 25.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat") float PistolBaseCooldownSec = 0.2f;  // plain rate of fire, no beat gating

	UFUNCTION(BlueprintPure) float GetPistolCooldownAlpha() const;

	// ── Camera feedback ───────────────────────────────────────────────────────
	// Beat punch: small FOV dip on every gameplay beat (implicit rhythm feel).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Beat") float CameraBeatPunch  = 3.f;

	// Slide FOV / camera drop (mirrors the old boost camera, now tied to slide state).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Slide") float SlideCameraDropZ = 18.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Slide") float SlideFOVGain     =  8.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Slide") float SlideCameraSpeed =  8.f;

private:
	// ── Input handlers ────────────────────────────────────────────────────────
	void Input_Move(const FInputActionValue& Value);
	void Input_Look(const FInputActionValue& Value);
	void Input_JumpPressed();
	void Input_JumpReleased();
	void Input_GroundPound();
	void Input_Fire();

	// ── Combat ────────────────────────────────────────────────────────────────
	void TryFire();
	bool IsOnBeat() const; // used for NotifyEnemyHit, NOT for gating fire

	// ── Beat callback ─────────────────────────────────────────────────────────
	UFUNCTION() void OnGameplayBeat(float BeatTimestamp);

	// ── Camera ────────────────────────────────────────────────────────────────
	void UpdateCameraEffects(float DeltaTime);

	float DefaultCameraZ      = 60.f;
	float DefaultFOV          = 90.f;
	float CurrentSlideAlpha   = 0.f;  // 0 = not sliding, 1 = fully sliding
	float PistolCooldown      = 0.f;
	float BeatFOVOffset       = 0.f;
};