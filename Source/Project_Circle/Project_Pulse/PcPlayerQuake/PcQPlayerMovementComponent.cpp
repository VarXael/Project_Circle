#include "PcQPlayerMovementComponent.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "Project_Circle/MusicSystem/MusicGameplaySystem/PcMusicAnalysisSubsystem.h"
#include "Project_Circle/Project_Pulse/Enemies/PcQEnemyBase.h"

UPcQPlayerMovementComponent::UPcQPlayerMovementComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	BrakingFrictionFactor = 0.f; GroundFriction = 0.f;
	bMaintainHorizontalGroundVelocity = true; AirControl = 0.f;
	MaxWalkSpeed = 1000.f; JumpZVelocity = 600.f;
	BrakingDecelerationWalking = 0.f; BrakingDecelerationFalling = 0.f;
	bUseSeparateBrakingFriction = false; BrakingFriction = 0.f;
}

float UPcQPlayerMovementComponent::Cfg_BaseMaxSpeed() const { return Config ? Config->BaseMaxSpeed : 1000.f; }
float UPcQPlayerMovementComponent::Cfg_GroundAcceleration() const { return Config ? Config->GroundAcceleration : 30.f; }
float UPcQPlayerMovementComponent::Cfg_GroundFriction() const { return Config ? Config->GroundFriction : 25.f; }
float UPcQPlayerMovementComponent::Cfg_AirAcceleration() const { return Config ? Config->AirAcceleration : 15.f; }
float UPcQPlayerMovementComponent::Cfg_JumpPeakHeight() const { return Config ? Config->JumpPeakHeightCM : 260.f; }
float UPcQPlayerMovementComponent::Cfg_IdealJumpAirTime() const { return Config ? Config->IdealJumpAirTimeSec : 0.85f; }
float UPcQPlayerMovementComponent::Cfg_SuperJumpHorizBoost() const { return Config ? Config->SuperJumpHorizBoost : 420.f; }
int32 UPcQPlayerMovementComponent::Cfg_MaxDoubleJumps() const { return Config ? Config->MaxDoubleJumps : 2; }
float UPcQPlayerMovementComponent::Cfg_IdealDJAirTime() const { return Config ? Config->IdealDJAirTimeSec : 0.5f; }
float UPcQPlayerMovementComponent::Cfg_GPSlamSpeed() const { return Config ? Config->GPSlamSpeed : 2800.f; }
float UPcQPlayerMovementComponent::Cfg_GPImmunityBeats() const { return Config ? Config->GPPulseImmunityBeats : 2.f; }
float UPcQPlayerMovementComponent::Cfg_IdealGroundPulseDuration() const { return Config ? Config->IdealGroundPulseDurationSec : 0.6f; }
float UPcQPlayerMovementComponent::Cfg_DashBoostSpeedMult() const { return Config ? Config->DashBoostSpeedMult : 1.55f; }
float UPcQPlayerMovementComponent::Cfg_IdealDashDuration() const { return Config ? Config->IdealDashDurationSec : 0.35f; }
float UPcQPlayerMovementComponent::Cfg_DashSteerAccel() const { return Config ? Config->DashSteerAcceleration : 30.f; }
float UPcQPlayerMovementComponent::Cfg_DashJumpBoost() const { return Config ? Config->DashJumpBoost : 200.f; }
float UPcQPlayerMovementComponent::Cfg_PostDashImmunityBeats() const { return Config ? Config->PostDashImmunityBeats : 0.25f; }
float UPcQPlayerMovementComponent::Cfg_HardSpeedCapMult() const { return Config ? Config->HardSpeedCapMult : 4.f; }
float UPcQPlayerMovementComponent::Cfg_OverspeedDecay() const { return Config ? Config->OverspeedDecayRate : 200.f; }
int32 UPcQPlayerMovementComponent::Cfg_OnBeatWindowMs() const { return Config ? Config->OnBeatWindowMs : 160; }
float UPcQPlayerMovementComponent::Cfg_JumpInputBuffer() const { return Config ? Config->JumpInputBufferSec : 0.02f; }

