#include "PcQPlayerMovementComponent.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "Project_Circle/MusicSystem/MusicGameplaySystem/PcMusicAnalysisSubsystem.h"

UPcQPlayerMovementComponent::UPcQPlayerMovementComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	BrakingFrictionFactor = 0.f;
	GroundFriction = 0.f;
	bMaintainHorizontalGroundVelocity = true;
	AirControl = 0.f;
	GravityScale = 1.0f;
	JumpZVelocity = 600.f;
	BrakingDecelerationWalking = 0.f;
	BrakingDecelerationFalling = 0.f;
}

void UPcQPlayerMovementComponent::BeginPlay()
{
	Super::BeginPlay();

	MaxWalkSpeed = MoveConfig ? MoveConfig->BaseMaxSpeed : 900.f;

	float HopCooldown = MoveConfig ? MoveConfig->AirHopCooldownBeats : 0.f;
	float DashCooldown = MoveConfig ? MoveConfig->DashBoostCooldownBeats : 1.5f;

	Ability_AirHop.Initialize(HopCooldown, 1, false);
	Ability_DashBoost.Initialize(DashCooldown, 1, true); 
}

bool UPcQPlayerMovementComponent::IsOnBeat() const
{
	UPcMusicAnalysisSubsystem* Sub = GetWorld()->GetSubsystem<UPcMusicAnalysisSubsystem>();
	if (!Sub || !Sub->IsReadyForPlayback() || !MoveConfig) return false;
	
	const int32 Now = Sub->GetCurrentPlaybackTimeMS();
	const int32 Next = Sub->GetNextGameplayBeatTimeMS();
	const int32 Prev = Sub->GetNextGameplayBeatTimeMS() - FMath::RoundToInt(Sub->GetGameplayBeatIntervalMS());
	const int32 Interval = FMath::RoundToInt(Sub->GetGameplayBeatIntervalMS());
	
	const int32 Window = FMath::RoundToInt(Interval * MoveConfig->OnBeatWindowFraction); 
	
	return FMath::Min(FMath::Abs(Next - Now), FMath::Abs(Now - Prev)) <= Window;
}

float UPcQPlayerMovementComponent::GetGameplayBeatIntervalSec() const
{
	if (UPcMusicAnalysisSubsystem* Sub = GetWorld()->GetSubsystem<UPcMusicAnalysisSubsystem>())
		if (Sub->IsReadyForPlayback())
			return Sub->GetGameplayBeatIntervalMS() / 1000.f;
	return MoveConfig ? MoveConfig->GetNormalBeatInterval() : 0.8f;
}

float UPcQPlayerMovementComponent::GetBeatSnappedDuration(float BaseSec) const
{
	if (UPcMusicAnalysisSubsystem* Sub = GetWorld()->GetSubsystem<UPcMusicAnalysisSubsystem>())
	{
		if (Sub->IsReadyForPlayback())
		{
			const float IntervalSec = Sub->GetGameplayBeatIntervalMS() / 1000.f;
			if (IntervalSec > 0.001f)
			{
				const float TimeToNext = Sub->GetTimeUntilNextGameplayBeat();
				const float AlreadyElapsed = IntervalSec - TimeToNext;
				const int32 WholeBeats = FMath::CeilToInt(BaseSec / IntervalSec);
				return FMath::Max((float)WholeBeats * IntervalSec - AlreadyElapsed, IntervalSec);
			}
		}
	}
	return BaseSec;
}

float UPcQPlayerMovementComponent::GetOnBeatFlash() const
{
	float Dur = MoveConfig ? MoveConfig->OnBeatFlashDuration : 0.35f;
	return Dur > 0.f ? FMath::Clamp(OnBeatFlashTimer / Dur, 0.f, 1.f) : 0.f;
}

