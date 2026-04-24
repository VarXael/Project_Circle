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
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") 
	UCameraComponent* CameraComp;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") 
	UPcQPlayerMovementComponent* MoveComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") 
	UPcQHealthComponent* HealthComp;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input") UInputMappingContext* DefaultMappingContext;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input") UInputAction* IA_Move;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input") UInputAction* IA_Look;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input") UInputAction* IA_Jump;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input") UInputAction* IA_GroundPound;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input") UInputAction* IA_Snap;  // right click
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input") UInputAction* IA_Fire;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat") float BaseDamage = 25.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input") float LookSensitivityX = 0.4f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input") float LookSensitivityY = 0.4f;

	// Pistol
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat") float PistolBaseCooldownSec = 0.5f;
	UFUNCTION(BlueprintPure) float GetPistolCooldownAlpha() const;

	// Slide camera
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boost|Camera") float BoostCameraDropZ = 22.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boost|Camera") float BoostFOVGain     =  8.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Boost|Camera") float BoostCameraSpeed =  8.f;
	
	// Rhythm beat punch
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Feedback") float CameraBeatPunch = 3.f;
	// Auto-jump: subtle FOV widen so you feel the sync state physically
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Feedback") float AutoJumpFOVBoost  = 5.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Feedback") float AutoJumpCamSpeed  = 5.f;

private:
	void Input_Move(const FInputActionValue& Value);
	void Input_Look(const FInputActionValue& Value);
	void Input_JumpPressed();
	void Input_JumpReleased();
	void Input_GroundPound();
	void Input_Snap();
	void Input_Fire();
	void TryFire();
	bool IsOnBeat() const;

	UFUNCTION() void OnGameplayBeat(float BeatTimestamp);
	UFUNCTION() void OnActiveBeatAction_Handler();

	void UpdateCameraEffects(float DeltaTime);
	
	float DefaultCameraZ     = 60.f;
	float DefaultFOV         = 90.f;
	float CurrentBoostAlpha    = 0.f;
	float CurrentAutoJumpAlpha = 0.f;
	float PistolCooldown       = 0.f;
	
	float BeatFOVOffset      = 0.f; // Controls the camera pulse
};