#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Project_Circle/MusicSystem/MusicImportSystem/PcMusicAnalysisTypes.h"
#include "PcPlayerConfiguration.h"
#include "PcQPlayerMovementComponent.generated.h"

UENUM(BlueprintType)
enum class EBhopState : uint8 { Normal, DashBoosting, AirHopping, GroundPounding, WallSwim };

UENUM(BlueprintType)
enum class ESnapAction : uint8 { None, Jump, AirHop, DashBoost };

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBhopChargeUpdated, float, ChargeAlpha);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBhopLanded, float, HorizontalSpeed);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWallSwimChanged, float, SwimAlpha);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnActiveBeatAction);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnComboEvent, const FString&, Label, FLinearColor, Color);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSnapStateChanged, ESnapAction, NewAction);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSnapPulse, ESnapAction, SnapAction);

USTRUCT(BlueprintType)
struct PROJECT_CIRCLE_API FPcRhythmAbility
{
	GENERATED_BODY()

	float CooldownBeats = 1.0f;
	int32 MaxCharges = 1;
	bool bBeatSnapped = false;

	int32 CurrentCharges = 1;
	float CooldownTimer = 0.f;
	float LastCalculatedCooldownDuration = 0.1f;

	void Initialize(float InCooldownBeats, int32 InMaxCharges, bool bInBeatSnapped)
	{
		CooldownBeats = InCooldownBeats;
		MaxCharges = InMaxCharges;
		bBeatSnapped = bInBeatSnapped;
		CurrentCharges = MaxCharges;
		CooldownTimer = 0.f;
	}

	void Tick(float DeltaTime)
	{
		if (CurrentCharges < MaxCharges)
		{
			CooldownTimer -= DeltaTime;
			if (CooldownTimer <= 0.f)
			{
				CurrentCharges = MaxCharges;
				CooldownTimer = 0.f;
			}
		}
	}

	bool TryActivate(float BaseDurationSec, float SnappedDurationSec)
	{
		if (CurrentCharges > 0)
		{
			CurrentCharges--;
			CooldownTimer = bBeatSnapped ? SnappedDurationSec : BaseDurationSec;
			LastCalculatedCooldownDuration = FMath::Max(CooldownTimer, 0.001f);
			return true;
		}
		return false;
	}

	void ResetCharges()
	{
		CurrentCharges = MaxCharges;
		CooldownTimer = 0.f;
	}

	float GetCooldownAlpha() const
	{
		if (CurrentCharges == MaxCharges || LastCalculatedCooldownDuration <= 0.001f) return 0.f;
		return FMath::Clamp(CooldownTimer / LastCalculatedCooldownDuration, 0.f, 1.f);
	}
};

UCLASS(Blueprintable, BlueprintType)
class PROJECT_CIRCLE_API UPcQPlayerMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UPcQPlayerMovementComponent();

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void PhysWalking(float deltaTime, int32 Iterations) override;
	virtual void PhysFalling(float deltaTime, int32 Iterations) override;
	virtual void ProcessLanded(const FHitResult& Hit, float remainingTime, int32 Iterations) override;

