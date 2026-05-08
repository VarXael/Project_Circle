#include "PcQPlayerMovementComponent.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "Project_Circle/MusicSystem/MusicGameplaySystem/PcMusicAnalysisSubsystem.h"

// =============================================================================
//  CONSTRUCTOR
// =============================================================================

UPcQPlayerMovementComponent::UPcQPlayerMovementComponent()
{
	PrimaryComponentTick.bCanEverTick     = true;
	BrakingFrictionFactor                 = 0.f;
	GroundFriction                        = 0.f;
	bMaintainHorizontalGroundVelocity     = true;
	AirControl                            = 0.f;
	GravityScale                          = 2.8f;
	MaxWalkSpeed                          = 850.f;
	JumpZVelocity                         = 600.f;
	BrakingDecelerationWalking            = 0.f;
	BrakingDecelerationFalling            = 0.f;
	bUseSeparateBrakingFriction           = false;
	BrakingFriction                       = 0.f;
}

// =============================================================================
//  CONFIG ACCESSORS
// =============================================================================

float        UPcQPlayerMovementComponent::Cfg_BaseMaxSpeed()          const { return Config ? Config->BaseMaxSpeed           : 850.f;  }
float        UPcQPlayerMovementComponent::Cfg_GroundAcceleration()    const { return Config ? Config->GroundAcceleration     : 30.f;   }
float        UPcQPlayerMovementComponent::Cfg_GroundFriction()        const { return Config ? Config->GroundFriction         : 25.f;   }
float        UPcQPlayerMovementComponent::Cfg_AirAcceleration()       const { return Config ? Config->AirAcceleration        : 15.f;   }
float        UPcQPlayerMovementComponent::Cfg_GravityScale()          const { return Config ? Config->GravityScale           : 2.8f;   }
float        UPcQPlayerMovementComponent::Cfg_JumpPeakHeight()        const { return Config ? Config->JumpPeakHeightCM       : 260.f;  }
float        UPcQPlayerMovementComponent::Cfg_JumpAirTimeBeats()      const { return Config ? Config->JumpAirTimeBeats       : 1.f;    }
float        UPcQPlayerMovementComponent::Cfg_SuperJumpHorizBoost()   const { return Config ? Config->SuperJumpHorizBoost    : 420.f;  }
float        UPcQPlayerMovementComponent::Cfg_DJPeakHeight()          const { return Config ? Config->DJPeakHeightCM        : 220.f;  }
float        UPcQPlayerMovementComponent::Cfg_DJCooldownBeats()       const { return Config ? Config->DJCooldownBeats        : 2.f;    }
float        UPcQPlayerMovementComponent::Cfg_GPSlamSpeed()           const { return Config ? Config->GPSlamSpeed            : 2800.f; }
float        UPcQPlayerMovementComponent::Cfg_GPImmunityBeats()       const { return Config ? Config->GPPulseImmunityBeats   : 2.f;    }
float        UPcQPlayerMovementComponent::Cfg_DashBoostSpeedMult()    const { return Config ? Config->DashBoostSpeedMult     : 1.55f;  }
float        UPcQPlayerMovementComponent::Cfg_DashDurationBeats()     const { return Config ? Config->DashDurationBeats      : 1.f;    }
float        UPcQPlayerMovementComponent::Cfg_DashSteerAccel()        const { return Config ? Config->DashSteerAcceleration  : 30.f;   }
float        UPcQPlayerMovementComponent::Cfg_DashJumpBoost()         const { return Config ? Config->DashJumpBoost          : 200.f;  }
float        UPcQPlayerMovementComponent::Cfg_PostDashImmunityBeats() const { return Config ? Config->PostDashImmunityBeats  : 0.25f;  }
float        UPcQPlayerMovementComponent::Cfg_HardSpeedCapMult()      const { return Config ? Config->HardSpeedCapMult       : 4.f;    }
float        UPcQPlayerMovementComponent::Cfg_OverspeedDecay()        const { return Config ? Config->OverspeedDecayRate     : 200.f;  }
int32        UPcQPlayerMovementComponent::Cfg_OnBeatWindowMs()        const { return Config ? Config->OnBeatWindowMs         : 160;    }
float        UPcQPlayerMovementComponent::Cfg_JumpInputBuffer()       const { return Config ? Config->JumpInputBufferSec     : 0.22f;  }
UCurveFloat* UPcQPlayerMovementComponent::Cfg_JumpCurve()             const { return Config ? Config->JumpCurve.Get()        : nullptr; }

// =============================================================================
//  COMPUTED VALUES
// =============================================================================

