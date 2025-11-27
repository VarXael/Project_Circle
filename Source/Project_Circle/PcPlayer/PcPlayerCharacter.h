#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "PcPlayerCharacter.generated.h"

class UCameraComponent;
class APcPlanet;
class APcWeapon; // Forward Declaration

UCLASS()
class PROJECT_CIRCLE_API APcPlayerCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	APcPlayerCharacter();

protected:
	virtual void Tick(float DeltaTime) override;
	virtual void BeginPlay() override;
	virtual void NotifyActorBeginOverlap(AActor* OtherActor) override;
	virtual void NotifyActorEndOverlap(AActor* OtherActor) override;

public:
	UFUNCTION(BlueprintCallable)
	void Input_Move(FVector2D Value);
	UFUNCTION(BlueprintCallable)
	void Input_Look(FVector2D Value);
	UFUNCTION(BlueprintCallable)
	void Input_JumpTrigger();

	// --- COMBAT INPUTS ---
	UFUNCTION(BlueprintCallable)
	void Input_StartAttack();
	UFUNCTION(BlueprintCallable)
	void Input_StopAttack();
	UFUNCTION(BlueprintCallable)
	void Input_FireLaser();

	UFUNCTION(BlueprintCallable)
	void TakeHit();
	UFUNCTION(BlueprintImplementableEvent)
	void OnHitReceived();

	// Debug
	UFUNCTION(BlueprintCallable, Category = "Debug")
	FString GetDebugInfo() const;
	UFUNCTION(BlueprintCallable, Category = "Debug")
	bool IsInRhythmWindow() const;

public:
	UPROPERTY(VisibleAnywhere, Category = "Components")
	UCameraComponent* CameraComp;

	UPROPERTY(VisibleAnywhere, Category = "Gravity")
	APcPlanet* CurrentPlanet;

	// ==============================================================================
	// WEAPON SYSTEM
	// ==============================================================================
	UPROPERTY(EditAnywhere, Category = "Project Circle | Combat")
	TSubclassOf<APcWeapon> StartingWeaponClass;

	UPROPERTY(BlueprintReadOnly, Category = "Project Circle | Combat")
	APcWeapon* CurrentWeapon;

	// ==============================================================================
	// MOVEMENT CONFIGURATION
	// ==============================================================================
	UPROPERTY(EditAnywhere, Category = "Project Circle | 1. Movement")
	float BaseMoveSpeed = 600.0f;
	UPROPERTY(EditAnywhere, Category = "Project Circle | 1. Movement")
	float MaxSkimSpeed = 2500.0f;

	UPROPERTY(EditAnywhere, Category = "Project Circle | 2. Physics")
	float MinSteeringRate = 120.0f;
	UPROPERTY(EditAnywhere, Category = "Project Circle | 2. Physics")
	float MaxSteeringRate = 400.0f;
	UPROPERTY(EditAnywhere, Category = "Project Circle | 2. Physics")
	float CarveAcceleration = 1200.0f;
	UPROPERTY(EditAnywhere, Category = "Project Circle | 2. Physics")
	float BaseDrag = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Project Circle | 3. Jump")
	float MinJumpHeight = 150.0f;
	UPROPERTY(EditAnywhere, Category = "Project Circle | 3. Jump")
	float MaxJumpHeight = 500.0f;
	UPROPERTY(EditAnywhere, Category = "Project Circle | 3. Jump")
	float JumpBoostAmount = 800.0f;
	UPROPERTY(EditAnywhere, Category = "Project Circle | 3. Jump")
	float WaveDuration = 0.8f;
	UPROPERTY(EditAnywhere, Category = "Project Circle | 3. Jump")
	float CoyoteThreshold = 0.25f;

	UPROPERTY(EditAnywhere, Category = "Project Circle | 4. Buoyancy")
	float MudDepth = 80.0f;
	UPROPERTY(EditAnywhere, Category = "Project Circle | 4. Buoyancy")
	float LiftSensitivity = 3.0f;

	UPROPERTY(EditAnywhere, Category = "Project Circle | 2. Physics")
	float StraightLineDrag = 800.0f;
	
	UPROPERTY(EditAnywhere, Category = "Project Circle | 2. Physics")
	float MomentumMultiplier = 800.0f;

private:
	FVector HorizontalVelocity = FVector::ZeroVector;
	FVector CurrentInput = FVector::ZeroVector;
	float CurrentSpeed = 0.0f;
	float CarveIntensity = 0.0f;
	//float CurrentSteeringRate = 0.0f;

	bool bIsWaveActive = false;
	float WavePhase = 0.0f;
	float SmoothedAltitude = 0.0f;
	float CurrentJumpPeak = 0.0f;

	void ApplySkaterMovement(float DeltaTime);
};
