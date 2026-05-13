#include "PcQPlayerMovementComponent.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "Project_Circle/MusicSystem/MusicGameplaySystem/PcMusicAnalysisSubsystem.h"

UPcQPlayerMovementComponent::UPcQPlayerMovementComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	BrakingFrictionFactor = 0.f; GroundFriction = 0.f;
	bMaintainHorizontalGroundVelocity = true; AirControl = 0.f;
	MaxWalkSpeed = 1000.f; JumpZVelocity = 600.f;
	BrakingDecelerationWalking = 0.f; BrakingDecelerationFalling = 0.f;
	bUseSeparateBrakingFriction = false; BrakingFriction = 0.f;
}

float UPcQPlayerMovementComponent::GetHorizontalSpeed() const { return FVector(Velocity.X, Velocity.Y, 0.f).Size(); }

float UPcQPlayerMovementComponent::GetCurrentBeatIntervalSec() const {
	if (UPcMusicAnalysisSubsystem* Sub = GetWorld() ? GetWorld()->GetSubsystem<UPcMusicAnalysisSubsystem>() : nullptr)
		if (Sub->IsReadyForPlayback()) return Sub->GetGameplayBeatIntervalMS() / 1000.f;
	return 60.f / FMath::Max(Config ? Config->ReferenceBPM : 100.f, 1.f);
}

float UPcQPlayerMovementComponent::GetAdaptiveTime(float IdealTimeSec) const {
	float Interval = GetCurrentBeatIntervalSec();
	if (Interval <= 0.f) return IdealTimeSec; 
	float Beats = IdealTimeSec / Interval;
	float SnappedBeats = FMath::RoundToFloat(Beats * 2.f) / 2.f; 
	return FMath::Max(0.5f, SnappedBeats) * Interval;
}

float UPcQPlayerMovementComponent::GetSmoothScaledTime(float BaseTimeSec) const {
	float CurrentBPM = Config ? Config->ReferenceBPM : 100.f;
	if (UPcMusicAnalysisSubsystem* Sub = GetWorld() ? GetWorld()->GetSubsystem<UPcMusicAnalysisSubsystem>() : nullptr) {
		if (Sub->IsReadyForPlayback()) CurrentBPM = Sub->GetCurrentGameplayBPM();
	}
	float Scale = Config ? FMath::Clamp(CurrentBPM / FMath::Max(Config->ReferenceBPM, 1.f), Config->SpeedScaleMin, Config->SpeedScaleMax) : 1.f;
	return BaseTimeSec / Scale;
}

float UPcQPlayerMovementComponent::ComputeRhythmGravity() const {
	float TargetHeight = Config ? Config->JumpPeakHeightCM : 260.f;
	float TargetTime = GetSmoothScaledTime(Config ? Config->IdealJumpAirTimeSec : 0.85f);
	if (TargetTime <= 0.01f) return FMath::Abs(GetWorld()->GetDefaultGravityZ());
	return (8.f * TargetHeight) / (TargetTime * TargetTime);
}

float UPcQPlayerMovementComponent::ComputeCurrentMaxSpeed() const {
	float Base = Config ? Config->BaseMaxSpeed : 1000.f;
	if (!Config) return Base;
	float CurrentBPM = Config->ReferenceBPM;
	if (UPcMusicAnalysisSubsystem* Sub = GetWorld() ? GetWorld()->GetSubsystem<UPcMusicAnalysisSubsystem>() : nullptr)
		if (Sub->IsReadyForPlayback()) CurrentBPM = Sub->GetCurrentGameplayBPM();
	float Scale = FMath::Clamp(CurrentBPM / FMath::Max(Config->ReferenceBPM, 1.f), Config->SpeedScaleMin, Config->SpeedScaleMax);
	Base *= Scale;
	if (GroundPulseBoostTimer > 0.f && MovState == EPlayerMovementState::Grounded) Base *= 1.4f; 
	return Base;
}

