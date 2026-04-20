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
	
	// NEW: The shoot action
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input") UInputAction* IA_Fire;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat") float BaseDamage = 25.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")  float LookSensitivityX = 0.4f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")  float LookSensitivityY = 0.4f;

	// ── Slide camera ─────────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slide|Camera") float SlideCameraDropZ  = 25.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slide|Camera") float SlideFOVSqueeze   =  8.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slide|Camera") float SlideCameraSpeed  =  8.f;

private:
	void Input_Move(const FInputActionValue& Value);
	void Input_Look(const FInputActionValue& Value);
	void Input_JumpPressed();
	void Input_JumpReleased();
	void Input_GroundPound();
	void Input_Fire();
	void TryFire();   // shared logic for manual and auto-fire
	bool IsOnBeat() const;

	UFUNCTION() void OnGameplayBeat(float BeatTimestamp);
	UFUNCTION() void OnActiveBeatAction_Handler();

	void UpdateCameraEffects(float DeltaTime);

	float DefaultCameraZ    = 60.f;
	float DefaultFOV        = 90.f;
	float CurrentSlideAlpha = 0.f;
};