bool UPcQPlayerMovementComponent::RegisterBeatAction(const FString& ActionName, FLinearColor Color, bool bRequireBeat)
{
	bool bIsOnBeat = IsOnBeat();
	if (bIsOnBeat) {
		TriggerOnBeatFlash();
		OnComboEvent.Broadcast(ActionName + TEXT(" ★"), Color);
		PlayerPulse = FMath::Min(PlayerPulse + 0.85f, 1.5f);
		return true;
	}
	if (!bRequireBeat) {
		OnComboEvent.Broadcast(ActionName, Color.Desaturate(0.4f));
		PlayerPulse = FMath::Min(PlayerPulse + 0.85f, 1.5f);
	}
	return false;
}

void UPcQPlayerMovementComponent::TriggerOnBeatFlash()
{
	RecordHit();
	OnBeatFlashTimer = MoveConfig ? MoveConfig->OnBeatFlashDuration : 0.35f;

	bool bGrantReset = true;
	if (UPcMusicAnalysisSubsystem* Sub = GetWorld()->GetSubsystem<UPcMusicAnalysisSubsystem>()) {
		if (Sub->IsReadyForPlayback()) {
			const int32 NextBeat = Sub->GetNextGameplayBeatTimeMS();
			if (NextBeat == LastBeatResetTimestampMS) bGrantReset = false;
			else LastBeatResetTimestampMS = NextBeat;
		}
	}

	if (bGrantReset) {
		float FillRate = MoveConfig ? MoveConfig->FrenzyFillOnBeat : 0.20f;
		FrenzyGauge = FMath::Min(1.f, FrenzyGauge + FillRate);

		float LockTime = MoveConfig ? MoveConfig->STierLockTime : 3.0f;
		if (bSTierActive && bSTierLocked) STierLockTimer = LockTime;
		
		if (!bSTierActive && FrenzyGauge >= (MoveConfig ? MoveConfig->TierS_Threshold : 1.f)) {
			bSTierActive = true; bSTierLocked = true; STierLockTimer = LockTime;
			OnComboEvent.Broadcast(TEXT("FRENZY S ★"), FLinearColor(1.f, 0.15f, 0.2f));
		}
	}
	OnActiveBeatAction.Broadcast();
}

void UPcQPlayerMovementComponent::RecordHit()
{
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	HitTimestamps[HitWriteIdx % HitHistorySize] = Now;
	HitWriteIdx++;
	HitCount = FMath::Min(HitCount + 1, HitHistorySize);
	if (HitCount >= 2) {
		float Total = 0.f; int32 Pairs = 0;
		for (int32 i = 1; i < HitCount; ++i) {
			const int32 A = (HitWriteIdx - i - 1 + HitHistorySize) % HitHistorySize;
			const int32 B = (HitWriteIdx - i + HitHistorySize) % HitHistorySize;
			const float Dt = HitTimestamps[B] - HitTimestamps[A];
			if (Dt > 0.05f && Dt < 3.f) { Total += Dt; Pairs++; }
		}
		if (Pairs > 0) PlayerBPM = 60.f / (Total / Pairs);
	}
}

ESnapAction UPcQPlayerMovementComponent::GetActiveSnap() const
{
	if (BhopState == EBhopState::WallSwim) return ESnapAction::Jump;
	return IsFalling() ? ESnapAction::AirHop : ESnapAction::DashBoost;
}

// SPACEBAR (Jump)
void UPcQPlayerMovementComponent::OnJumpPressed()
{
	if (BhopState == EBhopState::WallSwim) {
		EjectFromWall(false);
		return;
	}

	bool bOnBeat = IsOnBeat();
	
	if (GPLandingWindowTimer > 0.f) {
		ExecuteSuperJump(bOnBeat);
		return;
	}

	if (IsMovingOnGround() || BhopState == EBhopState::DashBoosting) {
		ExecuteNormalJump(bOnBeat);
		return;
	}

	bJumpInputBuffered = true;
	JumpInputBufferTimer = MoveConfig ? MoveConfig->JumpInputBufferSec : 0.22f;
}

