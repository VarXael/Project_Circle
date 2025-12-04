#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "PcPlayerCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;
class APcWeapon;
class UPcGravityMovementComponent; 

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
	UFUNCTION(BlueprintCallable)
	void Input_Move(FVector2D Value);
	
	UFUNCTION(BlueprintCallable)
	void Input_Look(FVector2D Value); // The logic changes here
	
	UFUNCTION(BlueprintCallable)
	void Input_JumpTrigger();
	
	UFUNCTION(BlueprintCallable)
	void Input_StartAttack();
	UFUNCTION(BlueprintCallable)
	void Input_StopAttack();
	UFUNCTION(BlueprintCallable)
	void Input_FireLaser(); // KEPT LASER
	UFUNCTION(BlueprintCallable)
	void TakeHit();

	// --- HUD GETTERS ---
	UFUNCTION(BlueprintCallable, Category = "Flow")
	float GetFuseFraction() const;
	UFUNCTION(BlueprintCallable, Category = "Flow")
	int32 GetFlowStacks() const { return FlowStacks; }
	UFUNCTION(BlueprintCallable, Category = "Flow")
	float GetCurrentSpeed() const { return CurrentSpeed; }
	UFUNCTION(BlueprintCallable, Category = "Flow")
	float GetTargetMaxSpeed() const { return BaseMoveSpeed + (FlowStacks * BonusSpeedPerStack); }
	UFUNCTION(BlueprintCallable, Category = "Debug")
	bool IsInRhythmWindow() const { return bInPerfectWindow; }
	UFUNCTION(BlueprintCallable, Category = "Debug")
	FString GetDebugInfo() const;

public:
	// --- COMPONENTS ---
	UPROPERTY(VisibleAnywhere, Category = "Components")
	USpringArmComponent* SpringArmComp;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	UCameraComponent* CameraComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UPcGravityMovementComponent* GravityComp;

	UPROPERTY(EditAnywhere, Category = "Components")
	TSubclassOf<APcWeapon> StartingWeaponClass;
	
	UPROPERTY(BlueprintReadOnly, Category = "Components")
	APcWeapon* CurrentWeapon;

	// --- CONFIG ---
	UPROPERTY(EditAnywhere, Category = "Movement")
	float BaseMoveSpeed = 800.0f;
	UPROPERTY(EditAnywhere, Category = "Movement")
	float MaxSkimSpeed = 2500.0f; 
	UPROPERTY(EditAnywhere, Category = "Movement")
	float BonusSpeedPerStack = 250.0f;
	UPROPERTY(EditAnywhere, Category = "Movement")
	float SteeringRate = 300.0f; 
	UPROPERTY(EditAnywhere, Category = "Movement")
	float CarveAcceleration = 600.0f; 
	UPROPERTY(EditAnywhere, Category = "Movement")
	float PassiveDrag = 20.0f; 

	UPROPERTY(EditAnywhere, Category = "Jump")
	float JumpPeakHeight = 200.0f; 
	UPROPERTY(EditAnywhere, Category = "Jump")
	float JumpImpulse = 400.0f; 
	UPROPERTY(EditAnywhere, Category = "Jump")
	float WaveDuration = 0.6f; 

	UPROPERTY(EditAnywhere, Category = "Flow")
	float FuseDuration = 2.0f;
	UPROPERTY(EditAnywhere, Category = "Flow")
	int32 MaxStacks = 3;
	UPROPERTY(EditAnywhere, Category = "Flow")
	float PerfectWindowDuration = 0.2f;
	UPROPERTY(EditAnywhere, Category = "Flow")
	float InputBufferAllowance = 0.15f;

private:
	// --- INTERNAL STATE ---
	FVector CurrentInput = FVector::ZeroVector;
	float CurrentSpeed = 0.0f;
	
	// CAMERA STATE
	float CameraPitch = 0.0f; // Track looking up/down locally

	int32 FlowStacks = 0;
	float FuseTimer = 0.0f;
	float SpeedLockTimer = 0.0f; 
	bool bIsInvulnerable = false; 
	bool bIsJumping = false;
	bool bInPerfectWindow = false; 
	float JumpPhaseTime = 0.0f;    
	float WindowTimer = 0.0f;      
	float InputBufferTimer = 0.0f; 

	void UpdateSkaterPhysics(float DeltaTime);
	void UpdateJumpLogic(float DeltaTime);
	void UpdateFlowFuse(float DeltaTime);
	void PerformJump(bool bIsPerfect);
	void PushStyleMessage(FString Msg, uint8 Type);
};