float UPcQPlayerMovementComponent::GetHorizontalSpeed() const
{
	return FVector(Velocity.X, Velocity.Y, 0.f).Size();
}

float UPcQPlayerMovementComponent::GetCurrentBeatIntervalSec() const
{
	if (UPcMusicAnalysisSubsystem* Sub = GetWorld() ? GetWorld()->GetSubsystem<UPcMusicAnalysisSubsystem>() : nullptr)
		if (Sub->IsReadyForPlayback())
			return Sub->GetGameplayBeatIntervalMS() / 1000.f;
	const float RefBPM = Config ? Config->ReferenceBPM : 100.f;
	return 60.f / FMath::Max(RefBPM, 1.f);
}

float UPcQPlayerMovementComponent::ComputeCurrentMaxSpeed() const
{
	const float Base = Cfg_BaseMaxSpeed();
	if (!Config) return Base;
	float CurrentBPM = Config->ReferenceBPM;
	if (UPcMusicAnalysisSubsystem* Sub = GetWorld() ? GetWorld()->GetSubsystem<UPcMusicAnalysisSubsystem>() : nullptr)
		if (Sub->IsReadyForPlayback())
			CurrentBPM = Sub->GetCurrentGameplayBPM();
	const float Scale = FMath::Clamp(CurrentBPM / FMath::Max(Config->ReferenceBPM, 1.f),
	                                  Config->SpeedScaleMin, Config->SpeedScaleMax);
	return Base * Scale;
}

float UPcQPlayerMovementComponent::GetDJCooldownAlpha() const
{
	if (bDJAvailable) return 0.f;
	const float MaxCD = Cfg_DJCooldownBeats() * GetCurrentBeatIntervalSec();
	return MaxCD > 0.f ? FMath::Clamp(DJCooldownTimer / MaxCD, 0.f, 1.f) : 1.f;
}

float UPcQPlayerMovementComponent::GetDashActiveAlpha() const
{
	return (DashBoostMaxTime > 0.f)
		? FMath::Clamp(DashBoostTimer / DashBoostMaxTime, 0.f, 1.f)
		: 0.f;
}

float UPcQPlayerMovementComponent::GetOnBeatFlash() const
{
	return OnBeatFlashDuration > 0.f
		? FMath::Clamp(OnBeatFlashTimer / OnBeatFlashDuration, 0.f, 1.f)
		: 0.f;
}

int32 UPcQPlayerMovementComponent::GetOnBeatWindowMs() const { return Cfg_OnBeatWindowMs(); }

float UPcQPlayerMovementComponent::GetBeatSnappedDuration(float BaseSec) const
{
	UPcMusicAnalysisSubsystem* Sub = GetWorld() ? GetWorld()->GetSubsystem<UPcMusicAnalysisSubsystem>() : nullptr;
	if (!Sub || !Sub->IsReadyForPlayback()) return BaseSec;
	const float IntervalSec = Sub->GetGameplayBeatIntervalMS() / 1000.f;
	if (IntervalSec <= 0.f) return BaseSec;
	const float TimeToNext     = Sub->GetTimeUntilNextGameplayBeat();
	const float AlreadyElapsed = IntervalSec - TimeToNext;
	const int32 WholeBeats     = FMath::CeilToInt(BaseSec / IntervalSec);
	return FMath::Max((float)WholeBeats * IntervalSec - AlreadyElapsed, IntervalSec);
}

float UPcQPlayerMovementComponent::ComputeSyncedAirTime(float AirTimeBeats) const
{
	// Calculates the air time so the player lands on a beat boundary, regardless
	// of when in the beat cycle they jumped.
	//
	// The math: find the nearest beat in time, then compute how long until
	// the beat that is AirTimeBeats steps after it.
	//   - Just past a beat (timeSince <= timeToNext):
	//       airTime = AirTimeBeats * interval - timeSincePrev
	//   - Approaching a beat (timeToNext < timeSince):
	//       airTime = timeToNext + AirTimeBeats * interval
	//
	// Example at 600ms interval, AirTimeBeats=1:
	//   Jump at beat  (timeSince=0,   toNext=600): airTime = 600-0   = 600ms ✓
	//   Jump 80ms late (timeSince=80, toNext=520): airTime = 600-80  = 520ms → lands at +600ms ✓
	//   Jump 80ms early (timeSince=520,toNext=80): airTime = 80+600  = 680ms → lands at +600ms ✓

	UPcMusicAnalysisSubsystem* Sub = GetWorld() ? GetWorld()->GetSubsystem<UPcMusicAnalysisSubsystem>() : nullptr;
	if (!Sub || !Sub->IsReadyForPlayback())
		return AirTimeBeats * GetCurrentBeatIntervalSec();

	const float IntervalSec      = Sub->GetGameplayBeatIntervalMS() / 1000.f;
	if (IntervalSec <= 0.f) return AirTimeBeats * GetCurrentBeatIntervalSec();

	const float TimeToNextSec    = Sub->GetTimeUntilNextGameplayBeat();
	const float TimeSincePrevSec = IntervalSec - TimeToNextSec;

	float AirTime;
	if (TimeSincePrevSec <= TimeToNextSec)
		AirTime = AirTimeBeats * IntervalSec - TimeSincePrevSec; // just past a beat
	else
		AirTime = TimeToNextSec + AirTimeBeats * IntervalSec;   // approaching a beat

	// Never degenerate (shouldn't happen in practice but guard it)
	return FMath::Max(AirTime, IntervalSec * 0.15f);
}

