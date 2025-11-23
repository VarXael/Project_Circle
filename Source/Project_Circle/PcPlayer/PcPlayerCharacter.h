#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "InputActionValue.h" // Required for Enhanced Input
#include "PcPlayerCharacter.generated.h"

// Forward Declarations
class UCapsuleComponent;
class UCameraComponent;
class UInputMappingContext;
class UInputAction;

UCLASS()
class PROJECT_CIRCLE_API APcPlayerCharacter : public APawn
{
	GENERATED_BODY()

public:
	APcPlayerCharacter();

protected:
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	// --- ENHANCED INPUT VARIABLES ---
	
	// The "Map" of keys (WASD = Move, Mouse = Look)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputMappingContext* DefaultMappingContext;

	// The individual actions
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* MoveAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	UInputAction* LookAction;

	// --- INPUT FUNCTIONS ---
	// Note: They now take "const FInputActionValue& Value"
	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);

private:
	// Physics Helpers
	FVector SlideAlongSurface(const FVector& Velocity, const FVector& Normal);

public:
	// Components
	UPROPERTY(VisibleAnywhere, Category = "Components")
	UCapsuleComponent* CapsuleComp;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	UCameraComponent* CameraComp;

	// Settings
	UPROPERTY(EditAnywhere, Category = "Project Circle")
	FVector SphereCenter = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "Project Circle")
	bool bIsVoidInside = true; 

	UPROPERTY(EditAnywhere, Category = "Project Circle")
	float MoveSpeed = 600.0f;

	UPROPERTY(EditAnywhere, Category = "Project Circle")
	float GravityStrength = 980.0f;

	// State
	FVector Velocity = FVector::ZeroVector;
	FVector CurrentInput = FVector::ZeroVector;
	bool bIsGrounded = false;
};