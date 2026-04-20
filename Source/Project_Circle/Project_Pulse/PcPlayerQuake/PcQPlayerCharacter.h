#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "PcQPlayerMovementComponent.h"
#include "PcQPlayerCharacter.generated.h"

class UCameraComponent;
class UInputMappingContext;
class UInputAction;
class UPcQHealthComponent;
struct FInputActionValue;

UCLASS()
class PROJECT_CIRCLE_API APcQPlayerCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	APcQPlayerCharacter(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void BeginPlay()  override;
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") UCameraComponent*             CameraComp;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") UPcQPlayerMovementComponent* MoveComp;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") UPcQHealthComponent*          HealthComp;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input") UInputMappingContext* DefaultMappingContext;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input") UInputAction* IA_Move;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input") UInputAction* IA_Look;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input") UInputAction* IA_Jump;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input") UInputAction* IA_GroundPound;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input") UInputAction* IA_Slide;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input") UInputAction* IA_Fire;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat") float BaseDamage    = 25.f;
	// ms window around a beat that counts as "on beat" for shooting bonus
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat") int32 OnBeatWindowMS = 120;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input") float LookSensitivityX = 0.4f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input") float LookSensitivityY = 0.4f;

	// ── Slide camera ─────────────────────────────────────────────────────────
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slide|Camera") float SlideCameraDropZ  = 28.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slide|Camera") float SlideFOVSqueeze   = 10.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Slide|Camera") float SlideCameraSpeed  =  9.f;

private:
	void Input_Move(const FInputActionValue& Value);
	void Input_Look(const FInputActionValue& Value);
	void Input_JumpPressed();
	void Input_JumpReleased();
	void Input_GroundPound();
	void Input_SlidePressed();
	void Input_SlideReleased();
	void Input_Fire();

	// On-beat shooting: character still queries the beat for this opt-in bonus.
	bool IsOnBeat() const;
	void TryFire();

	// Auto-fire when movement component signals an active beat action
	UFUNCTION() void OnActiveBeatAction_Handler();

	void UpdateCameraEffects(float DeltaTime);

	float DefaultCameraZ    = 60.f;
	float DefaultFOV        = 90.f;
	float CurrentSlideAlpha = 0.f;
};