float UPcQPlayerMovementComponent::Cfg_SlashLungeDistance() const { return Config ? Config->SlashLungeDistance : 525.f; } 
float UPcQPlayerMovementComponent::Cfg_SlashLungeDurationSec() const { return Config ? Config->SlashLungeDurationSec : 0.15f; }
float UPcQPlayerMovementComponent::Cfg_SlashBopEnemyLift() const { return Config ? Config->SlashBopEnemyLift : 700.f; }
float UPcQPlayerMovementComponent::Cfg_SlashBopWallLift() const { return Config ? Config->SlashBopWallLift : 500.f; }
float UPcQPlayerMovementComponent::Cfg_SlashBopHorizRetain() const { return Config ? Config->SlashBopHorizRetain : 0.2f; }

float UPcQPlayerMovementComponent::GetHorizontalSpeed() const { return FVector(Velocity.X, Velocity.Y, 0.f).Size(); }

float UPcQPlayerMovementComponent::GetCurrentBeatIntervalSec() const {
	if (UPcMusicAnalysisSubsystem* Sub = GetWorld() ? GetWorld()->GetSubsystem<UPcMusicAnalysisSubsystem>() : nullptr)
		if (Sub->IsReadyForPlayback()) return Sub->GetGameplayBeatIntervalMS() / 1000.f;
	const float RefBPM = Config ? Config->ReferenceBPM : 100.f;
	return 60.f / FMath::Max(RefBPM, 1.f);
}

float UPcQPlayerMovementComponent::GetAdaptiveTime(float IdealTimeSec) const
{
	float Interval = GetCurrentBeatIntervalSec();
	if (Interval <= 0.f) return IdealTimeSec; 
	float Beats = IdealTimeSec / Interval;
	float SnappedBeats = FMath::RoundToFloat(Beats * 2.f) / 2.f; 
	SnappedBeats = FMath::Max(0.5f, SnappedBeats); 
	return SnappedBeats * Interval;
}

// ── NEW: SMOOTH BPM SCALING (No stuttering!) ──
float UPcQPlayerMovementComponent::GetSmoothScaledTime(float BaseTimeSec) const
{
	float CurrentBPM = Config ? Config->ReferenceBPM : 100.f;
	if (UPcMusicAnalysisSubsystem* Sub = GetWorld() ? GetWorld()->GetSubsystem<UPcMusicAnalysisSubsystem>() : nullptr) {
		if (Sub->IsReadyForPlayback()) CurrentBPM = Sub->GetCurrentGameplayBPM();
	}
	float Scale = Config ? FMath::Clamp(CurrentBPM / FMath::Max(Config->ReferenceBPM, 1.f), Config->SpeedScaleMin, Config->SpeedScaleMax) : 1.f;
	return BaseTimeSec / Scale;
}

float UPcQPlayerMovementComponent::ComputeRhythmGravity() const
{
	float TargetHeight = Cfg_JumpPeakHeight();
	float TargetTime = GetSmoothScaledTime(Cfg_IdealJumpAirTime());
	if (TargetTime <= 0.01f) return FMath::Abs(GetWorld()->GetDefaultGravityZ());
	return (8.f * TargetHeight) / (TargetTime * TargetTime);
}

float UPcQPlayerMovementComponent::ComputeCurrentMaxSpeed() const {
	float Base = Cfg_BaseMaxSpeed();
	if (!Config) return Base;
	float CurrentBPM = Config->ReferenceBPM;
	if (UPcMusicAnalysisSubsystem* Sub = GetWorld() ? GetWorld()->GetSubsystem<UPcMusicAnalysisSubsystem>() : nullptr)
		if (Sub->IsReadyForPlayback()) CurrentBPM = Sub->GetCurrentGameplayBPM();
	const float Scale = FMath::Clamp(CurrentBPM / FMath::Max(Config->ReferenceBPM, 1.f), Config->SpeedScaleMin, Config->SpeedScaleMax);
	Base *= Scale;
	
	if (GroundPulseBoostTimer > 0.f && MovState == EPlayerMovementState::Grounded) {
		Base *= 1.4f; 
	}
	return Base;
}