// DASH INPUT 
void UPcQPlayerMovementComponent::OnDashPressed()
{
	if (IsMovingOnGround() || BhopState == EBhopState::DashBoosting) {
		bool bOnBeat = IsOnBeat();
		SnapPulseTimer = 0.15f;
		float BaseDur = (MoveConfig ? MoveConfig->DashBoostCooldownBeats : 1.5f) * GetGameplayBeatIntervalSec();
		if (Ability_DashBoost.TryActivate(BaseDur, GetBeatSnappedDuration(BaseDur))) {
			ExecuteDashBoost(bOnBeat);
		}
	}
}

// AIR HOP INPUT
void UPcQPlayerMovementComponent::OnAirHopPressed()
{
	if (IsFalling() || BhopState == EBhopState::AirHopping) {
		bool bOnBeat = IsOnBeat();
		SnapPulseTimer = 0.15f;
		float BaseDur = (MoveConfig ? MoveConfig->AirHopCooldownBeats : 0.f) * GetGameplayBeatIntervalSec();
		if (Ability_AirHop.TryActivate(BaseDur, GetBeatSnappedDuration(BaseDur))) {
			ExecuteAirHop(bOnBeat);
		}
	}
}

void UPcQPlayerMovementComponent::OnGroundPoundPressed()
{
	if (BhopState == EBhopState::WallSwim || IsMovingOnGround()) return;
	RegisterBeatAction(TEXT("GROUND POUND"), FLinearColor(0.8f, 0.2f, 0.2f), false);
	BhopState = EBhopState::GroundPounding;
	Velocity.Z = MoveConfig ? -MoveConfig->GPDownVelocity : -2800.f;
}

void UPcQPlayerMovementComponent::OnJumpReleased() {}
void UPcQPlayerMovementComponent::OnGroundPoundReleased() {}

void UPcQPlayerMovementComponent::NotifyGunFired()
{
	RegisterBeatAction(TEXT("SHOT"), FLinearColor(1.f, 0.35f, 1.f), true);
	if (IsMovingOnGround()) OnDashPressed();
	else OnAirHopPressed();
}

void UPcQPlayerMovementComponent::ApplyArcWithAirTime(float PeakHeightCM, float AirTimeSec)
{
	if (CharacterOwner) CharacterOwner->JumpCurrentCount = 0;
	if (AirTimeSec <= 0.001f) AirTimeSec = 0.5f;

	const float T = AirTimeSec * 0.5f;
	const float G = (2.f * PeakHeightCM) / (T * T);
	GravityScale = G / FMath::Abs(GetWorld()->GetDefaultGravityZ());
	Velocity.Z = G * T;
	SetMovementMode(MOVE_Falling);
}

void UPcQPlayerMovementComponent::ExecuteNormalJump(bool bOnBeat)
{
	RegisterBeatAction(TEXT("JUMP"), FLinearColor(0.8f, 1.f, 0.4f), bOnBeat);
	
	BhopState = EBhopState::Normal;
	const float PreJumpH = GetHorizontalSpeed();
	FVector Dir2D = Acceleration.GetSafeNormal2D().IsZero() ? FVector(Velocity.X, Velocity.Y, 0.f).GetSafeNormal() : Acceleration.GetSafeNormal2D();

	ApplyArcWithAirTime(MoveConfig ? MoveConfig->FallbackNormalJumpHeight : 600.f, GetGameplayBeatIntervalSec());

	if (!Dir2D.IsZero()) {
		float Boost = bOnBeat && MoveConfig ? MoveConfig->BeatJumpHorizBoost : 0.f;
		Velocity.X = Dir2D.X * (PreJumpH + Boost);
		Velocity.Y = Dir2D.Y * (PreJumpH + Boost);
	}
	OnBhopLanded.Broadcast(GetHorizontalSpeed());
}

