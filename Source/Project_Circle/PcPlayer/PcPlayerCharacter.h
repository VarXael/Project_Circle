// ==========================================
// FILE: PcPlayerCharacter.h
// PATH: E:\GameDev\Unreal Engine Projects\Project_Circle\Source\Project_Circle\PcPlayer\PcPlayerCharacter.h
// ==========================================
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "PcPlayerCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;
class APcWeapon;
class UPcGravityMovementComponent; 
class UPcFlowMechanicComponent;
class UNiagaraComponent;
class UNiagaraSystem;
class UCameraShakeBase;

UCLASS()
class PROJECT_CIRCLE_API APcPlayerCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	APcPlayerCharacter();

protected:
	virtual void Tick(float DeltaTime) override;
	virtual void BeginPlay() override;

public:
	// --- INPUTS ---
	UFUNCTION(BlueprintCallable) void Input_Move(FVector2D Value);
	UFUNCTION(BlueprintCallable) void Input_Look(FVector2D Value);
	UFUNCTION(BlueprintCallable) void Input_JumpTrigger();
	
	UFUNCTION(BlueprintCallable) void Input_StartDrift();
	UFUNCTION(BlueprintCallable) void Input_StopDrift();
	
	UFUNCTION(BlueprintCallable) void Input_StartAttack();
	UFUNCTION(BlueprintCallable) void Input_StopAttack();
	UFUNCTION(BlueprintCallable) void Input_FireLaser(); 
	UFUNCTION(BlueprintCallable) void TakeHit();

	UFUNCTION(BlueprintCallable, Category = "Flow")
	float GetCurrentSpeed() const;

	// --- COMPONENTS ---
	UPROPERTY(VisibleAnywhere, Category = "Components")
	USpringArmComponent* SpringArmComp;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	UCameraComponent* CameraComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UPcGravityMovementComponent* GravityComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UPcFlowMechanicComponent* FlowComp;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	UNiagaraComponent* DriftSparksComp;

	UPROPERTY(EditAnywhere, Category = "Components")
	TSubclassOf<APcWeapon> StartingWeaponClass;
	
	UPROPERTY(BlueprintReadOnly, Category = "Components")
	APcWeapon* CurrentWeapon;

	// --- VISUAL FEEDBACK CONFIG ---
	UPROPERTY(EditAnywhere, Category = "Feedback|FOV")
	float BaseFOV = 90.0f;
	UPROPERTY(EditAnywhere, Category = "Feedback|FOV")
	float FOVPerTier = 10.0f; 
	UPROPERTY(EditAnywhere, Category = "Feedback|FOV")
	float BoostFOVImpulse = 10.0f; 

	UPROPERTY(EditAnywhere, Category = "Feedback|Camera")
	float LandingSinkAmount = 60.0f; 
	UPROPERTY(EditAnywhere, Category = "Feedback|Camera")
	float DriftCameraSinkAmount = 30.0f; 
	
	// NEW: How fast the camera catches up vertically (Lower = Smoother/Laggier)
	UPROPERTY(EditAnywhere, Category = "Feedback|Camera")
	float VerticalCameraLagSpeed = 15.0f; 

	UPROPERTY(EditAnywhere, Category = "Feedback|Shake")
	TSubclassOf<UCameraShakeBase> LandingShake;
	
	UPROPERTY(EditAnywhere, Category = "Feedback|VFX")
	UNiagaraSystem* JumpLaunchFX; 

	// --- PHYSICS CONFIG ---
	UPROPERTY(EditAnywhere, Category = "Movement|Base")
	float BaseMoveSpeed = 800.0f;
	UPROPERTY(EditAnywhere, Category = "Movement|Base")
	float SpeedPerTier = 600.0f; 

	UPROPERTY(EditAnywhere, Category = "Movement|Grip")
	float GripSteeringRate = 300.0f; 

	// Rotational Momentum Settings
	UPROPERTY(EditAnywhere, Category = "Movement|Drift")
	float RotationalDrag = 2.0f; 
	UPROPERTY(EditAnywhere, Category = "Movement|Drift")
	float LandingSpinBoost = 5.0f; 
	UPROPERTY(EditAnywhere, Category = "Movement|Drift")
	float DriftAcceleration = 1500.0f; 

	// --- JUMP CONFIG (SINE WAVE) ---
	UPROPERTY(EditAnywhere, Category = "Jump")
	float JumpPeakHeight = 200.0f; // Height in Units
	
	UPROPERTY(EditAnywhere, Category = "Jump")
	float WaveDuration = 0.6f; // Time in Seconds (Matches Beat)
	
	UPROPERTY(EditAnywhere, Category = "Jump")
	float LandComboWindow = 0.25f; 

	// --- DEBUG ---
	FVector DebugLastVelocityDir;
	FVector DebugLastInputDir;
	float DebugSlipAngle; 

private:
	// --- INTERNAL STATE ---
	FVector CurrentInput = FVector::ZeroVector;
	float CurrentSpeed = 0.0f;
	float CameraPitch = 0.0f; 

	// Visuals State
	float CurrentFOVMod = 0.0f; 
	float FOVImpulse = 0.0f;    
	
	// Steadycam State (To hide Sine Wave Snap)
	float SmoothedCameraHeight = 0.0f; // Tracks relative height
	float TargetCameraSink = 0.0f;     // The "Dunk" target

	// Physics State
	float CurrentAngularVelocity = 0.0f;
	bool bIsDrifting = false;
	
	// Jump Logic
	bool bIsJumping = false;
	float JumpPhaseTime = 0.0f;
	
	float LandWindowTimer = 0.0f;      
	bool bCanComboLand = false;

	// Input Buffer
	float InputBufferTimer = 0.0f;

	// Internal Functions
	void UpdateSkaterPhysics(float DeltaTime);
	void UpdateJumpLogic(float DeltaTime);
	void UpdateVisuals(float DeltaTime); 
	
	void PerformJump();
	void OnLandedHit();
};