float UPcQPlayerMovementComponent::GetDashActiveAlpha() const { return (DashBoostMaxTime > 0.f) ? FMath::Clamp(DashBoostTimer / DashBoostMaxTime, 0.f, 1.f) : 0.f; }
float UPcQPlayerMovementComponent::GetOnBeatFlash() const { return OnBeatFlashDuration > 0.f ? FMath::Clamp(OnBeatFlashTimer / OnBeatFlashDuration, 0.f, 1.f) : 0.f; }
int32 UPcQPlayerMovementComponent::GetOnBeatWindowMs() const { return Cfg_OnBeatWindowMs(); }

float UPcQPlayerMovementComponent::GetGroundPulseBoostAlpha() const {
	const float MaxTime = GetCurrentBeatIntervalSec();
	return MaxTime > 0.f ? FMath::Clamp(GroundPulseBoostTimer / MaxTime, 0.f, 1.f) : 0.f;
}

float UPcQPlayerMovementComponent::GetBeatPhase() const {
	UPcMusicAnalysisSubsystem* Sub = GetWorld() ? GetWorld()->GetSubsystem<UPcMusicAnalysisSubsystem>() : nullptr;
	if (!Sub || !Sub->IsReadyForPlayback()) return 0.f;
	const float Interval = Sub->GetGameplayBeatIntervalMS() / 1000.f;
	if (Interval <= 0.f) return 0.f;
	return 1.f - FMath::Clamp(Sub->GetTimeUntilNextGameplayBeat() / Interval, 0.f, 1.f);
}
float UPcQPlayerMovementComponent::GetJumpBufferAlpha() const {
	if (!bJumpInputBuffered) return 0.f;
	const float MaxBuffer = Cfg_JumpInputBuffer();
	return MaxBuffer > 0.f ? FMath::Clamp(JumpInputBufferTimer / MaxBuffer, 0.f, 1.f) : 0.f;
}
bool UPcQPlayerMovementComponent::IsNearBeat() const {
	UPcMusicAnalysisSubsystem* Sub = GetWorld() ? GetWorld()->GetSubsystem<UPcMusicAnalysisSubsystem>() : nullptr;
	if (!Sub || !Sub->IsReadyForPlayback()) return false;
	const int32 Now = Sub->GetCurrentPlaybackTimeMS();
	const int32 Next = Sub->GetNextGameplayBeatTimeMS();
	const int32 Interval = FMath::RoundToInt(Sub->GetGameplayBeatIntervalMS());
	const int32 Prev = Next - Interval;
	return FMath::Min(FMath::Abs(Next - Now), FMath::Abs(Now - Prev)) <= Cfg_OnBeatWindowMs();
}

void UPcQPlayerMovementComponent::ExecuteKineticJump()
{
	FVector Dir2D = Acceleration.GetSafeNormal2D();
	if (Dir2D.IsZero()) Dir2D = FVector(Velocity.X, Velocity.Y, 0.f).GetSafeNormal();

	if (!Dir2D.IsZero())
	{
		const float LaunchH = FMath::Min(GetHorizontalSpeed() + Cfg_DashJumpBoost() + Cfg_SuperJumpHorizBoost(), Cfg_BaseMaxSpeed() * Cfg_HardSpeedCapMult());
		Velocity.X = Dir2D.X * LaunchH;
		Velocity.Y = Dir2D.Y * LaunchH;
	}
	
	ExitDash();
	if (CharacterOwner) CharacterOwner->JumpCurrentCount = 0;
	MovState = EPlayerMovementState::InAir;

	ApplyMagneticJump(GetSmoothScaledTime(Cfg_IdealJumpAirTime()));
	
	OnSuperJumped.Broadcast();
	PushCombo(TEXT("KINETIC JUMP"), FLinearColor(0.1f, 0.8f, 1.f)); 
}

void UPcQPlayerMovementComponent::OnJumpPressed()
{
	float Now = GetWorld()->GetTimeSeconds();
	LastJumpTime = Now;

	bool bCanKineticJump = (MovState == EPlayerMovementState::Dashing || MovState == EPlayerMovementState::SwordLunging) 
	                       || (Now - LastSlashTime <= 0.2f); 

	if (bCanKineticJump)
	{
		ExecuteKineticJump();
		return;
	}

	switch (MovState)
	{
	case EPlayerMovementState::Grounded:
		DoNormalJump();
		break;

	case EPlayerMovementState::InAir:
		if (CurrentDJCount > 0) {
			DoDoubleJump();
		} 
		else if (CanBufferLanding()) {
			bJumpInputBuffered = true; 
			JumpInputBufferTimer = Cfg_JumpInputBuffer(); 
		}
		break;

	case EPlayerMovementState::GroundPounding:
		bJumpInputBuffered = true; JumpInputBufferTimer = Cfg_JumpInputBuffer(); break;
	default: break;
	}
}