// ── CURVE-DRIVEN AIR HOP ──
void UPcQPlayerMovementComponent::ExecuteAirHop(bool bOnBeat)
{
	RegisterBeatAction(TEXT("AIR HOP"), FLinearColor(0.27f, 0.67f, 1.f), bOnBeat);
	BhopState = EBhopState::AirHopping;
	
	float HopDurationBeats = MoveConfig ? MoveConfig->AirHopDurationBeats : 1.0f;
	ActiveDashDuration = GetGameplayBeatIntervalSec() * HopDurationBeats;
	ActiveDashTimer = ActiveDashDuration;
	ActiveDashPeakSpeed = (MoveConfig ? MoveConfig->AirHopPeakSpeed : 1800.f) * GetFrenzySpeedMult();

	ActiveDashDir = Acceleration.GetSafeNormal2D();
	if (ActiveDashDir.IsZero()) ActiveDashDir = FVector(Velocity.X, Velocity.Y, 0.f).GetSafeNormal();
	if (ActiveDashDir.IsZero() && CharacterOwner) ActiveDashDir = CharacterOwner->GetActorForwardVector().GetSafeNormal2D();

	ApplyArcWithAirTime(MoveConfig ? MoveConfig->AirHopVerticalForce : 250.f, ActiveDashDuration);
}

// ── CURVE-DRIVEN DASH BOOST ──
void UPcQPlayerMovementComponent::ExecuteDashBoost(bool bOnBeat)
{
	RegisterBeatAction(TEXT("DASH BOOST"), FLinearColor(1.f, 0.55f, 0.15f), bOnBeat);
	BhopState = EBhopState::DashBoosting;
	
	ActiveDashDuration = MoveConfig ? MoveConfig->DashBoostDurationSec : 0.35f;
	ActiveDashTimer = ActiveDashDuration;
	ActiveDashPeakSpeed = (MoveConfig ? MoveConfig->DashBoostPeakSpeed : 2200.f) * GetFrenzySpeedMult();

	ActiveDashDir = Acceleration.GetSafeNormal2D();
	if (ActiveDashDir.IsZero()) ActiveDashDir = FVector(Velocity.X, Velocity.Y, 0.f).GetSafeNormal();
	if (ActiveDashDir.IsZero() && CharacterOwner) ActiveDashDir = CharacterOwner->GetActorForwardVector().GetSafeNormal2D();
	
	Velocity.Z = 0.f;
}

// ── CURVE-DRIVEN S-TIER AUTO DASH ──
void UPcQPlayerMovementComponent::TriggerFrenzyDashBoost()
{
	if (!IsMovingOnGround() || !bSTierActive) return;

	ActiveDashDir = Acceleration.GetSafeNormal2D();
	if (ActiveDashDir.IsZero()) ActiveDashDir = FVector(Velocity.X, Velocity.Y, 0.f).GetSafeNormal();
	if (ActiveDashDir.IsZero() && CharacterOwner) ActiveDashDir = CharacterOwner->GetActorForwardVector().GetSafeNormal2D();
	if (ActiveDashDir.IsZero()) return;

	BhopState = EBhopState::DashBoosting;
	ActiveDashDuration = MoveConfig ? MoveConfig->DashBoostDurationSec : 0.35f;
	ActiveDashTimer = ActiveDashDuration;
	ActiveDashPeakSpeed = (MoveConfig ? MoveConfig->STierAutoDashPeakSpeed : 2600.f) * GetFrenzySpeedMult();
	Velocity.Z = 0.f;

	RegisterBeatAction(TEXT("FRENZY DASH"), FLinearColor(1.f, 0.15f, 0.2f), true);
}