float UPcQPlayerMovementComponent::GetBeatPhase() const
{
	// 0 = beat just fired, 1 = next beat imminent
	UPcMusicAnalysisSubsystem* Sub = GetWorld() ? GetWorld()->GetSubsystem<UPcMusicAnalysisSubsystem>() : nullptr;
	if (!Sub || !Sub->IsReadyForPlayback()) return 0.f;
	const float Interval = Sub->GetGameplayBeatIntervalMS() / 1000.f;
	if (Interval <= 0.f) return 0.f;
	return 1.f - FMath::Clamp(Sub->GetTimeUntilNextGameplayBeat() / Interval, 0.f, 1.f);
}

float UPcQPlayerMovementComponent::GetJumpBufferAlpha() const
{
	if (!bJumpInputBuffered) return 0.f;
	const float MaxBuffer = Cfg_JumpInputBuffer();
	return MaxBuffer > 0.f ? FMath::Clamp(JumpInputBufferTimer / MaxBuffer, 0.f, 1.f) : 0.f;
}

// =============================================================================
//  BEAT HELPER
// =============================================================================

bool UPcQPlayerMovementComponent::IsNearBeat() const
{
	UPcMusicAnalysisSubsystem* Sub = GetWorld() ? GetWorld()->GetSubsystem<UPcMusicAnalysisSubsystem>() : nullptr;
	if (!Sub || !Sub->IsReadyForPlayback()) return false;
	const int32 Now      = Sub->GetCurrentPlaybackTimeMS();
	const int32 Next     = Sub->GetNextGameplayBeatTimeMS();
	const int32 Interval = FMath::RoundToInt(Sub->GetGameplayBeatIntervalMS());
	const int32 Prev     = Next - Interval;
	return FMath::Min(FMath::Abs(Next - Now), FMath::Abs(Now - Prev)) <= Cfg_OnBeatWindowMs();
}

// =============================================================================
//  INPUT HANDLERS
// =============================================================================

void UPcQPlayerMovementComponent::OnJumpPressed()
{
	// ── Jumping out of a dash ─────────────────────────────────────────────────
	if (MovState == EPlayerMovementState::Dashing)
	{
		const FVector WishDir = Acceleration.GetSafeNormal2D();
		if (!WishDir.IsZero())
		{
			const float LaunchH = FMath::Min(GetHorizontalSpeed() + Cfg_DashJumpBoost(),
			                                 Cfg_BaseMaxSpeed() * Cfg_HardSpeedCapMult());
			Velocity.X = WishDir.X * LaunchH;
			Velocity.Y = WishDir.Y * LaunchH;
		}
		ExitDash();
		IsNearBeat() ? DoSuperJump() : DoNormalJump();
		return;
	}

	switch (MovState)
	{
	case EPlayerMovementState::Grounded:
		IsNearBeat() ? DoSuperJump() : DoNormalJump();
		break;

	case EPlayerMovementState::InAir:
		if (CanBufferLanding())
		{
			bJumpInputBuffered   = true;
			JumpInputBufferTimer = Cfg_JumpInputBuffer();
			return;
		}
		if (bDJAvailable && DJCooldownTimer <= 0.f)
			DoDoubleJump();
		break;

	case EPlayerMovementState::GroundPounding:
		bJumpInputBuffered   = true;
		JumpInputBufferTimer = Cfg_JumpInputBuffer();
		break;

	default: break;
	}
}

void UPcQPlayerMovementComponent::OnJumpReleased() {}

