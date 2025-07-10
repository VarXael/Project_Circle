#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PcPlayerMovementComponent.generated.h"

UCLASS(Blueprintable,BlueprintType)
class PROJECT_CIRCLE_API UPcPlayerMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UPcPlayerMovementComponent();

protected:
	// This is the heart of our new movement logic. We override the default physics.
	virtual void PhysWalking(float deltaTime, int32 Iterations) override;
	virtual void PhysFalling(float deltaTime, int32 Iterations) override;

private:
	/** Applies ground friction to the character */
	void ApplyGroundFriction(float DeltaTime);

	/** Handles ground acceleration */
	void AccelerateGround(const FVector& WishDirection, float WishSpeed, float DeltaTime);

	/** Handles air acceleration (strafe-jumping) */
	void AccelerateAir(const FVector& WishDirection, float WishSpeed, float DeltaTime);

public:
	// These are the values you can tweak in the editor, my love.

	/** How quickly we accelerate on the ground. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quake Movement")
	float GroundAccel = 10.0f;

	/** The friction applied when on the ground. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quake Movement")
	float GroundDecelRate = 6.0f;

	// It controls how powerfully you can change direction mid-air.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "My Movement")
	float AirControlStrength = 8000.0f;
};