void UPcQPlayerMovementComponent::OnJumpReleased() {}
void UPcQPlayerMovementComponent::OnGroundPoundPressed()
{
	if (MovState == EPlayerMovementState::Dashing || MovState == EPlayerMovementState::SwordLunging) return;
	switch (MovState) {
	case EPlayerMovementState::Grounded: EnterDash(); break;
	case EPlayerMovementState::InAir:
		if (CanBufferLanding()) { bGPInputBuffered = true; GPInputBufferTimer = Cfg_JumpInputBuffer(); return; }
		DoGroundPound(); break;
	case EPlayerMovementState::GroundPounding: break;
	default: break;
	}
}

void UPcQPlayerMovementComponent::TriggerGroundPulse()
{
	if (PulseImmunityTimer > 0.f) return;
	if (!IsMovingOnGround()) {
		if (IsFalling() && Velocity.Z < 0.f && MovState == EPlayerMovementState::InAir) {
			bPulseBufferedForLanding = true;
			PulseBufferTimer = (float)Cfg_OnBeatWindowMs() / 1000.f;
		}
		return;
	}
	if (MovState != EPlayerMovementState::Grounded) return;
	
	GroundPulseBoostTimer = GetAdaptiveTime(Cfg_IdealGroundPulseDuration()); 
	OnBeatFlashTimer = OnBeatFlashDuration;
	OnGroundPulseHit.Broadcast();

	if (bJumpInputBuffered && JumpInputBufferTimer > 0.f) {
		bJumpInputBuffered = false; JumpInputBufferTimer = 0.f;
		DoNormalJump(); 
	}
}

void UPcQPlayerMovementComponent::NotifyGunFired(bool bWasOnBeat) {
	if (bWasOnBeat) OnBeatFlashTimer = OnBeatFlashDuration;
}

void UPcQPlayerMovementComponent::ResetMobilityAbilities() {
	CurrentDJCount = Cfg_MaxDoubleJumps();
	PushCombo(TEXT("MOBILITY RESET!"), FLinearColor(0.2f, 1.f, 0.4f));
}

void UPcQPlayerMovementComponent::DoSwordLunge(FVector ViewDirection)
{
	float Now = GetWorld()->GetTimeSeconds();
	LastSlashTime = Now;

	if (!IsMovingOnGround() && (Now - LastJumpTime <= 0.2f))
	{
		ExecuteKineticJump();
		return;
	}

	MovState = EPlayerMovementState::SwordLunging;
	SwordLungeDirection = ViewDirection;
	
	SwordLungeTimer = FMath::Max(0.01f, Cfg_SlashLungeDurationSec());
	float RequiredSpeed = Cfg_SlashLungeDistance() / SwordLungeTimer;
	Velocity = SwordLungeDirection * RequiredSpeed;
	
	PushCombo(TEXT("KINETIC SLASH"), FLinearColor(0.1f, 0.8f, 1.f)); 
}

void UPcQPlayerMovementComponent::DoSwordBop(bool bIsWallKick)
{
	MovState = EPlayerMovementState::InAir;
	
	float HorizSpeed = GetHorizontalSpeed();
	Velocity.X *= Cfg_SlashBopHorizRetain();
	Velocity.Y *= Cfg_SlashBopHorizRetain();
	
	float BaseLift = bIsWallKick ? Cfg_SlashBopWallLift() : Cfg_SlashBopEnemyLift();
	float BonusLift = FMath::Min(HorizSpeed * 0.8f, 1200.f); 
	
	Velocity.Z = BaseLift + BonusLift; 
	CurrentDJCount = Cfg_MaxDoubleJumps(); 
	
	PushCombo(bIsWallKick ? TEXT("WALL KICK") : TEXT("ENEMY STEP"), FLinearColor(1.f, 0.3f, 0.4f));
}