float UPcQPlayerMovementComponent::GetDashActiveAlpha() const { return (DashBoostMaxTime > 0.f) ? FMath::Clamp(DashBoostTimer / DashBoostMaxTime, 0.f, 1.f) : 0.f; }
float UPcQPlayerMovementComponent::GetOnBeatFlash() const { return OnBeatFlashDuration > 0.f ? FMath::Clamp(OnBeatFlashTimer / OnBeatFlashDuration, 0.f, 1.f) : 0.f; }
int32 UPcQPlayerMovementComponent::GetOnBeatWindowMs() const { return Config ? Config->OnBeatWindowMs : 160; }
float UPcQPlayerMovementComponent::GetJumpBufferAlpha() const { return !bJumpInputBuffered ? 0.f : FMath::Clamp(JumpInputBufferTimer / (Config ? Config->JumpInputBufferSec : 0.2f), 0.f, 1.f); }

// RESTORED FIX: The HUD uses these to draw the metronome and screen edges!
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

bool UPcQPlayerMovementComponent::IsNearBeat() const {
	UPcMusicAnalysisSubsystem* Sub = GetWorld() ? GetWorld()->GetSubsystem<UPcMusicAnalysisSubsystem>() : nullptr;
	if (!Sub || !Sub->IsReadyForPlayback()) return false;
	int32 Now = Sub->GetCurrentPlaybackTimeMS();
	int32 Next = Sub->GetNextGameplayBeatTimeMS();
	int32 Interval = FMath::RoundToInt(Sub->GetGameplayBeatIntervalMS());
	return FMath::Min(FMath::Abs(Next - Now), FMath::Abs(Now - (Next - Interval))) <= (Config ? Config->OnBeatWindowMs : 160);
}

void UPcQPlayerMovementComponent::ExecuteDashJump()
{
	FVector Dir2D = Acceleration.GetSafeNormal2D();
	if (Dir2D.IsZero()) Dir2D = FVector(Velocity.X, Velocity.Y, 0.f).GetSafeNormal();

	if (!Dir2D.IsZero()) {
		float Boost = (Config ? Config->DashJumpBoost : 200.f) + (Config ? Config->SuperJumpHorizBoost : 420.f);
		float Cap = (Config ? Config->BaseMaxSpeed : 1000.f) * (Config ? Config->HardSpeedCapMult : 4.f);
		float LaunchH = FMath::Min(GetHorizontalSpeed() + Boost, Cap);
		Velocity.X = Dir2D.X * LaunchH;
		Velocity.Y = Dir2D.Y * LaunchH;
	}
	
	if (MovState == EPlayerMovementState::Dashing) ExitDash();
	if (CharacterOwner) CharacterOwner->JumpCurrentCount = 0;
	MovState = EPlayerMovementState::InAir;

	ApplyJumpVelocity(GetSmoothScaledTime(Config ? Config->IdealJumpAirTimeSec : 0.85f));
	OnSuperJumped.Broadcast();
	PushCombo(TEXT("DASH JUMP"), FLinearColor(1.f, 0.85f, 0.1f)); 
}

void UPcQPlayerMovementComponent::OnJumpPressed()
{
	float Now = GetWorld()->GetTimeSeconds();
	LastJumpTime = Now;

	if (MovState == EPlayerMovementState::Dashing || (Now - LastDashTime <= 0.2f)) {
		ExecuteDashJump();
		return;
	}

	switch (MovState) {
	case EPlayerMovementState::Grounded:
		DoNormalJump();
		break;

	case EPlayerMovementState::InAir:
		if (CurrentDJCount > 0) {
			DoDoubleJump();
		} else if (CanBufferLanding()) {
			bJumpInputBuffered = true; 
			JumpInputBufferTimer = Config ? Config->JumpInputBufferSec : 0.2f;
			Velocity.Z = FMath::Min(Velocity.Z, -(Config ? Config->MagneticSlamDownforce : 2500.f)); 
			OnMagneticSlam.Broadcast(); 
			PushCombo(TEXT("MAGNETIC SLAM"), FLinearColor(1.f, 0.3f, 1.f)); 
		}
		break;
	default: break;
	}
}

