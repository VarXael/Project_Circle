#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Curves/CurveFloat.h"
#include "PcPlayerConfiguration.h"
#include "PcQPlayerMovementComponent.generated.h"

class APcQEnemyBase;

UENUM(BlueprintType)
enum class EPlayerMovementState : uint8
{
	Grounded       UMETA(DisplayName = "Grounded"),
	InAir          UMETA(DisplayName = "In Air"),
	GroundPounding UMETA(DisplayName = "Ground Pounding"),
	Dashing        UMETA(DisplayName = "Dashing"),
	SwordLunging   UMETA(DisplayName = "Sword Lunging")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnComboEvent,   const FString&, Label, FLinearColor, Color);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSwordHitEnemy, APcQEnemyBase*, Enemy);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnSuperJumped);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDoubleJumped);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDashStarted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDashEnded);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnGroundPulseHit);

UCLASS(Blueprintable, BlueprintType)
class PROJECT_CIRCLE_API UPcQPlayerMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UPcQPlayerMovementComponent();

	UFUNCTION(BlueprintCallable, Category = "Movement") void OnJumpPressed();
	UFUNCTION(BlueprintCallable, Category = "Movement") void OnJumpReleased();
	UFUNCTION(BlueprintCallable, Category = "Movement") void OnGroundPoundPressed();
	
	UFUNCTION(BlueprintCallable, Category = "Beat") void TriggerGroundPulse();
	UFUNCTION(BlueprintCallable, Category = "Combat") void NotifyGunFired(bool bWasOnBeat);
	UFUNCTION(BlueprintCallable, Category = "Combat") void ResetMobilityAbilities();

	UFUNCTION(BlueprintCallable, Category = "Combat") void DoSwordLunge(FVector ViewDirection);
	UFUNCTION(BlueprintCallable, Category = "Combat") void DoSwordBop(bool bIsWallKick);

	UFUNCTION(BlueprintPure) EPlayerMovementState GetMovementState()   const { return MovState; }
	UFUNCTION(BlueprintPure) float                GetHorizontalSpeed() const;
	UFUNCTION(BlueprintPure) bool                 HasDoubleJump()      const { return bDJAvailable; }
	UFUNCTION(BlueprintPure) float                GetDJCooldownAlpha() const;
	UFUNCTION(BlueprintPure) bool                 IsDashing()          const { return MovState == EPlayerMovementState::Dashing; }
	UFUNCTION(BlueprintPure) bool                 IsGroundPounding()   const { return MovState == EPlayerMovementState::GroundPounding; }
	UFUNCTION(BlueprintPure) float                GetDashActiveAlpha() const;
	UFUNCTION(BlueprintPure) float                GetOnBeatFlash()     const;
	UFUNCTION(BlueprintPure) float                GetBeatPhase()       const;
	UFUNCTION(BlueprintPure) float                GetJumpBufferAlpha() const;
	UFUNCTION(BlueprintPure) bool                 GetPulseBuffered()   const { return bPulseBufferedForLanding; }
	UFUNCTION(BlueprintPure) int32                GetOnBeatWindowMs()  const;
	UFUNCTION(BlueprintPure) float                GetGroundPulseBoostAlpha() const; 

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Config")
	TObjectPtr<UPcPlayerConfiguration> Config;

	UPROPERTY(BlueprintAssignable) FOnComboEvent     OnComboEvent;
	UPROPERTY(BlueprintAssignable) FOnSwordHitEnemy  OnSwordHitEnemy;
	UPROPERTY(BlueprintAssignable) FOnSuperJumped    OnSuperJumped;
	UPROPERTY(BlueprintAssignable) FOnDoubleJumped   OnDoubleJumped;
	UPROPERTY(BlueprintAssignable) FOnDashStarted    OnDashStarted;
	UPROPERTY(BlueprintAssignable) FOnDashEnded      OnDashEnded;
	UPROPERTY(BlueprintAssignable) FOnGroundPulseHit OnGroundPulseHit;

protected:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void PhysWalking(float deltaTime, int32 Iterations) override;
	virtual void PhysFalling(float deltaTime, int32 Iterations) override;
	virtual void ProcessLanded(const FHitResult& Hit, float remainingTime, int32 Iterations) override;

private:
	EPlayerMovementState MovState = EPlayerMovementState::Grounded;

	bool  bDJAvailable    = true;
	float DJCooldownTimer = 0.f;

	float DashBoostTimer   = 0.f;
	float DashBoostMaxTime = 0.f;
	float PulseImmunityTimer = 0.f;
	float GroundPulseBoostTimer = 0.f;

	float SwordLungeTimer = 0.f;
	FVector SwordLungeDirection;

	bool  bJumpInputBuffered   = false;
	float JumpInputBufferTimer = 0.f;
	bool  bGPInputBuffered     = false;
	float GPInputBufferTimer   = 0.f;

	bool  bPulseBufferedForLanding = false;
	float PulseBufferTimer         = 0.f;

	float OnBeatFlashTimer    = 0.f;
	float OnBeatFlashDuration = 0.35f;

	bool  bUsingJumpCurve     = false;
	float JumpCurveTimer      = 0.f;
	float JumpCurveTotalTime  = 0.f;
	float JumpCurvePeakHeight = 0.f;
	float PreviousFrameSpeed  = 0.f;

	void DoNormalJump();
	void DoSuperJump();
	void DoDoubleJump();
	void DoFreeDoubleJump(); 
	void DoGroundPound();
	void EnterDash();
	void ExitDash();
	void ApplyArcWithAirTime(float PeakHeightCM, float AirTimeSec);
	void ExitCurveJump();

	bool IsNearBeat()       const;
	bool CanBufferLanding() const;
	void PushCombo(const FString& Label, FLinearColor Color);

	float Cfg_BaseMaxSpeed() const;
	float Cfg_GroundAcceleration() const;
	float Cfg_GroundFriction() const;
	float Cfg_AirAcceleration() const;
	float Cfg_GravityScale() const;
	float Cfg_JumpPeakHeight() const;
	float Cfg_JumpAirTimeBeats() const;
	float Cfg_SuperJumpHorizBoost() const;
	float Cfg_DJPeakHeight() const;
	float Cfg_DJCooldownBeats() const;
	float Cfg_GPSlamSpeed() const;
	float Cfg_GPImmunityBeats() const;
	float Cfg_DashBoostSpeedMult() const;
	float Cfg_DashDurationBeats() const;
	float Cfg_DashSteerAccel() const;
	float Cfg_DashJumpBoost() const;
	float Cfg_PostDashImmunityBeats() const;
	float Cfg_HardSpeedCapMult() const;
	float Cfg_OverspeedDecay() const;
	int32 Cfg_OnBeatWindowMs() const;
	float Cfg_JumpInputBuffer() const;
	UCurveFloat* Cfg_JumpCurve() const;

	// Phase C Configs
	float Cfg_SwordLungeSpeed() const;
	float Cfg_SwordLungeDurationSec() const;
	float Cfg_SwordBopEnemyLift() const;
	float Cfg_SwordBopWallLift() const;
	float Cfg_SwordBopHorizRetain() const;

	float ComputeCurrentMaxSpeed() const;
	float GetCurrentBeatIntervalSec() const;
	float GetSyncedJumpAirTime() const;
};