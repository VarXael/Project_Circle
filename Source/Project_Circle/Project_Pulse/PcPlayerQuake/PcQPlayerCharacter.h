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
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") UCameraComponent*             CameraComp;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") UPcQPlayerMovementComponent*  MoveComp;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") UPcQHealthComponent*          HealthComp;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input") UInputMappingContext* DefaultMappingContext;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input") UInputAction* IA_Move;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input") UInputAction* IA_Look;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input") UInputAction* IA_Jump;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input") UInputAction* IA_GroundPound;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input") UInputAction* IA_Slide;   // NEW — separate slide button
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input") UInputAction* IA_Fire;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat") float BaseDamage         = 25.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")  float LookSensitivityX   =  0.4f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")  float LookSensitivityY   =  0.4f;

	// Pistol
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat") float PistolBaseCooldownSec = 0.5f;
	UFUNCTION(BlueprintPure) float GetPistolCooldownAlpha() const;

	// Camera — boost slide
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Boost") float BoostCameraDropZ  = 22.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Boost") float BoostFOVGain      =  8.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Boost") float BoostCameraSpeed  =  8.f;

	// Camera — auto-jump sync
	// Persistent FOV widening so the player physically feels the sync state
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|AutoJump") float AutoJumpFOVBoost   =  6.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|AutoJump") float AutoJumpCamSpeed   =  5.f;

	// Rhythm beat punch (subtle FOV dip on every beat)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera|Rhythm") float CameraBeatPunch = 3.f;

	// Slide hold: how long the button must be held on the ground before it becomes a slide (not a dash)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input|Slide") float SlideActivateThreshold = 0.18f;

private:
	void Input_Move(const FInputActionValue& Value);
	void Input_Look(const FInputActionValue& Value);
	void Input_JumpPressed();
	void Input_JumpReleased();
	void Input_GroundPound();      // simple press — no tap/hold
	void Input_SlidePressed();
	void Input_SlideReleased();
	void Input_Fire();
	void TryFire();
	bool IsOnBeat() const;

	UFUNCTION() void OnGameplayBeat(float BeatTimestamp);
	UFUNCTION() void OnActiveBeatAction_Handler();

	void UpdateCameraEffects(float DeltaTime);

	// Slide hold tracking (character-side only — MC receives clean intent calls)
	bool  bSlideHeld     = false;
	float SlideHeldTime  = 0.f;
	bool  bSlideActivated = false;  // true once the hold crossed the threshold this press

	float DefaultCameraZ        = 60.f;
	float DefaultFOV            = 90.f;
	float CurrentBoostAlpha     = 0.f;
	float CurrentAutoJumpAlpha  = 0.f;   // smooth lerp for auto-jump FOV/effects
	float PistolCooldown        = 0.f;
	float BeatFOVOffset         = 0.f;
};