void UPcQPlayerMovementComponent::OnJumpReleased() {}

void UPcQPlayerMovementComponent::OnGroundPoundPressed()
{
	if (MovState == EPlayerMovementState::Dashing) return;
	if (MovState == EPlayerMovementState::InAir) {
		if (CanBufferLanding()) { bGPInputBuffered = true; GPInputBufferTimer = Config ? Config->JumpInputBufferSec : 0.2f; return; }
		DoGroundPound(); 
	}
}

void UPcQPlayerMovementComponent::TriggerGroundPulse()
{
	if (PulseImmunityTimer > 0.f) return;
	if (!IsMovingOnGround()) {
		if (IsFalling() && Velocity.Z < 0.f && MovState == EPlayerMovementState::InAir) {
			bPulseBufferedForLanding = true;
			PulseBufferTimer = (Config ? Config->OnBeatWindowMs : 160) / 1000.f;
		}
		return;
	}
	if (MovState != EPlayerMovementState::Grounded) return;
	
	GroundPulseBoostTimer = GetAdaptiveTime(Config ? Config->IdealGroundPulseDurationSec : 0.6f); 
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
	CurrentDJCount = Config ? Config->MaxDoubleJumps : 2;
	PushCombo(TEXT("MOBILITY RESET!"), FLinearColor(0.2f, 1.f, 0.4f));
}

void UPcQPlayerMovementComponent::EnterDash() {
	float Now = GetWorld()->GetTimeSeconds();
	LastDashTime = Now;

	if (!IsMovingOnGround() && (Now - LastJumpTime <= 0.2f)) {
		ExecuteDashJump();
		return;
	}

	FVector Dir2D = Acceleration.GetSafeNormal2D();
	if (Dir2D.IsZero()) Dir2D = FVector(Velocity.X, Velocity.Y, 0.f).GetSafeNormal();
	if (Dir2D.IsZero() && CharacterOwner) Dir2D = CharacterOwner->GetActorForwardVector().GetSafeNormal2D();
	
	float BoostSpd = ComputeCurrentMaxSpeed() * (Config ? Config->DashBoostSpeedMult : 1.55f);
	if (!Dir2D.IsZero()) { Velocity.X = Dir2D.X * BoostSpd; Velocity.Y = Dir2D.Y * BoostSpd; Velocity.Z = 0.f; }
	
	float Duration = GetAdaptiveTime(Config ? Config->IdealDashDurationSec : 0.35f);
	DashBoostTimer = Duration; DashBoostMaxTime = Duration;
	MovState = EPlayerMovementState::Dashing;
	PulseImmunityTimer = FMath::Max(PulseImmunityTimer, Duration + 0.05f);
	OnDashStarted.Broadcast(); PushCombo(TEXT("DASH"), FLinearColor(1.f, 0.55f, 0.15f));
}

void UPcQPlayerMovementComponent::ExitDash() {
	MovState = IsMovingOnGround() ? EPlayerMovementState::Grounded : EPlayerMovementState::InAir;
	DashBoostTimer = 0.f;
	DashBoostMaxTime = 0.f;
	PulseImmunityTimer = FMath::Max(PulseImmunityTimer, (Config ? Config->PostDashImmunityBeats : 0.25f) * GetCurrentBeatIntervalSec());
	OnDashEnded.Broadcast();
}