void UPcQPlayerMovementComponent::OnGroundPoundPressed()
{
	if (MovState == EPlayerMovementState::Dashing) return; // already dashing

	switch (MovState)
	{
	case EPlayerMovementState::Grounded:
		EnterDash();
		break;

	case EPlayerMovementState::InAir:
		if (CanBufferLanding())
		{
			bGPInputBuffered   = true;
			GPInputBufferTimer = Cfg_JumpInputBuffer();
			return;
		}
		DoGroundPound();
		break;

	case EPlayerMovementState::GroundPounding:
		break;

	default: break;
	}
}

// =============================================================================
//  GROUND PULSE
// =============================================================================

void UPcQPlayerMovementComponent::TriggerGroundPulse()
{
	if (PulseImmunityTimer > 0.f) return;

	if (!IsMovingOnGround())
	{
		// Player is in the air. If they're descending, buffer the pulse so landing
		// within the beat window still triggers the bounce (fixes the "miss on landing" bug).
		if (IsFalling() && Velocity.Z < 0.f && MovState == EPlayerMovementState::InAir)
		{
			bPulseBufferedForLanding = true;
			PulseBufferTimer = (float)Cfg_OnBeatWindowMs() / 1000.f;
		}
		return;
	}

	if (MovState != EPlayerMovementState::Grounded) return;

	// Pre-buffered jump near this beat → super jump.
	if (bJumpInputBuffered && JumpInputBufferTimer > 0.f)
	{
		bJumpInputBuffered   = false;
		JumpInputBufferTimer = 0.f;
		DoSuperJump();
		return;
	}

	DoPulseJump();
}

// =============================================================================
//  GUN FIRED  (on-beat shot: reset dash + DJ)
// =============================================================================

void UPcQPlayerMovementComponent::NotifyGunFired(bool bWasOnBeat)
{
	if (!bWasOnBeat) return;

	OnBeatFlashTimer = OnBeatFlashDuration;

	// ── In air: free double jump ───────────────────────────────────────────────
	// Fires the DJ vertical impulse but does NOT consume bDJAvailable or start
	// the cooldown. Player can immediately press jump to use the "real" DJ after.
	if (MovState == EPlayerMovementState::InAir || MovState == EPlayerMovementState::GroundPounding)
	{
		DoFreeDoubleJump();
		// Also clear any pending DJ cooldown so the real DJ is ready.
		bDJAvailable    = true;
		DJCooldownTimer = 0.f;
		PushCombo(TEXT("PULSE → FREE DJ"), FLinearColor(1.f, 0.85f, 0.1f));
		return;
	}

	// ── Grounded / already dashing: free dash ────────────────────────────────
	// Fires/resets dash timer without consuming any separate resource.
	// Player can press GP again to chain another dash immediately after.
	EnterDash();
	// Keep DJ available as a bonus.
	bDJAvailable    = true;
	DJCooldownTimer = 0.f;
	PushCombo(TEXT("PULSE → FREE DASH"), FLinearColor(1.f, 0.55f, 0.15f));
}

// =============================================================================
//  JUMP EXECUTORS
// =============================================================================

void UPcQPlayerMovementComponent::DoNormalJump()
{
	if (CharacterOwner) CharacterOwner->JumpCurrentCount = 0;
	// Normal jump: fixed arc duration. No beat sync — this is intentionally off-beat.
	ApplyArcWithAirTime(Cfg_JumpPeakHeight(), Cfg_JumpAirTimeBeats() * GetCurrentBeatIntervalSec());
	MovState     = EPlayerMovementState::InAir;
	bDJAvailable = (DJCooldownTimer <= 0.f);
	PushCombo(TEXT("JUMP"), FLinearColor(0.85f, 0.85f, 0.85f));
}

void UPcQPlayerMovementComponent::DoSuperJump()
{
	if (CharacterOwner) CharacterOwner->JumpCurrentCount = 0;
	// Super jump: synced arc — always lands on a beat regardless of press timing.
	ApplyArcWithAirTime(Cfg_JumpPeakHeight(), ComputeSyncedAirTime(Cfg_JumpAirTimeBeats()));

	FVector Dir2D = Acceleration.GetSafeNormal2D();
	if (Dir2D.IsZero()) Dir2D = FVector(Velocity.X, Velocity.Y, 0.f).GetSafeNormal();
	if (!Dir2D.IsZero())
	{
		const float NewH = FMath::Min(GetHorizontalSpeed() + Cfg_SuperJumpHorizBoost(),
		                              Cfg_BaseMaxSpeed() * Cfg_HardSpeedCapMult());
		Velocity.X = Dir2D.X * NewH;
		Velocity.Y = Dir2D.Y * NewH;
	}

	MovState         = EPlayerMovementState::InAir;
	bDJAvailable     = (DJCooldownTimer <= 0.f);
	OnBeatFlashTimer = OnBeatFlashDuration;

	OnSuperJumped.Broadcast();
	PushCombo(TEXT("SUPER JUMP"), FLinearColor(1.f, 0.85f, 0.1f));
}