void UPcQPlayerMovementComponent::ExecuteSuperJump(bool bOnBeat)
{
	RegisterBeatAction(TEXT("SUPER JUMP"), FLinearColor(1.f, 0.9f, 0.2f), bOnBeat);
	GPLandingWindowTimer = 0.f; BhopState = EBhopState::Normal;
	
	ApplyArcWithAirTime(MoveConfig ? MoveConfig->SuperJumpVerticalForce : 1200.f, GetGameplayBeatIntervalSec() * 2.0f);
	
	FVector Dir2D = Acceleration.GetSafeNormal2D();
	if (!Dir2D.IsZero() && MoveConfig) {
		Velocity.X += Dir2D.X * (MoveConfig->BeatJumpHorizBoost * 1.5f);
		Velocity.Y += Dir2D.Y * (MoveConfig->BeatJumpHorizBoost * 1.5f);
	}
}

void UPcQPlayerMovementComponent::TriggerBeatJump()
{
	if (BhopState == EBhopState::WallSwim) bWallBeatPending = true;
	else if (IsFalling() && Velocity.Z <= 0.f) bJumpQueuedForBeat = true;
}

void UPcQPlayerMovementComponent::EnterWallSpring(const FHitResult& Hit)
{
	if (BhopState == EBhopState::WallSwim) return;
	BhopState = EBhopState::WallSwim;
	WallEntryNormal = Hit.ImpactNormal; WallEntrySpeed = GetHorizontalSpeed();
	WallCompressionTimer = 0.f; bWallBeatPending = false;
	SetMovementMode(MOVE_Flying); OnWallSwimChanged.Broadcast(1.f);
}

void UPcQPlayerMovementComponent::EjectFromWall(bool bBeatBoost)
{
	BhopState = EBhopState::Normal;
	FVector EjectDir = Acceleration.GetSafeNormal2D();
	if (EjectDir.IsZero()) EjectDir = WallEntryNormal;

	float BaseSpeed = MoveConfig ? MoveConfig->WallEjectBaseSpeed : 1200.f;
	float UpKick = MoveConfig ? MoveConfig->WallEjectUpKick : 450.f;

	float EjectSpd = FMath::Max(WallEntrySpeed, BaseSpeed);
	Velocity.X = EjectDir.X * EjectSpd;
	Velocity.Y = EjectDir.Y * EjectSpd;
	Velocity.Z = UpKick; 

	GravityScale = 1.f;
	SetMovementMode(MOVE_Falling);
	OnWallSwimChanged.Broadcast(0.f);
	RegisterBeatAction(TEXT("WALL EJECT"), FLinearColor(0.55f, 1.f, 0.35f), bBeatBoost);
}

void UPcQPlayerMovementComponent::ProcessLanded(const FHitResult& Hit, float remainingTime, int32 Iterations)
{
	if (BhopState == EBhopState::GroundPounding) {
		BhopState = EBhopState::Normal;
		GPLandingWindowTimer = MoveConfig ? MoveConfig->GPLandingComboWindow : 0.2f; 
		RegisterBeatAction(TEXT("GP SLAM"), FLinearColor(1.f, 0.55f, 0.15f), false);
	} else {
		BhopState = EBhopState::Normal;
	}

	Ability_AirHop.ResetCharges();
	GravityScale = 1.f;
	
	Super::ProcessLanded(Hit, remainingTime, Iterations);

	if (bJumpInputBuffered && JumpInputBufferTimer > 0.f) {
		bJumpInputBuffered = false;
		if (GPLandingWindowTimer > 0.f) ExecuteSuperJump(IsOnBeat());
		else ExecuteNormalJump(IsOnBeat());
	}
}