void UPcQPlayerMovementComponent::DoNormalJump() {
	if (CharacterOwner) CharacterOwner->JumpCurrentCount = 0;
	
	float RhythmGravity = ComputeRhythmGravity();
	float TargetTime = GetSmoothScaledTime(Config ? Config->IdealJumpAirTimeSec : 0.85f);
	
	if (Config && Config->bSyncJumpToBeat) {
		UPcMusicAnalysisSubsystem* Sub = GetWorld() ? GetWorld()->GetSubsystem<UPcMusicAnalysisSubsystem>() : nullptr;
		if (Sub && Sub->IsReadyForPlayback()) {
			float NowSec = Sub->GetCurrentPlaybackTimeMS() / 1000.f;
			float IntervalSec = Sub->GetGameplayBeatIntervalMS() / 1000.f;
			float NextBeatSec = Sub->GetNextGameplayBeatTimeMS() / 1000.f;

			float TargetLanding = NowSec + TargetTime;
			float ClosestBeat = NextBeatSec;
			
			while (ClosestBeat < TargetLanding - (IntervalSec * 0.5f)) ClosestBeat += IntervalSec;
			if (FMath::Abs((ClosestBeat + IntervalSec) - TargetLanding) < FMath::Abs(ClosestBeat - TargetLanding)) ClosestBeat += IntervalSec;
			
			float MagneticTime = ClosestBeat - NowSec;
			TargetTime = FMath::Clamp(MagneticTime, TargetTime * 0.75f, TargetTime * 1.25f);
		}
	}
	
	Velocity.Z = RhythmGravity * (TargetTime * 0.5f);
	SetMovementMode(MOVE_Falling);
	
	MovState = EPlayerMovementState::InAir; 
	PushCombo(TEXT("JUMP"), FLinearColor(0.85f, 0.85f, 0.85f));
}

void UPcQPlayerMovementComponent::DoDoubleJump() {
	if (CharacterOwner) CharacterOwner->JumpCurrentCount = 0;
	ApplyJumpVelocity(GetSmoothScaledTime(Config ? Config->IdealDJAirTimeSec : 0.5f));
	CurrentDJCount--; 
	OnBeatFlashTimer = OnBeatFlashDuration; OnDoubleJumped.Broadcast();
	PushCombo(TEXT("DOUBLE JUMP"), FLinearColor(0.27f, 0.67f, 1.f));
}

void UPcQPlayerMovementComponent::DoGroundPound() {
	MovState = EPlayerMovementState::GroundPounding;
	Velocity.Z = -FMath::Abs(Config ? Config->GPSlamSpeed : 2800.f); Velocity.X *= 0.25f; Velocity.Y *= 0.25f;
	PushCombo(TEXT("GROUND POUND"), FLinearColor(1.f, 0.35f, 0.1f));
}

void UPcQPlayerMovementComponent::ApplyJumpVelocity(float AirTimeSec) {
	if (CharacterOwner) CharacterOwner->JumpCurrentCount = 0;
	Velocity.Z = ComputeRhythmGravity() * (AirTimeSec * 0.5f);
	SetMovementMode(MOVE_Falling);
}