void UPcQPlayerMovementComponent::DoNormalJump() {
	if (CharacterOwner) CharacterOwner->JumpCurrentCount = 0;
	ApplyMagneticJump(GetSmoothScaledTime(Cfg_IdealJumpAirTime()));
	MovState = EPlayerMovementState::InAir; 
	PushCombo(TEXT("JUMP"), FLinearColor(0.85f, 0.85f, 0.85f));
}

void UPcQPlayerMovementComponent::DoDoubleJump() {
	if (CharacterOwner) CharacterOwner->JumpCurrentCount = 0;
	ApplyMagneticJump(GetSmoothScaledTime(Cfg_IdealDJAirTime()));
	CurrentDJCount--; 
	OnBeatFlashTimer = OnBeatFlashDuration; OnDoubleJumped.Broadcast();
	PushCombo(TEXT("DOUBLE JUMP"), FLinearColor(0.27f, 0.67f, 1.f));
}

void UPcQPlayerMovementComponent::DoGroundPound() {
	MovState = EPlayerMovementState::GroundPounding;
	Velocity.Z = -FMath::Abs(Cfg_GPSlamSpeed()); Velocity.X *= 0.25f; Velocity.Y *= 0.25f;
	PushCombo(TEXT("GROUND POUND"), FLinearColor(1.f, 0.35f, 0.1f));
}

void UPcQPlayerMovementComponent::EnterDash() {
	FVector Dir2D = Acceleration.GetSafeNormal2D();
	if (Dir2D.IsZero()) Dir2D = FVector(Velocity.X, Velocity.Y, 0.f).GetSafeNormal();
	if (Dir2D.IsZero() && CharacterOwner) Dir2D = CharacterOwner->GetActorForwardVector().GetSafeNormal2D();
	const float BoostSpd = ComputeCurrentMaxSpeed() * Cfg_DashBoostSpeedMult();
	if (!Dir2D.IsZero()) { Velocity.X = Dir2D.X * BoostSpd; Velocity.Y = Dir2D.Y * BoostSpd; Velocity.Z = 0.f; }
	
	float Duration = GetAdaptiveTime(Cfg_IdealDashDuration());
	DashBoostTimer = Duration; DashBoostMaxTime = Duration;
	MovState = EPlayerMovementState::Dashing;
	PulseImmunityTimer = FMath::Max(PulseImmunityTimer, Duration + 0.05f);
	OnDashStarted.Broadcast(); PushCombo(TEXT("DASH"), FLinearColor(1.f, 0.55f, 0.15f));
}

void UPcQPlayerMovementComponent::ExitDash() {
	MovState = EPlayerMovementState::Grounded; DashBoostTimer = 0.f;
	const float Grace = Cfg_PostDashImmunityBeats() * GetCurrentBeatIntervalSec();
	PulseImmunityTimer = FMath::Max(PulseImmunityTimer, Grace);
	OnDashEnded.Broadcast();
}

// ── THE MAGIC: BEAT MAGNETISM (AIM ASSIST FOR JUMPING) ──
void UPcQPlayerMovementComponent::ApplyMagneticJump(float IdealAirTimeSec) {
	if (CharacterOwner) CharacterOwner->JumpCurrentCount = 0;
	
	float RhythmGravity = ComputeRhythmGravity();
	float TargetAirTime = IdealAirTimeSec;

	// Only apply magnetism if the user checks "bSyncJumpToBeat"
	if (Config && Config->bSyncJumpToBeat) {
		UPcMusicAnalysisSubsystem* Sub = GetWorld() ? GetWorld()->GetSubsystem<UPcMusicAnalysisSubsystem>() : nullptr;
		if (Sub && Sub->IsReadyForPlayback()) {
			float NowSec = Sub->GetCurrentPlaybackTimeMS() / 1000.f;
			float IntervalSec = Sub->GetGameplayBeatIntervalMS() / 1000.f;
			float NextBeatSec = Sub->GetNextGameplayBeatTimeMS() / 1000.f;

			// Project our intended landing time
			float TargetTime = NowSec + IdealAirTimeSec;

			// Find the actual musical beat that is closest to our intended landing time
			float ClosestBeat = NextBeatSec;
			while (ClosestBeat < TargetTime - (IntervalSec * 0.5f)) {
				ClosestBeat += IntervalSec;
			}
			if (FMath::Abs((ClosestBeat + IntervalSec) - TargetTime) < FMath::Abs(ClosestBeat - TargetTime)) {
				ClosestBeat += IntervalSec;
			}

			// Adjust our AirTime so we land EXACTLY on that beat!
			TargetAirTime = ClosestBeat - NowSec;
			
			// Clamp it so we don't accidentally do a massive moon-jump if we jumped horribly off-beat
			TargetAirTime = FMath::Clamp(TargetAirTime, IdealAirTimeSec * 0.75f, IdealAirTimeSec * 1.25f);
		}
	}

	// Apply the dynamically adjusted jump strength!
	Velocity.Z = RhythmGravity * (TargetAirTime * 0.5f);
	SetMovementMode(MOVE_Falling);
}

