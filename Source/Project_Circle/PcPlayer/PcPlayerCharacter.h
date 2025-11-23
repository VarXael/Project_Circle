#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "PcPlayerCharacter.generated.h"

class UCameraComponent;
class APcPlanet;

UCLASS()
class PROJECT_CIRCLE_API APcPlayerCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	APcPlayerCharacter();

protected:
	virtual void Tick(float DeltaTime) override;
	virtual void NotifyActorBeginOverlap(AActor* OtherActor) override;
	virtual void NotifyActorEndOverlap(AActor* OtherActor) override;

public:
	UFUNCTION(BlueprintCallable) void Input_Move(FVector2D Value);
	UFUNCTION(BlueprintCallable) void Input_Look(FVector2D Value);
	UFUNCTION(BlueprintCallable) void Input_Jump();

public:
	UPROPERTY(VisibleAnywhere, Category = "Components")
	UCameraComponent* CameraComp;

	UPROPERTY(VisibleAnywhere, Category = "Gravity")
	APcPlanet* CurrentPlanet;

	// --- PHYSICS SETTINGS ---

	UPROPERTY(EditAnywhere, Category = "Water Physics")
	float MoveAcceleration = 1500.0f;

	UPROPERTY(EditAnywhere, Category = "Water Physics")
	float MaxSurfSpeed = 2000.0f;

	// How strongly the floor repels you (The Bounce)
	UPROPERTY(EditAnywhere, Category = "Water Physics")
	float BuoyancyStiffness = 1000.0f;

	// How quickly the bounce settles (Prevents infinite wobble)
	UPROPERTY(EditAnywhere, Category = "Water Physics")
	float BuoyancyDamping = 5.0f;

	// Friction when moving SLOW (Sticky)
	UPROPERTY(EditAnywhere, Category = "Water Physics")
	float FrictionLowSpeed = 4.0f;

	// Friction when moving FAST (Surfing)
	UPROPERTY(EditAnywhere, Category = "Water Physics")
	float FrictionHighSpeed = 0.2f;

private:
	// We manage our own velocity now
	FVector Velocity = FVector::ZeroVector;
	FVector CurrentInput = FVector::ZeroVector;
};