void UPcQPlayerMovementComponent::DoPulseJump()
{
	if (CharacterOwner) CharacterOwner->JumpCurrentCount = 0;
	// Pulse jump: fired exactly at the beat, so synced time = exactly 1 beat.
	// Still uses ComputeSyncedAirTime as a safety in case of sub-frame drift.
	ApplyArcWithAirTime(Cfg_JumpPeakHeight(), ComputeSyncedAirTime(Cfg_JumpAirTimeBeats()));
	MovState     = EPlayerMovementState::InAir;
	bDJAvailable = (DJCooldownTimer <= 0.f);
}

void UPcQPlayerMovementComponent::DoDoubleJump()
{
	if (!bDJAvailable || DJCooldownTimer > 0.f) return;
	if (CharacterOwner) CharacterOwner->JumpCurrentCount = 0;

	ApplyArcWithAirTime(Cfg_DJPeakHeight(), Cfg_JumpAirTimeBeats() * GetCurrentBeatIntervalSec());
	bDJAvailable    = false;
	DJCooldownTimer = Cfg_DJCooldownBeats() * GetCurrentBeatIntervalSec();

	OnBeatFlashTimer = OnBeatFlashDuration;
	OnDoubleJumped.Broadcast();
	PushCombo(TEXT("DOUBLE JUMP"), FLinearColor(0.27f, 0.67f, 1.f));
}

void UPcQPlayerMovementComponent::DoFreeDoubleJump()
{
	// Same vertical impulse as a real DJ but resource is untouched.
	if (CharacterOwner) CharacterOwner->JumpCurrentCount = 0;
	ApplyArcWithAirTime(Cfg_DJPeakHeight(), Cfg_JumpAirTimeBeats() * GetCurrentBeatIntervalSec());
	// Note: bDJAvailable and DJCooldownTimer are NOT touched here.
	// The caller (NotifyGunFired) sets them ready after this returns.
	OnDoubleJumped.Broadcast();
}

void UPcQPlayerMovementComponent::DoGroundPound()
{
	ExitCurveJump();
	MovState   = EPlayerMovementState::GroundPounding;
	Velocity.Z = -FMath::Abs(Cfg_GPSlamSpeed());
	Velocity.X *= 0.25f;
	Velocity.Y *= 0.25f;
	PushCombo(TEXT("GROUND POUND"), FLinearColor(1.f, 0.35f, 0.1f));
}

// =============================================================================
//  DASH
// =============================================================================

void UPcQPlayerMovementComponent::EnterDash()
{
	// Direction: prefer WASD input, fall back to current velocity, then actor forward.
	FVector Dir2D = Acceleration.GetSafeNormal2D();
	if (Dir2D.IsZero()) Dir2D = FVector(Velocity.X, Velocity.Y, 0.f).GetSafeNormal();
	if (Dir2D.IsZero() && CharacterOwner)
		Dir2D = CharacterOwner->GetActorForwardVector().GetSafeNormal2D();

	// Instant velocity burst to boost speed.
	const float BoostSpd = ComputeCurrentMaxSpeed() * Cfg_DashBoostSpeedMult();
	if (!Dir2D.IsZero())
	{
		Velocity.X = Dir2D.X * BoostSpd;
		Velocity.Y = Dir2D.Y * BoostSpd;
		Velocity.Z = 0.f;
	}

	// Timer is a fixed N-beat duration, not beat-snapped to the next boundary.
	// This way every dash feels the same length regardless of when you triggered it.
	const float Duration  = Cfg_DashDurationBeats() * GetCurrentBeatIntervalSec();
	DashBoostTimer   = Duration;
	DashBoostMaxTime = Duration;

	MovState = EPlayerMovementState::Dashing;

	// Immune to pulse for the dash duration (pulse can't bounce you mid-dash).
	PulseImmunityTimer = FMath::Max(PulseImmunityTimer, Duration + 0.05f);

	OnDashStarted.Broadcast();
	PushCombo(TEXT("DASH"), FLinearColor(1.f, 0.55f, 0.15f));
}

void UPcQPlayerMovementComponent::ExitDash()
{
	MovState       = EPlayerMovementState::Grounded;
	DashBoostTimer = 0.f;

	// Brief grace so the next pulse doesn't immediately bounce the player.
	const float Grace = Cfg_PostDashImmunityBeats() * GetCurrentBeatIntervalSec();
	PulseImmunityTimer = FMath::Max(PulseImmunityTimer, Grace);

	OnDashEnded.Broadcast();
}