void UPcQPlayerMovementComponent::ProcessLanded(const FHitResult& Hit, float remainingTime, int32 Iterations) {
	if (MovState == EPlayerMovementState::GroundPounding) {
		PulseImmunityTimer = Cfg_GPImmunityBeats() * GetCurrentBeatIntervalSec();
		const FVector WishDir = Acceleration.GetSafeNormal2D();
		if (WishDir.IsZero()) { MovState = EPlayerMovementState::Grounded; Velocity.X = 0.f; Velocity.Y = 0.f; Velocity.Z = 0.f; }
		else { EnterDash(); PushCombo(TEXT("GP SLAM → DASH"), FLinearColor(1.f, 0.4f, 0.1f)); }
		Super::ProcessLanded(Hit, remainingTime, Iterations);
		if (bJumpInputBuffered && JumpInputBufferTimer > 0.f) {
			bJumpInputBuffered = false; JumpInputBufferTimer = 0.f; ExitDash();
			DoNormalJump();
		}
		return;
	}
	MovState = EPlayerMovementState::Grounded;
	Super::ProcessLanded(Hit, remainingTime, Iterations);
	
	CurrentDJCount = Cfg_MaxDoubleJumps(); 
	
	if (bPulseBufferedForLanding && PulseBufferTimer > 0.f && PulseImmunityTimer <= 0.f) {
		bPulseBufferedForLanding = false; PulseBufferTimer = 0.f;
		GroundPulseBoostTimer = GetAdaptiveTime(Cfg_IdealGroundPulseDuration()); 
		OnGroundPulseHit.Broadcast(); 
		if (bJumpInputBuffered && JumpInputBufferTimer > 0.f) {
			bJumpInputBuffered = false; JumpInputBufferTimer = 0.f;
			DoNormalJump(); 
		}
		return;
	}
	bPulseBufferedForLanding = false;
	
	if (bGPInputBuffered && GPInputBufferTimer > 0.f) {
		bGPInputBuffered = false; GPInputBufferTimer = 0.f; OnGroundPoundPressed(); return;
	}
	if (bJumpInputBuffered && JumpInputBufferTimer > 0.f) {
		bJumpInputBuffered = false; JumpInputBufferTimer = 0.f;
		DoNormalJump();
	}
}

void UPcQPlayerMovementComponent::PhysWalking(float deltaTime, int32 Iterations) {
	if (deltaTime < MIN_TICK_TIME) return;
	if (MovState == EPlayerMovementState::Dashing || MovState == EPlayerMovementState::SwordLunging) {
		FVector Saved = Acceleration; Acceleration = FVector::ZeroVector;
		Super::PhysWalking(deltaTime, Iterations);
		Acceleration = Saved; return;
	}
	const float TargetSpeed = ComputeCurrentMaxSpeed();
	const FVector WishDir = Acceleration.GetSafeNormal2D();
	const FVector Vel2D(Velocity.X, Velocity.Y, 0.f);
	const float CurSpd = Vel2D.Size();
	const float EffTarget = (CurSpd > TargetSpeed && !WishDir.IsZero()) ? CurSpd : TargetSpeed;
	const FVector NewVel2D = FMath::VInterpTo(Vel2D, WishDir * EffTarget, deltaTime, WishDir.IsZero() ? Cfg_GroundFriction() : Cfg_GroundAcceleration());
	Velocity.X = NewVel2D.X; Velocity.Y = NewVel2D.Y;
	FVector Saved = Acceleration; Acceleration = FVector::ZeroVector;
	Super::PhysWalking(deltaTime, Iterations); Acceleration = Saved;
}

