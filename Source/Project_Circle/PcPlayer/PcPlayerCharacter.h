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
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void NotifyActorBeginOverlap(AActor* OtherActor) override;
	virtual void NotifyActorEndOverlap(AActor* OtherActor) override;

public:
	// --- INPUTS ---
	UFUNCTION(BlueprintCallable) void Input_Move(FVector2D Value);
	UFUNCTION(BlueprintCallable) void Input_Look(FVector2D Value);
	UFUNCTION(BlueprintCallable) void Input_JumpTrigger(); 
	UFUNCTION(BlueprintCallable) void Input_StartAttack();
	UFUNCTION(BlueprintCallable) void Input_StopAttack();

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
	// CONFIGURATION
	// ==============================================================================

	UPROPERTY(EditAnywhere, Category = "Project Circle | 1. Movement")
	float BaseMoveSpeed = 800.0f; 

	// --- GROUND PHYSICS (The Water) ---
	UPROPERTY(EditAnywhere, Category = "Project Circle | 1. Movement")
	float GroundDrag = 15.0f; // Heavy friction on floor

	UPROPERTY(EditAnywhere, Category = "Project Circle | 1. Movement")
	float GroundAccel = 15.0f; 

	// --- AIR PHYSICS (The Glide) ---
	// Lower numbers = More slidey, preserving momentum
	UPROPERTY(EditAnywhere, Category = "Project Circle | 1. Movement")
	float AirDrag = 2.0f; 

	UPROPERTY(EditAnywhere, Category = "Project Circle | 1. Movement")
	float AirAccel = 5.0f; 


	// --- 2. THE WAVE ---
	UPROPERTY(EditAnywhere, Category = "Project Circle | 2. Wave Jump")
	float JumpHeight = 350.0f;

	UPROPERTY(EditAnywhere, Category = "Project Circle | 2. Wave Jump")
	float SinkDepth = 60.0f;

	UPROPERTY(EditAnywhere, Category = "Project Circle | 2. Wave Jump")
	float WaveDuration = 0.8f;

	// --- 3. BUNNY HOP ---
	UPROPERTY(EditAnywhere, Category = "Project Circle | 3. Bunny Hop")
	float SpeedBonusPerHop = 400.0f;

	UPROPERTY(EditAnywhere, Category = "Project Circle | 3. Bunny Hop")
	float MaxBoostSpeed = 2500.0f;

	UPROPERTY(EditAnywhere, Category = "Project Circle | 3. Bunny Hop")
	float CoyoteThreshold = 0.25f; // Generous window

	// --- 4. COMBAT ---
	UPROPERTY(EditAnywhere, Category = "Project Circle | 4. Combat")
	TSubclassOf<APcProjectile> ProjectileClass;

	UPROPERTY(EditAnywhere, Category = "Project Circle | 4. Combat")
	float FireRate = 0.12f;

private:
	FVector HorizontalVelocity = FVector::ZeroVector;
	FVector CurrentInput = FVector::ZeroVector;
	float CurrentSpeedCap = 0.0f;

	bool bIsWaveActive = false;
	float WavePhase = 0.0f;         
	float SmoothedAltitude = 0.0f;

	FTimerHandle TimerHandle_Attack;
	
	// Helper for auto-fire
	void FireProjectile(); 

	void ApplyDeterministicMovement(float DeltaTime);
};