// ── CURVE TICK LOGIC ──
void UPcQPlayerMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Ability_AirHop.Tick(DeltaTime);
	Ability_DashBoost.Tick(DeltaTime);

	// Frenzy Drain
	float Drain = MoveConfig ? MoveConfig->FrenzyDrainPerSec : 0.04f;
	if (bSTierActive) {
		if (bSTierLocked) {
			STierLockTimer -= DeltaTime;
			if (STierLockTimer <= 0.f) bSTierLocked = false;
		} else {
			FrenzyGauge = FMath::Max(0.f, FrenzyGauge - Drain * 0.1f * DeltaTime);
			if (FrenzyGauge < 0.85f) bSTierActive = false;
		}
	} else {
		FrenzyGauge = FMath::Max(0.f, FrenzyGauge - Drain * DeltaTime);
	}

	MaxWalkSpeed = (MoveConfig ? MoveConfig->BaseMaxSpeed : 900.f) * GetFrenzySpeedMult();

	// Dash/Hop Curve State Management
	if (BhopState == EBhopState::DashBoosting || BhopState == EBhopState::AirHopping)
	{
		ActiveDashTimer -= DeltaTime;
		if (ActiveDashTimer <= 0.f) {
			BhopState = EBhopState::Normal;
		}
	}
	else
	{
		// Regular Overspeed Decay when not dashing
		float CurH = GetHorizontalSpeed();
		float Cap = MaxWalkSpeed * (MoveConfig ? MoveConfig->HardSpeedCapMult : 4.f);
		if (CurH > MaxWalkSpeed) {
			float Decay = (MoveConfig ? MoveConfig->OverspeedDecayRate : 300.f) * DeltaTime;
			float NewH = FMath::Clamp(CurH - Decay, MaxWalkSpeed, Cap);
			FVector Dir2D = FVector(Velocity.X, Velocity.Y, 0.f).GetSafeNormal();
			if (!Dir2D.IsZero()) { Velocity.X = Dir2D.X * NewH; Velocity.Y = Dir2D.Y * NewH; }
		}
	}

	if (GPLandingWindowTimer > 0.f) GPLandingWindowTimer -= DeltaTime;
	if (OnBeatFlashTimer > 0.f)     OnBeatFlashTimer -= DeltaTime;
	if (SnapPulseTimer > 0.f)       SnapPulseTimer -= DeltaTime;
	if (bJumpInputBuffered)         JumpInputBufferTimer -= DeltaTime;
	
	PlayerPulse = FMath::Max(0.f, PlayerPulse - DeltaTime * (MoveConfig ? MoveConfig->PlayerPulseDecayRate : 3.5f));

	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

float UPcQPlayerMovementComponent::GetFrenzySpeedMult() const
{
	if (!MoveConfig) return 1.f;
	if (bSTierActive) return MoveConfig->SpeedMult_S;
	if (FrenzyGauge >= MoveConfig->TierA_Threshold) return MoveConfig->SpeedMult_A;
	if (FrenzyGauge >= MoveConfig->TierB_Threshold) return MoveConfig->SpeedMult_B;
	if (FrenzyGauge >= MoveConfig->TierC_Threshold) return MoveConfig->SpeedMult_C;
	return MoveConfig->SpeedMult_D;
}

