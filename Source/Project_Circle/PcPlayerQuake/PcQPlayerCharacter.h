#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "PcQPlayerMovementComponent.h"
#include "PcQPlayerCharacter.generated.h"

class UCameraComponent;
class UInputMappingContext;
class UInputAction;
struct FInputActionValue;

UCLASS()
class PROJECT_CIRCLE_API APcQPlayerCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	APcQPlayerCharacter(const FObjectInitializer& ObjectInitializer);

protected:
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) UCameraComponent* CameraComp;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly) UPcQPlayerMovementComponent* MoveComp;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input") UInputMappingContext* DefaultMappingContext;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input") UInputAction* IA_Move;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input") UInputAction* IA_Look;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input") UInputAction* IA_Jump;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input") UInputAction* IA_GroundPound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite) float LookSensitivityX = 0.4f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float LookSensitivityY = 0.4f;

private:
	void Input_Move(const FInputActionValue& Value);
	void Input_Look(const FInputActionValue& Value);
	void Input_JumpPressed();
	void Input_JumpReleased();
	void Input_GroundPound();

	UFUNCTION() void OnGameplayBeat(float BeatTimestamp);
};