void UPcQPlayerMovementComponent::ProcessLanded(const FHitResult& Hit, float remainingTime, int32 Iterations) {
	if (MovState == EPlayerMovementState::GroundPounding) {
		PulseImmunityTimer = (Config ? Config->GPPulseImmunityBeats : 2.f) * GetCurrentBeatIntervalSec();
		FVector WishDir = Acceleration.GetSafeNormal2D();
		
		if (WishDir.IsZero()) { 
			MovState = EPlayerMovementState::Grounded; Velocity.X = 0.f; Velocity.Y = 0.f; Velocity.Z = 0.f; 
		} else { 
			EnterDash(); PushCombo(TEXT("SLAM → DASH"), FLinearColor(1.f, 0.4f, 0.1f)); 
		}
		
		Super::ProcessLanded(Hit, remainingTime, Iterations);
		if (bJumpInputBuffered && JumpInputBufferTimer > 0.f) {
			bJumpInputBuffered = false; JumpInputBufferTimer = 0.f; ExitDash();
			DoNormalJump();
		}
		return;
	}
	
	MovState = EPlayerMovementState::Grounded;
	Super::ProcessLanded(Hit, remainingTime, Iterations);
	
	CurrentDJCount = Config ? Config->MaxDoubleJumps : 2; 
	
	if (bPulseBufferedForLanding && PulseBufferTimer > 0.f && PulseImmunityTimer <= 0.f) {
		bPulseBufferedForLanding = false; PulseBufferTimer = 0.f;
		GroundPulseBoostTimer = GetAdaptiveTime(Config ? Config->IdealGroundPulseDurationSec : 0.6f); 
		OnGroundPulseHit.Broadcast(); 
		if (bJumpInputBuffered && JumpInputBufferTimer > 0.f) {
			bJumpInputBuffered = false; JumpInputBufferTimer = 0.f; DoNormalJump(); 
		}
		return;
	}
	bPulseBufferedForLanding = false;
	
	if (bGPInputBuffered && GPInputBufferTimer > 0.f) {
		bGPInputBuffered = false; GPInputBufferTimer = 0.f; OnGroundPoundPressed(); return;
	}
	if (bJumpInputBuffered && JumpInputBufferTimer > 0.f) {
		bJumpInputBuffered = false; JumpInputBufferTimer = 0.f; DoNormalJump();
	}
}

void UPcQPlayerMovementComponent::PhysWalking(float deltaTime, int32 Iterations) {
	if (deltaTime < MIN_TICK_TIME) return;
	if (MovState == EPlayerMovementState::Dashing) {
		FVector Saved = Acceleration; Acceleration = FVector::ZeroVector;
		Super::PhysWalking(deltaTime, Iterations);
		Acceleration = Saved; return;
	}
	float TargetSpeed = ComputeCurrentMaxSpeed();
	FVector WishDir = Acceleration.GetSafeNormal2D();
	FVector Vel2D(Velocity.X, Velocity.Y, 0.f);
	float EffTarget = (Vel2D.Size() > TargetSpeed && !WishDir.IsZero()) ? Vel2D.Size() : TargetSpeed;
	FVector NewVel2D = FMath::VInterpTo(Vel2D, WishDir * EffTarget, deltaTime, WishDir.IsZero() ? (Config ? Config->GroundFriction : 25.f) : (Config ? Config->GroundAcceleration : 30.f));
	Velocity.X = NewVel2D.X; Velocity.Y = NewVel2D.Y;
	
	FVector Saved = Acceleration; Acceleration = FVector::ZeroVector;
	Super::PhysWalking(deltaTime, Iterations); Acceleration = Saved;
}

void UPcQPlayerMovementComponent::PhysFalling(float deltaTime, int32 Iterations) {
	if (deltaTime < MIN_TICK_TIME) return;
	if (MovState == EPlayerMovementState::GroundPounding) {
		Acceleration = FVector::ZeroVector; Super::PhysFalling(deltaTime, Iterations); return;
	}
	float TargetAirSpeed = ComputeCurrentMaxSpeed();
	FVector WishDir = Acceleration.GetSafeNormal2D();
	FVector Vel2D(Velocity.X, Velocity.Y, 0.f);
	float EffTarget = WishDir.IsZero() ? 0.f : FMath::Max(Vel2D.Size(), TargetAirSpeed);
	FVector NewVel2D = FMath::VInterpTo(Vel2D, WishDir * EffTarget, deltaTime, WishDir.IsZero() ? 0.f : (Config ? Config->AirAcceleration : 15.f));
	Velocity.X = NewVel2D.X; Velocity.Y = NewVel2D.Y;
	
	FVector Saved = Acceleration; Acceleration = FVector::ZeroVector;
	Super::PhysFalling(deltaTime, Iterations); Acceleration = Saved;
}

void UPcQPlayerMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) {
	
	GravityScale = ComputeRhythmGravity() / FMath::Abs(GetWorld()->GetDefaultGravityZ());
	
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	MaxWalkSpeed = ComputeCurrentMaxSpeed();
	if (IsMovingOnGround() && MovState == EPlayerMovementState::InAir) MovState = EPlayerMovementState::Grounded;

	if (MovState == EPlayerMovementState::Dashing) {
		DashBoostTimer -= DeltaTime;
		if (DashBoostTimer <= 0.f) { ExitDash(); } 
		else if (IsMovingOnGround()) {
			float BoostSpd = ComputeCurrentMaxSpeed() * (Config ? Config->DashBoostSpeedMult : 1.55f);
			FVector WishDir = Acceleration.GetSafeNormal2D();
			if (!WishDir.IsZero()) {
				FVector Vel2D(Velocity.X, Velocity.Y, 0.f);
				FVector New2D = FMath::VInterpTo(Vel2D, WishDir * BoostSpd, DeltaTime, Config ? Config->DashSteerAcceleration : 30.f);
				Velocity.X = New2D.X; Velocity.Y = New2D.Y;
			}
		}
	}

	if (MovState != EPlayerMovementState::Dashing) {
		float CurH = GetHorizontalSpeed();
		float MaxSpd = ComputeCurrentMaxSpeed();
		float HardCap = MaxSpd * (Config ? Config->HardSpeedCapMult : 4.f);
		if (CurH > MaxSpd) {
			float DecayThis = (Config ? Config->OverspeedDecayRate : 200.f) * DeltaTime * ((CurH - MaxSpd) / MaxSpd);
			float NewH = FMath::Max(CurH - DecayThis, MaxSpd);
			FVector Dir2D = FVector(Velocity.X, Velocity.Y, 0.f).GetSafeNormal();
			if (!Dir2D.IsZero()) { Velocity.X = Dir2D.X * FMath::Min(NewH, HardCap); Velocity.Y = Dir2D.Y * FMath::Min(NewH, HardCap); }
		}
	}

	auto Tick = [&](float& T) { if (T > 0.f) T = FMath::Max(0.f, T - DeltaTime); };
	Tick(PulseImmunityTimer); Tick(OnBeatFlashTimer); Tick(GroundPulseBoostTimer);
	if (bPulseBufferedForLanding) { PulseBufferTimer -= DeltaTime; if (PulseBufferTimer <= 0.f) bPulseBufferedForLanding = false; }
	if (bJumpInputBuffered) { JumpInputBufferTimer -= DeltaTime; if (JumpInputBufferTimer <= 0.f) bJumpInputBuffered = false; }
	if (bGPInputBuffered) { GPInputBufferTimer -= DeltaTime; if (GPInputBufferTimer <= 0.f) bGPInputBuffered = false; }
}

bool UPcQPlayerMovementComponent::CanBufferLanding() const {
	if (!CharacterOwner || Velocity.Z >= 0.f) return false;
	UCapsuleComponent* Cap = CharacterOwner->GetCapsuleComponent();
	float FallDist  = FMath::Abs(Velocity.Z) * (Config ? Config->JumpInputBufferSec : 0.2f);
	float CheckDist = Cap->GetUnscaledCapsuleHalfHeight() + FallDist + 120.f; 
	FHitResult Hit; FCollisionQueryParams P; P.AddIgnoredActor(CharacterOwner);
	return GetWorld()->SweepSingleByChannel(Hit, CharacterOwner->GetActorLocation(), CharacterOwner->GetActorLocation() - FVector(0.f, 0.f, CheckDist), FQuat::Identity, ECC_WorldStatic, FCollisionShape::MakeSphere(Cap->GetUnscaledCapsuleRadius() * 0.9f), P);
}

void UPcQPlayerMovementComponent::PushCombo(const FString& Label, FLinearColor Color) { OnComboEvent.Broadcast(Label, Color); }