public:
	UFUNCTION(BlueprintCallable, Category = "Movement") void OnJumpPressed();
	UFUNCTION(BlueprintCallable, Category = "Movement") void OnJumpReleased();
	UFUNCTION(BlueprintCallable, Category = "Movement") void OnGroundPoundPressed();
	UFUNCTION(BlueprintCallable, Category = "Movement") void OnGroundPoundReleased();
	UFUNCTION(BlueprintCallable, Category = "Movement") void OnDashPressed();
	UFUNCTION(BlueprintCallable, Category = "Movement") void OnAirHopPressed();
	UFUNCTION(BlueprintCallable, Category = "Movement") void NotifyGunFired();
	
	UFUNCTION(BlueprintCallable, Category = "Movement") void TriggerBeatJump();
	UFUNCTION(BlueprintCallable, Category = "Movement") void TriggerFrenzyDashBoost();

	// ── State Getters ────────────────────────────────────────────────────────
	UFUNCTION(BlueprintPure) EBhopState  GetBhopState()       const { return BhopState; }
	UFUNCTION(BlueprintPure) float       GetHorizontalSpeed() const { return FVector(Velocity.X, Velocity.Y, 0.f).Size(); }
	UFUNCTION(BlueprintPure) bool        IsInBhopChain()      const { return GetHorizontalSpeed() > MaxWalkSpeed * 1.05f; }
	UFUNCTION(BlueprintPure) bool        IsWallSwimming()     const { return BhopState == EBhopState::WallSwim; }
	UFUNCTION(BlueprintPure) bool        IsPowerBoosting()    const { return BhopState == EBhopState::DashBoosting || BhopState == EBhopState::AirHopping; }
	
	UFUNCTION(BlueprintPure) float       GetPlayerBPM()       const { return PlayerBPM; }
	UFUNCTION(BlueprintPure) ESnapAction GetActiveSnap()      const;
	UFUNCTION(BlueprintPure) float       GetSnapPulseFlash()  const { return SnapPulseTimer > 0.f ? FMath::Clamp(SnapPulseTimer / 0.15f, 0.f, 1.f) : 0.f; }
	UFUNCTION(BlueprintPure) float       GetPlayerPulse()     const { return PlayerPulse; }
	UFUNCTION(BlueprintPure) float       GetBeatSnappedDuration(float BaseSec) const;

	// ── Frenzy ───────────────────────────────────────────────────────────────
	UFUNCTION(BlueprintPure) float GetFrenzyGauge()              const { return FrenzyGauge; }
	UFUNCTION(BlueprintPure) float GetFrenzySpeedMult()          const;
	UFUNCTION(BlueprintPure) bool  IsSTierActive()               const { return bSTierActive; }
	UFUNCTION(BlueprintPure) bool  IsSTierLocked()               const { return bSTierLocked; }
	UFUNCTION(BlueprintPure) int32 GetLastBeatGrantTimestampMS() const { return LastBeatResetTimestampMS; }

	// ── UI Getters ───────────────────────────────────────────────────────────
	UFUNCTION(BlueprintPure) float GetFrenzyThresh_C() const { return MoveConfig ? MoveConfig->TierC_Threshold : 0.25f; }
	UFUNCTION(BlueprintPure) float GetFrenzyThresh_B() const { return MoveConfig ? MoveConfig->TierB_Threshold : 0.50f; }
	UFUNCTION(BlueprintPure) float GetFrenzyThresh_A() const { return MoveConfig ? MoveConfig->TierA_Threshold : 0.75f; }
	UFUNCTION(BlueprintPure) float GetFrenzyThresh_S() const { return MoveConfig ? MoveConfig->TierS_Threshold : 1.00f; }

	UFUNCTION(BlueprintPure) float GetAirHopCooldownAlpha()     const { return Ability_AirHop.GetCooldownAlpha(); }
	UFUNCTION(BlueprintPure) float GetDashBoostAlpha()          const { return Ability_DashBoost.GetCooldownAlpha(); }
	UFUNCTION(BlueprintPure) float GetOnBeatFlash()             const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement Config")
	TObjectPtr<UPcPlayerConfiguration> MoveConfig;

	UPROPERTY(BlueprintReadOnly, Category="Abilities") FPcRhythmAbility Ability_AirHop;
	UPROPERTY(BlueprintReadOnly, Category="Abilities") FPcRhythmAbility Ability_DashBoost;

	UPROPERTY(BlueprintAssignable) FOnBhopChargeUpdated OnBhopChargeUpdated;
	UPROPERTY(BlueprintAssignable) FOnBhopLanded        OnBhopLanded;
	UPROPERTY(BlueprintAssignable) FOnWallSwimChanged   OnWallSwimChanged;
	UPROPERTY(BlueprintAssignable) FOnActiveBeatAction  OnActiveBeatAction;
	UPROPERTY(BlueprintAssignable) FOnComboEvent        OnComboEvent;
	UPROPERTY(BlueprintAssignable) FOnSnapStateChanged  OnSnapStateChanged;
	UPROPERTY(BlueprintAssignable) FOnSnapPulse         OnSnapPulse;

private:
	void ExecuteNormalJump(bool bOnBeat);
	void ExecuteAirHop(bool bOnBeat);
	void ExecuteSuperJump(bool bOnBeat);
	void ExecuteDashBoost(bool bOnBeat);
	void EnterGroundPound();
	
	void ApplyArcWithAirTime(float PeakHeightCM, float AirTimeSec);
	bool RegisterBeatAction(const FString& ActionName, FLinearColor Color, bool bRequireBeat = false);
	void TriggerOnBeatFlash();

	bool  IsOnBeat() const;
	float GetGameplayBeatIntervalSec() const;
	void  RecordHit();

	EBhopState BhopState = EBhopState::Normal;

	// ── Curve Driven Dash State ──
	FVector ActiveDashDir = FVector::ZeroVector;
	float   ActiveDashPeakSpeed = 0.f;
	float   ActiveDashDuration = 0.f;
	float   ActiveDashTimer = 0.f;
	
	bool  bJumpQueuedForBeat   = false;
	float BeatQueueTimer       = 0.f;
	bool  bJumpInputBuffered   = false;
	float JumpInputBufferTimer = 0.f;

	float GPLandingWindowTimer = 0.f;
	float OnBeatFlashTimer = 0.f;
	float SnapPulseTimer   = 0.f;

	float FrenzyGauge     = 0.f;
	bool  bSTierActive    = false;
	bool  bSTierLocked    = false;
	float STierLockTimer  = 0.f;
	float PlayerPulse     = 0.f;
	float PlayerBPM       = 0.f;
	
	int32 LastBeatResetTimestampMS = -1;

	FName WallPrevCollisionProfile = NAME_None;
	FVector WallEntryNormal        = FVector::ZeroVector;
	float WallEntrySpeed           = 0.f;
	float WallCompressionTimer     = 0.f;
	bool  bWallBeatPending         = false;

	void EnterWallSpring(const FHitResult& Hit);
	void EjectFromWall(bool bBeatBoost);

	static constexpr int32 HitHistorySize = 8;
	float HitTimestamps[8] = {};
	int32 HitWriteIdx      = 0;
	int32 HitCount         = 0;
};