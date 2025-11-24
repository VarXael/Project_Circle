#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "PcPlayerCharacter.generated.h"

class UCameraComponent;
class APcPlanet;
class APcProjectile;

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
	// ==============================================================================
	// INPUT INTERFACE
	// ==============================================================================
	
	UFUNCTION(BlueprintCallable) void Input_Move(FVector2D Value);
	UFUNCTION(BlueprintCallable) void Input_Look(FVector2D Value);
	UFUNCTION(BlueprintCallable) void Input_JumpStart(); 
	UFUNCTION(BlueprintCallable) void Input_JumpStop();
	UFUNCTION(BlueprintCallable) void Input_PrimaryAttack();

	// --- DEBUG INTERFACE FOR HUD ---
	UFUNCTION(BlueprintCallable, Category = "Debug")
	FString GetDebugInfo() const;

	UFUNCTION(BlueprintCallable, Category = "Debug")
	bool IsInRhythmWindow() const;

public:
	// ==============================================================================
	// COMPONENTS
	// ==============================================================================
	
	UPROPERTY(VisibleAnywhere, Category = "Components")
	UCameraComponent* CameraComp;

	UPROPERTY(VisibleAnywhere, Category = "Gravity")
	APcPlanet* CurrentPlanet;

	// ==============================================================================
	// CONFIGURATION
	// ==============================================================================

	// --- 1. MOVEMENT BASICS ---
	UPROPERTY(EditAnywhere, Category = "Project Circle | 1. Movement")
	float BaseMoveSpeed = 800.0f; 

	UPROPERTY(EditAnywhere, Category = "Project Circle | 1. Movement")
	float GroundAcceleration = 8.0f; 

	UPROPERTY(EditAnywhere, Category = "Project Circle | 1. Movement")
	float AirAcceleration = 2.0f;    

	UPROPERTY(EditAnywhere, Category = "Project Circle | 1. Movement")
	float Deceleration = 10.0f;


	// --- 2. THE WAVE (Jump Arc) ---
	UPROPERTY(EditAnywhere, Category = "Project Circle | 2. Wave Jump")
	float MaxJumpHeight = 350.0f;

	UPROPERTY(EditAnywhere, Category = "Project Circle | 2. Wave Jump")
	float SinkDepth = 60.0f;

	UPROPERTY(EditAnywhere, Category = "Project Circle | 2. Wave Jump")
	float WaveDuration = 0.9f;


	// --- 3. BUNNY HOP (Rhythm System) ---
	
	/** Speed gained per successful perfect jump. */
	UPROPERTY(EditAnywhere, Category = "Project Circle | 3. Bunny Hop")
	float SpeedBonusPerHop = 400.0f;

	/** Maximum possible speed. */
	UPROPERTY(EditAnywhere, Category = "Project Circle | 3. Bunny Hop")
	float MaxBoostSpeed = 2500.0f;

	/** How much snappy acceleration we regain as we get faster. */
	UPROPERTY(EditAnywhere, Category = "Project Circle | 3. Bunny Hop")
	float MaxControlBonus = 25.0f; 

	/** How early (percentage) before the bottom of the sink can we press Jump? */
	UPROPERTY(EditAnywhere, Category = "Project Circle | 3. Bunny Hop")
	float CoyoteThreshold = 0.20f; 

	// --- 4. COMBAT ---
	UPROPERTY(EditAnywhere, Category = "Project Circle | 4. Combat")
	TSubclassOf<APcProjectile> ProjectileClass;

private:
	// --- STATE ---
	FVector HorizontalVelocity = FVector::ZeroVector;
	FVector CurrentInput = FVector::ZeroVector;
	
	// Dynamic Stats
	float CurrentSpeedCap = 0.0f;

	// Wave Logic
	bool bIsWaveActive = false;
	bool bIsHoldingJump = false;
	float WavePhase = 0.0f;         
	float CurrentJumpPeak = 0.0f;   

	void ApplyDeterministicMovement(float DeltaTime);
};