// =============================================================================
//  ARC JUMP MATH
// =============================================================================

void UPcQPlayerMovementComponent::ApplyArcWithAirTime(float PeakHeightCM, float AirTimeSec)
{
	if (CharacterOwner) CharacterOwner->JumpCurrentCount = 0;

	UCurveFloat* Curve = Cfg_JumpCurve();
	if (Curve)
	{
		JumpCurveTimer      = 0.f;
		JumpCurveTotalTime  = AirTimeSec;
		JumpCurvePeakHeight = PeakHeightCM;
		GravityScale        = 0.f;
		bUsingJumpCurve     = true;
		const float H0 = Curve->GetFloatValue(0.f)    * PeakHeightCM;
		const float H1 = Curve->GetFloatValue(0.001f) * PeakHeightCM;
		Velocity.Z = (H1 - H0) / (0.001f * AirTimeSec);
	}
	else
	{
		const float T = AirTimeSec * 0.5f;
		const float G = (2.f * PeakHeightCM) / (T * T);
		GravityScale  = G / FMath::Abs(GetWorld()->GetDefaultGravityZ());
		Velocity.Z    = G * T;
	}
	SetMovementMode(MOVE_Falling);
}

void UPcQPlayerMovementComponent::ExitCurveJump()
{
	if (!bUsingJumpCurve) return;
	bUsingJumpCurve = false;
	GravityScale    = Cfg_GravityScale();
}

// =============================================================================
//  PROCESS LANDED
// =============================================================================

void UPcQPlayerMovementComponent::ProcessLanded(const FHitResult& Hit, float remainingTime, int32 Iterations)
{
	ExitCurveJump();
	GravityScale = Cfg_GravityScale();

	// ── Ground pound landing → auto-enter dash ────────────────────────────────
	if (MovState == EPlayerMovementState::GroundPounding)
	{
		// Set GP pulse immunity first (overrides dash immunity since GP beats are longer).
		PulseImmunityTimer = Cfg_GPImmunityBeats() * GetCurrentBeatIntervalSec();

		// Enter dash — sets velocity and its own timer.
		const FVector WishDir = Acceleration.GetSafeNormal2D();
		if (WishDir.IsZero())
		{
			// No directional input: just set grounded, don't dash.
			MovState = EPlayerMovementState::Grounded;
			Velocity.X = 0.f; Velocity.Y = 0.f; Velocity.Z = 0.f;
		}
		else
		{
			EnterDash(); // EnterDash reads Acceleration.GetSafeNormal2D() internally
			PushCombo(TEXT("GP SLAM → DASH"), FLinearColor(1.f, 0.4f, 0.1f));
		}

		Super::ProcessLanded(Hit, remainingTime, Iterations);

		if (bJumpInputBuffered && JumpInputBufferTimer > 0.f)
		{
			bJumpInputBuffered = false; JumpInputBufferTimer = 0.f;
			ExitDash();
			IsNearBeat() ? DoSuperJump() : DoNormalJump();
		}
		return;
	}

	// ── Normal landing ────────────────────────────────────────────────────────
	MovState = EPlayerMovementState::Grounded;
	Super::ProcessLanded(Hit, remainingTime, Iterations);

	// Buffered pulse: the beat fired just before we landed — trigger it now.
	if (bPulseBufferedForLanding && PulseBufferTimer > 0.f && PulseImmunityTimer <= 0.f)
	{
		bPulseBufferedForLanding = false;
		PulseBufferTimer         = 0.f;

		// If the player also has a jump buffered, make it a super jump.
		if (bJumpInputBuffered && JumpInputBufferTimer > 0.f)
		{
			bJumpInputBuffered = false; JumpInputBufferTimer = 0.f;
			DoSuperJump();
			return;
		}
		DoPulseJump();
		return;
	}
	bPulseBufferedForLanding = false;

	if (bGPInputBuffered && GPInputBufferTimer > 0.f)
	{
		bGPInputBuffered = false; GPInputBufferTimer = 0.f;
		OnGroundPoundPressed();
		return;
	}
	if (bJumpInputBuffered && JumpInputBufferTimer > 0.f)
	{
		bJumpInputBuffered = false; JumpInputBufferTimer = 0.f;
		IsNearBeat() ? DoSuperJump() : DoNormalJump();
	}
}

// =============================================================================
//  PHYSICS — WALKING
// =============================================================================

