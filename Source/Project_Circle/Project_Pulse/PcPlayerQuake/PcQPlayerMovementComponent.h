#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PcPlayerConfiguration.h"
#include "PcQPlayerMovementComponent.generated.h"

UENUM(BlueprintType)
enum class EPlayerMovementState : uint8
{
	Grounded       UMETA(DisplayName = "Grounded"),
	InAir          UMETA(DisplayName = "In Air"),
	GroundPounding UMETA(DisplayName = "Ground Pounding"),
	Dashing        UMETA(DisplayName = "Dashing") 
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnComboEvent,   const FString&, Label, FLinearColor, Color);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSuperJumped);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDoubleJumped);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDashStarted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDashEnded);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnGroundPulseHit);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMagneticSlam); 

UCLASS(Blueprintable, BlueprintType)
class PROJECT_CIRCLE_API UPcQPlayerMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UPcQPlayerMovementComponent();

	UFUNCTION(BlueprintCallable, Category = "Movement") void OnJumpPressed();
	UFUNCTION(BlueprintCallable, Category = "Movement") void OnJumpReleased();
	UFUNCTION(BlueprintCallable, Category = "Movement") void OnGroundPoundPressed();
	UFUNCTION(BlueprintCallable, Category = "Movement") void EnterDash();
	
	UFUNCTION(BlueprintCallable, Category = "Beat") void TriggerGroundPulse();
	UFUNCTION(BlueprintCallable, Category = "Combat") void NotifyGunFired(bool bWasOnBeat);
	UFUNCTION(BlueprintCallable, Category = "Combat") void ResetMobilityAbilities();

	UFUNCTION(BlueprintPure) EPlayerMovementState GetMovementState()   const { return MovState; }
	UFUNCTION(BlueprintPure) float                GetHorizontalSpeed() const;
	UFUNCTION(BlueprintPure) int32                GetDoubleJumpCharges() const { return CurrentDJCount; }
	UFUNCTION(BlueprintPure) bool                 HasDoubleJump()      const { return CurrentDJCount > 0; }
	UFUNCTION(BlueprintPure) bool                 IsDashing()          const { return MovState == EPlayerMovementState::Dashing; }
	UFUNCTION(BlueprintPure) float                GetDashActiveAlpha() const;
	UFUNCTION(BlueprintPure) float                GetOnBeatFlash()     const;
	UFUNCTION(BlueprintPure) float                GetJumpBufferAlpha() const;
	UFUNCTION(BlueprintPure) int32                GetOnBeatWindowMs()  const;

	// Restored for the HUD:
	UFUNCTION(BlueprintPure) float                GetBeatPhase()       const;
	UFUNCTION(BlueprintPure) float                GetGroundPulseBoostAlpha() const; 
	UFUNCTION(BlueprintPure) bool                 GetPulseBuffered()   const { return bPulseBufferedForLanding; }

	UFUNCTION(BlueprintPure) float GetAdaptiveTime(float IdealTimeSec) const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	TObjectPtr<UPcPlayerConfiguration> Config;

	UPROPERTY(BlueprintAssignable) FOnComboEvent     OnComboEvent;
	UPROPERTY(BlueprintAssignable) FOnSuperJumped    OnSuperJumped;
	UPROPERTY(BlueprintAssignable) FOnDoubleJumped   OnDoubleJumped;
	UPROPERTY(BlueprintAssignable) FOnDashStarted    OnDashStarted;
	UPROPERTY(BlueprintAssignable) FOnDashEnded      OnDashEnded;
	UPROPERTY(BlueprintAssignable) FOnGroundPulseHit OnGroundPulseHit;
	UPROPERTY(BlueprintAssignable) FOnMagneticSlam   OnMagneticSlam; 

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void PhysWalking(float deltaTime, int32 Iterations) override;
	virtual void PhysFalling(float deltaTime, int32 Iterations) override;
	virtual void ProcessLanded(const FHitResult& Hit, float remainingTime, int32 Iterations) override;

private:
	EPlayerMovementState MovState = EPlayerMovementState::Grounded;

	int32 CurrentDJCount = 2;
	float DashBoostTimer   = 0.f;
	float DashBoostMaxTime = 0.f;
	float PulseImmunityTimer = 0.f;
	float GroundPulseBoostTimer = 0.f;

	float LastDashTime = -1.f;
	float LastJumpTime = -1.f;

	bool  bJumpInputBuffered   = false;
	float JumpInputBufferTimer = 0.f;
	bool  bGPInputBuffered     = false;
	float GPInputBufferTimer   = 0.f;
	bool  bPulseBufferedForLanding = false;
	float PulseBufferTimer         = 0.f;
	float OnBeatFlashTimer    = 0.f;
	float OnBeatFlashDuration = 0.35f;

	void ExecuteDashJump();
	void ExitDash();
	void DoNormalJump();
	void DoDoubleJump();
	void DoGroundPound(); 
	void ApplyJumpVelocity(float AirTimeSec);

	bool IsNearBeat()       const;
	bool CanBufferLanding() const;
	void PushCombo(const FString& Label, FLinearColor Color);

	float ComputeRhythmGravity() const;
	float GetSmoothScaledTime(float BaseTimeSec) const; 
	float ComputeCurrentMaxSpeed() const;
	float GetCurrentBeatIntervalSec() const;
};