void UPcQPlayerMovementComponent::PhysFalling(float deltaTime, int32 Iterations) {
	if (deltaTime < MIN_TICK_TIME) return;
	if (MovState == EPlayerMovementState::GroundPounding) {
		Acceleration = FVector::ZeroVector; Super::PhysFalling(deltaTime, Iterations); return;
	}
	const float TargetAirSpeed = ComputeCurrentMaxSpeed();
	const FVector WishDir = Acceleration.GetSafeNormal2D();
	const FVector Vel2D(Velocity.X, Velocity.Y, 0.f);
	const float CurAirH = Vel2D.Size();
	const float EffTarget = WishDir.IsZero() ? 0.f : FMath::Max(CurAirH, TargetAirSpeed);
	const FVector NewVel2D = FMath::VInterpTo(Vel2D, WishDir * EffTarget, deltaTime, WishDir.IsZero() ? 0.f : Cfg_AirAcceleration());
	Velocity.X = NewVel2D.X; Velocity.Y = NewVel2D.Y;
	FVector Saved = Acceleration; Acceleration = FVector::ZeroVector;
	Super::PhysFalling(deltaTime, Iterations); Acceleration = Saved;
}

void UPcQPlayerMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) {
	
	GravityScale = ComputeRhythmGravity() / FMath::Abs(GetWorld()->GetDefaultGravityZ());
	
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	MaxWalkSpeed = ComputeCurrentMaxSpeed();
	if (IsMovingOnGround() && MovState == EPlayerMovementState::InAir) MovState = EPlayerMovementState::Grounded;
	
	if (MovState == EPlayerMovementState::SwordLunging) {
		SwordLungeTimer -= DeltaTime;
		
		if (SwordLungeTimer <= 0.f) {
			MovState = IsMovingOnGround() ? EPlayerMovementState::Grounded : EPlayerMovementState::InAir;
			Velocity *= 0.4f; 
		} else {
			float RequiredSpeed = Cfg_SlashLungeDistance() / FMath::Max(0.01f, Cfg_SlashLungeDurationSec());
			Velocity = SwordLungeDirection * RequiredSpeed;
			
			FVector Start = CharacterOwner->GetActorLocation();
			FVector End = Start + SwordLungeDirection * FMath::Max(150.f, RequiredSpeed * DeltaTime * 2.f); 
			
			TArray<FHitResult> Hits;
			FCollisionQueryParams QP; QP.AddIgnoredActor(CharacterOwner);
			FCollisionShape Shape = FCollisionShape::MakeSphere(120.f); 
			
			GetWorld()->SweepMultiByChannel(Hits, Start, End, FQuat::Identity, ECC_Visibility, Shape, QP);
			
			bool bHitEnemy = false;

			for(const FHitResult& H : Hits) {
				if (APcQEnemyBase* Enemy = Cast<APcQEnemyBase>(H.GetActor())) {
					OnSwordHitEnemy.Broadcast(Enemy);
					DoSwordBop(false); 
					bHitEnemy = true;
					break; 
				}
			}

			if (!bHitEnemy) {
				for(const FHitResult& H : Hits) {
					if (H.bBlockingHit && H.GetActor() != CharacterOwner) {
						if (H.ImpactNormal.Z > 0.6f) { 
							MovState = EPlayerMovementState::Dashing; 
							DashBoostTimer = GetAdaptiveTime(Cfg_IdealDashDuration());
							DashBoostMaxTime = DashBoostTimer;
							
							Velocity = FVector(Velocity.X, Velocity.Y, 0.f); 
							PushCombo(TEXT("KINETIC SLIDE"), FLinearColor(0.2f, 1.f, 0.8f));
						} else { 
							DoSwordBop(true); 
						}
						break;
					}
				}
			}
		}
	}

	if (MovState == EPlayerMovementState::Dashing && IsMovingOnGround()) {
		const float BoostSpd = ComputeCurrentMaxSpeed() * Cfg_DashBoostSpeedMult();
		const FVector WishDir = Acceleration.GetSafeNormal2D();
		if (!WishDir.IsZero()) {
			const FVector Vel2D(Velocity.X, Velocity.Y, 0.f);
			const FVector New2D = FMath::VInterpTo(Vel2D, WishDir * BoostSpd, DeltaTime, Cfg_DashSteerAccel());
			Velocity.X = New2D.X; Velocity.Y = New2D.Y;
		} else {
			const float CurH = GetHorizontalSpeed();
			const float MaxH = ComputeCurrentMaxSpeed();
			if (CurH > MaxH) {
				const float NewH = FMath::Max(CurH - Cfg_OverspeedDecay() * 3.f * DeltaTime, MaxH);
				const FVector Dir2D = FVector(Velocity.X, Velocity.Y, 0.f).GetSafeNormal();
				if (!Dir2D.IsZero()) { Velocity.X = Dir2D.X * NewH; Velocity.Y = Dir2D.Y * NewH; }
			}
		}
		DashBoostTimer -= DeltaTime;
		if (DashBoostTimer <= 0.f) ExitDash();
	}

	if (MovState != EPlayerMovementState::Dashing && MovState != EPlayerMovementState::SwordLunging) {
		const float CurH = GetHorizontalSpeed();
		const float MaxSpd = ComputeCurrentMaxSpeed();
		const float HardCap = MaxSpd * Cfg_HardSpeedCapMult();
		if (CurH > MaxSpd) {
			const float Excess = CurH - MaxSpd;
			const float DecayThis = Cfg_OverspeedDecay() * DeltaTime * (Excess / MaxSpd);
			const float NewH = FMath::Max(CurH - DecayThis, MaxSpd);
			const FVector Dir2D = FVector(Velocity.X, Velocity.Y, 0.f).GetSafeNormal();
			if (!Dir2D.IsZero()) {
				Velocity.X = Dir2D.X * FMath::Min(NewH, HardCap);
				Velocity.Y = Dir2D.Y * FMath::Min(NewH, HardCap);
			}
		}
	}

	auto Tick = [&](float& T) { if (T > 0.f) T = FMath::Max(0.f, T - DeltaTime); };
	Tick(PulseImmunityTimer); Tick(OnBeatFlashTimer); Tick(GroundPulseBoostTimer);
	if (bPulseBufferedForLanding) { PulseBufferTimer -= DeltaTime; if (PulseBufferTimer <= 0.f) bPulseBufferedForLanding = false; }
	if (bJumpInputBuffered) { JumpInputBufferTimer -= DeltaTime; if (JumpInputBufferTimer <= 0.f) bJumpInputBuffered = false; }
	if (bGPInputBuffered) { GPInputBufferTimer -= DeltaTime; if (GPInputBufferTimer <= 0.f) bGPInputBuffered = false; }
	PreviousFrameSpeed = GetHorizontalSpeed();
}

bool UPcQPlayerMovementComponent::CanBufferLanding() const {
	if (!CharacterOwner || Velocity.Z >= 0.f) return false;
	UCapsuleComponent* Cap = CharacterOwner->GetCapsuleComponent();
	if (!Cap) return false;
	const float CapsuleHalfHeight = Cap->GetUnscaledCapsuleHalfHeight();
	const float CapsuleRadius     = Cap->GetUnscaledCapsuleRadius();
	const float FallDist  = FMath::Abs(Velocity.Z) * Cfg_JumpInputBuffer();
	const float CheckDist = CapsuleHalfHeight + FallDist + 120.f; 
	FHitResult Hit; FCollisionQueryParams P; P.AddIgnoredActor(CharacterOwner);
	FCollisionShape Shape = FCollisionShape::MakeSphere(CapsuleRadius * 0.9f);
	return GetWorld()->SweepSingleByChannel(Hit, CharacterOwner->GetActorLocation(), CharacterOwner->GetActorLocation() - FVector(0.f, 0.f, CheckDist), FQuat::Identity, ECC_WorldStatic, Shape, P);
}

void UPcQPlayerMovementComponent::PushCombo(const FString& Label, FLinearColor Color) { OnComboEvent.Broadcast(Label, Color); }