void UPcQPlayerMovementComponent::PhysWalking(float deltaTime, int32 Iterations)
{
	if (deltaTime < MIN_TICK_TIME) return;

	if (MovState == EPlayerMovementState::Dashing)
	{
		// Dash velocity is driven in TickComponent (like the old boost).
		// PhysWalking just handles floor geometry with zero acceleration.
		FVector Saved = Acceleration; Acceleration = FVector::ZeroVector;
		Super::PhysWalking(deltaTime, Iterations);
		Acceleration = Saved;
		return;
	}

	// ── Normal walking ────────────────────────────────────────────────────────
	const float   TargetSpeed = ComputeCurrentMaxSpeed();
	const FVector WishDir     = Acceleration.GetSafeNormal2D();
	const FVector Vel2D(Velocity.X, Velocity.Y, 0.f);
	const float   CurSpd      = Vel2D.Size();
	const float   EffTarget   = (CurSpd > TargetSpeed && !WishDir.IsZero()) ? CurSpd : TargetSpeed;

	const FVector NewVel2D = FMath::VInterpTo(Vel2D, WishDir * EffTarget, deltaTime,
	                          WishDir.IsZero() ? Cfg_GroundFriction() : Cfg_GroundAcceleration());
	Velocity.X = NewVel2D.X;
	Velocity.Y = NewVel2D.Y;

	FVector Saved = Acceleration; Acceleration = FVector::ZeroVector;
	Super::PhysWalking(deltaTime, Iterations);
	Acceleration = Saved;
}

// =============================================================================
//  PHYSICS — FALLING
// =============================================================================

void UPcQPlayerMovementComponent::PhysFalling(float deltaTime, int32 Iterations)
{
	if (deltaTime < MIN_TICK_TIME) return;

	if (MovState == EPlayerMovementState::GroundPounding)
	{
		Acceleration = FVector::ZeroVector;
		Super::PhysFalling(deltaTime, Iterations);
		return;
	}

	const float   TargetAirSpeed = ComputeCurrentMaxSpeed();
	const FVector WishDir        = Acceleration.GetSafeNormal2D();
	const FVector Vel2D(Velocity.X, Velocity.Y, 0.f);
	const float   CurAirH        = Vel2D.Size();

	const float   EffTarget = WishDir.IsZero() ? 0.f : FMath::Max(CurAirH, TargetAirSpeed);
	const FVector NewVel2D  = FMath::VInterpTo(Vel2D, WishDir * EffTarget, deltaTime,
	                           WishDir.IsZero() ? 0.f : Cfg_AirAcceleration());
	Velocity.X = NewVel2D.X;
	Velocity.Y = NewVel2D.Y;

	FVector Saved = Acceleration; Acceleration = FVector::ZeroVector;
	Super::PhysFalling(deltaTime, Iterations);
	Acceleration = Saved;
}

// =============================================================================
//  TICK
// =============================================================================

void UPcQPlayerMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                                 FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	MaxWalkSpeed = ComputeCurrentMaxSpeed();

	// ── Safety: sync state with CMC mode ─────────────────────────────────────
	if (IsMovingOnGround() && MovState == EPlayerMovementState::InAir && !bUsingJumpCurve)
		MovState = EPlayerMovementState::Grounded;

	// ── Dash: update velocity each tick (exactly like old boost) ─────────────
	if (MovState == EPlayerMovementState::Dashing && IsMovingOnGround())
	{
		const float   BoostSpd  = ComputeCurrentMaxSpeed() * Cfg_DashBoostSpeedMult();
		const FVector WishDir   = Acceleration.GetSafeNormal2D();

		if (!WishDir.IsZero())
		{
			// Responsive steering toward wish direction at boost speed.
			const FVector Vel2D(Velocity.X, Velocity.Y, 0.f);
			const FVector New2D = FMath::VInterpTo(Vel2D, WishDir * BoostSpd, DeltaTime, Cfg_DashSteerAccel());
			Velocity.X = New2D.X;
			Velocity.Y = New2D.Y;
		}
		else
		{
			// No input: decay speed toward MaxWalkSpeed rather than to zero.
			const float CurH = GetHorizontalSpeed();
			const float MaxH = ComputeCurrentMaxSpeed();
			if (CurH > MaxH)
			{
				const float NewH   = FMath::Max(CurH - Cfg_OverspeedDecay() * 3.f * DeltaTime, MaxH);
				const FVector Dir2D = FVector(Velocity.X, Velocity.Y, 0.f).GetSafeNormal();
				if (!Dir2D.IsZero()) { Velocity.X = Dir2D.X * NewH; Velocity.Y = Dir2D.Y * NewH; }
			}
		}

		DashBoostTimer -= DeltaTime;
		if (DashBoostTimer <= 0.f) ExitDash();
	}

	// ── Curve jump ────────────────────────────────────────────────────────────
	if (bUsingJumpCurve)
	{
		UCurveFloat* Curve = Cfg_JumpCurve();
		if (Curve && JumpCurveTotalTime > 0.f)
		{
			JumpCurveTimer = FMath::Min(JumpCurveTimer + DeltaTime, JumpCurveTotalTime);
			const float T     = JumpCurveTimer / JumpCurveTotalTime;
			const float TNext = FMath::Min((JumpCurveTimer + DeltaTime) / JumpCurveTotalTime, 1.f);
			Velocity.Z = (Curve->GetFloatValue(TNext) - Curve->GetFloatValue(T))
			             * JumpCurvePeakHeight / DeltaTime;
			if (JumpCurveTimer >= JumpCurveTotalTime)
			{
				constexpr float BS = 0.005f;
				const float ExitVZ = (Curve->GetFloatValue(1.f) - Curve->GetFloatValue(1.f - BS))
				                     * JumpCurvePeakHeight / (BS * JumpCurveTotalTime);
				ExitCurveJump();
				Velocity.Z = FMath::Min(ExitVZ, -80.f);
			}
		}
		else ExitCurveJump();
	}

	// ── Overspeed decay (outside of dash) ─────────────────────────────────────
	if (MovState != EPlayerMovementState::Dashing)
	{
		const float CurH    = GetHorizontalSpeed();
		const float MaxSpd  = ComputeCurrentMaxSpeed();
		const float HardCap = MaxSpd * Cfg_HardSpeedCapMult();
		if (CurH > MaxSpd)
		{
			const float Excess    = CurH - MaxSpd;
			const float DecayThis = Cfg_OverspeedDecay() * DeltaTime * (Excess / MaxSpd);
			const float NewH      = FMath::Max(CurH - DecayThis, MaxSpd);
			const FVector Dir2D   = FVector(Velocity.X, Velocity.Y, 0.f).GetSafeNormal();
			if (!Dir2D.IsZero())
			{
				Velocity.X = Dir2D.X * FMath::Min(NewH, HardCap);
				Velocity.Y = Dir2D.Y * FMath::Min(NewH, HardCap);
			}
		}
	}

	// ── Timers ────────────────────────────────────────────────────────────────
	auto Tick = [&](float& T) { if (T > 0.f) T = FMath::Max(0.f, T - DeltaTime); };
	Tick(PulseImmunityTimer);
	Tick(OnBeatFlashTimer);
	if (bPulseBufferedForLanding) { PulseBufferTimer -= DeltaTime; if (PulseBufferTimer <= 0.f) bPulseBufferedForLanding = false; }

	if (DJCooldownTimer > 0.f)
	{
		DJCooldownTimer = FMath::Max(0.f, DJCooldownTimer - DeltaTime);
		if (DJCooldownTimer <= 0.f) bDJAvailable = true;
	}

	if (bJumpInputBuffered) { JumpInputBufferTimer -= DeltaTime; if (JumpInputBufferTimer <= 0.f) bJumpInputBuffered = false; }
	if (bGPInputBuffered)   { GPInputBufferTimer   -= DeltaTime; if (GPInputBufferTimer   <= 0.f) bGPInputBuffered   = false; }

	PreviousFrameSpeed = GetHorizontalSpeed();
}

// =============================================================================
//  HELPERS
// =============================================================================

bool UPcQPlayerMovementComponent::CanBufferLanding() const
{
	if (!CharacterOwner || Velocity.Z >= 0.f) return false;
	float CapsuleHalfHeight = 90.f;
	if (UCapsuleComponent* Cap = CharacterOwner->GetCapsuleComponent())
		CapsuleHalfHeight = Cap->GetUnscaledCapsuleHalfHeight();
	const float CheckDist = CapsuleHalfHeight + FMath::Abs(Velocity.Z) * Cfg_JumpInputBuffer() + 50.f;
	FHitResult Hit; FCollisionQueryParams P; P.AddIgnoredActor(CharacterOwner);
	return GetWorld()->LineTraceSingleByChannel(Hit, CharacterOwner->GetActorLocation(),
	       CharacterOwner->GetActorLocation() - FVector(0.f, 0.f, CheckDist), ECC_WorldStatic, P);
}

void UPcQPlayerMovementComponent::PushCombo(const FString& Label, FLinearColor Color)
{
	OnComboEvent.Broadcast(Label, Color);
}