// ── CUSTOM PHYSICS INTEGRATION ──
void UPcQPlayerMovementComponent::PhysWalking(float deltaTime, int32 Iterations)
{
	if (deltaTime < MIN_TICK_TIME) return;

	float TargetSpeed = MaxWalkSpeed;
	FVector WishDir = Acceleration.GetSafeNormal2D();
	FVector Vel2D(Velocity.X, Velocity.Y, 0.f);
	
	float Accel = (MoveConfig ? MoveConfig->GroundAcceleration : 2400.f) * GetFrenzySpeedMult();
	float Fric = MoveConfig ? MoveConfig->GroundFriction : 2400.f;
	float EffTarget = TargetSpeed;

	// If Dashing: Override target speed with the curve directly!
	if (BhopState == EBhopState::DashBoosting || BhopState == EBhopState::AirHopping)
	{
		float CurveTime = 1.0f - FMath::Clamp(ActiveDashTimer / FMath::Max(ActiveDashDuration, 0.001f), 0.0f, 1.0f);
		float CurveMod = 1.0f;
		if (BhopState == EBhopState::DashBoosting && MoveConfig && MoveConfig->DashSpeedCurve) {
			CurveMod = MoveConfig->DashSpeedCurve->GetFloatValue(CurveTime);
		} else if (BhopState == EBhopState::AirHopping && MoveConfig && MoveConfig->AirHopSpeedCurve) {
			CurveMod = MoveConfig->AirHopSpeedCurve->GetFloatValue(CurveTime);
		}

		TargetSpeed = ActiveDashPeakSpeed * CurveMod;
		EffTarget = TargetSpeed; // FORCE it to exactly the curve data

		// Steering: Blend player WASD into the dash direction
		if (!WishDir.IsZero()) {
			ActiveDashDir = FMath::VInterpTo(ActiveDashDir, WishDir, deltaTime, 12.0f).GetSafeNormal();
		}
		
		WishDir = ActiveDashDir;
		Accel = 25000.f; // Massive acceleration to stick exactly to the curve's speed
	}
	else
	{
		float Spd = Vel2D.Size();
		// If speeding faster than walkspeed (like directly after ending a dash), allow momentum
		EffTarget = (Spd > TargetSpeed && !WishDir.IsZero()) ? Spd : TargetSpeed;
	}

	FVector NewVel2D = FMath::VInterpTo(Vel2D, WishDir * EffTarget, deltaTime, WishDir.IsZero() ? Fric : Accel);
	Velocity.X = NewVel2D.X; Velocity.Y = NewVel2D.Y;

	FVector Saved = Acceleration; Acceleration = FVector::ZeroVector;
	Super::PhysWalking(deltaTime, Iterations);
	Acceleration = Saved;
}

void UPcQPlayerMovementComponent::PhysFalling(float deltaTime, int32 Iterations)
{
	if (deltaTime < MIN_TICK_TIME || BhopState == EBhopState::GroundPounding) {
		if (BhopState == EBhopState::GroundPounding) Acceleration = FVector::ZeroVector;
		Super::PhysFalling(deltaTime, Iterations);
		return;
	}

	float TargetSpeed = MaxWalkSpeed;
	FVector WishDir = Acceleration.GetSafeNormal2D();
	FVector Vel2D(Velocity.X, Velocity.Y, 0.f);
	
	float Accel = (MoveConfig ? MoveConfig->AirAcceleration : 700.f) * GetFrenzySpeedMult();
	float EffAirTarget;

	// If Air Hopping: Override target speed with the curve directly!
	if (BhopState == EBhopState::DashBoosting || BhopState == EBhopState::AirHopping)
	{
		float CurveTime = 1.0f - FMath::Clamp(ActiveDashTimer / FMath::Max(ActiveDashDuration, 0.001f), 0.0f, 1.0f);
		float CurveMod = 1.0f;
		if (BhopState == EBhopState::DashBoosting && MoveConfig && MoveConfig->DashSpeedCurve) {
			CurveMod = MoveConfig->DashSpeedCurve->GetFloatValue(CurveTime);
		} else if (BhopState == EBhopState::AirHopping && MoveConfig && MoveConfig->AirHopSpeedCurve) {
			CurveMod = MoveConfig->AirHopSpeedCurve->GetFloatValue(CurveTime);
		}

		TargetSpeed = ActiveDashPeakSpeed * CurveMod;
		EffAirTarget = TargetSpeed; // FORCE it to exactly the curve data

		if (!WishDir.IsZero()) {
			ActiveDashDir = FMath::VInterpTo(ActiveDashDir, WishDir, deltaTime, 12.0f).GetSafeNormal();
		}
		WishDir = ActiveDashDir;
		Accel = 25000.f; 
	}
	else
	{
		EffAirTarget = WishDir.IsZero() ? 0.f : FMath::Max(Vel2D.Size(), TargetSpeed);
	}
	
	FVector NewVel2D = FMath::VInterpTo(Vel2D, WishDir * EffAirTarget, deltaTime, Accel);
	Velocity.X = NewVel2D.X; Velocity.Y = NewVel2D.Y;

	FVector Saved = Acceleration; Acceleration = FVector::ZeroVector;
	Super::PhysFalling(deltaTime, Iterations);
	Acceleration = Saved;
}