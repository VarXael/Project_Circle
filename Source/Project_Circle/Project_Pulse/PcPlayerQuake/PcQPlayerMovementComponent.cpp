#include "PcQPlayerMovementComponent.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "Project_Circle/MusicSystem/MusicGameplaySystem/PcMusicAnalysisSubsystem.h"

UPcQPlayerMovementComponent::UPcQPlayerMovementComponent()
{
	PrimaryComponentTick.bCanEverTick         = true;
	BrakingFrictionFactor                     = 0.f;
	GroundFriction                            = 0.f;
	bMaintainHorizontalGroundVelocity         = true;
	AirControl                                = 0.f;
	GravityScale                              = 1.0f;
	MaxWalkSpeed                              = 900.f;
	JumpZVelocity                             = 600.f;
	BrakingDecelerationWalking                = 0.f;
	BrakingDecelerationFalling                = 0.f;
	bUseSeparateBrakingFriction               = false;
	BrakingFriction                           = 0.f;
}

float UPcQPlayerMovementComponent::GetHorizontalSpeed() const { return FVector(Velocity.X, Velocity.Y, 0.f).Size(); }
bool UPcQPlayerMovementComponent::IsInBhopChain() const { return GetHorizontalSpeed() > MaxWalkSpeed * 1.05f; }

bool UPcQPlayerMovementComponent::IsOnBeat() const
{
	UPcMusicAnalysisSubsystem* Sub = GetWorld()->GetSubsystem<UPcMusicAnalysisSubsystem>();
	if (!Sub || !Sub->IsReadyForPlayback()) return false;
	const int32 Now      = Sub->GetCurrentPlaybackTimeMS();
	const int32 Next     = Sub->GetNextGameplayBeatTimeMS();
	const int32 Interval = FMath::RoundToInt(Sub->GetGameplayBeatIntervalMS());
	// Window scales with the beat interval — stays proportional at any BPM.
	// At 75 BPM (800ms) 0.20 = 160ms. At 150 BPM (400ms) 0.20 = 80ms.
	const int32 Window   = FMath::RoundToInt(Interval * OnBeatWindowFraction);
	return FMath::Min(FMath::Abs(Next - Now), FMath::Abs(Now - (Next - Interval))) <= Window;
}

float UPcQPlayerMovementComponent::GetBeatSnappedDuration(float BaseSec) const
{
	UPcMusicAnalysisSubsystem* Sub = GetWorld()->GetSubsystem<UPcMusicAnalysisSubsystem>();
	if (!Sub || !Sub->IsReadyForPlayback()) return BaseSec;
	const float IntervalSec = Sub->GetGameplayBeatIntervalMS() / 1000.f;
	if (IntervalSec <= 0.001f) return BaseSec; // SAFEGUARD
	const float TimeToNext = Sub->GetTimeUntilNextGameplayBeat();
	const float AlreadyElapsed = IntervalSec - TimeToNext;
	const int32 WholeBeats = FMath::CeilToInt(BaseSec / IntervalSec);
	return FMath::Max((float)WholeBeats * IntervalSec - AlreadyElapsed, IntervalSec);
}

// SCALING FUNCTION: Mathematically scales speed based on the tempo to ensure exact physical distances
float UPcQPlayerMovementComponent::GetScaledSpeed(float BaseSpeed) const
{
	if (UPcMusicAnalysisSubsystem* Sub = GetWorld()->GetSubsystem<UPcMusicAnalysisSubsystem>()) {
		if (Sub->IsReadyForPlayback()) {
			float Interval = Sub->GetGameplayBeatIntervalMS() / 1000.f;
			if (Interval > 0.01f && ReferenceBeatInterval > 0.01f) {
				return BaseSpeed * (ReferenceBeatInterval / Interval);
			}
		}
	}
	return BaseSpeed;
}

int32 UPcQPlayerMovementComponent::GetOnBeatWindowMs() const
{
	if (UPcMusicAnalysisSubsystem* Sub = GetWorld()
		? GetWorld()->GetSubsystem<UPcMusicAnalysisSubsystem>() : nullptr)
	{
		if (Sub->IsReadyForPlayback())
			return FMath::RoundToInt(Sub->GetGameplayBeatIntervalMS() * OnBeatWindowFraction);
	}
	return FMath::RoundToInt(800.f * OnBeatWindowFraction); // 75 BPM fallback
}

float UPcQPlayerMovementComponent::GetBoostCooldownAlpha() const { return BoostCooldown <= 0.f ? 0.f : FMath::Clamp(BoostCooldown / FMath::Max(BoostBaseCooldownSec, 0.01f), 0.f, 1.f); }
float UPcQPlayerMovementComponent::GetBoostActiveAlpha() const { if (BhopState != EBhopState::PowerBoost || BoostTimer <= 0.f) return 0.f; return FMath::Clamp(BoostTimer / FMath::Max(BoostBaseDurationSec, 0.01f), 0.f, 1.f); }
float UPcQPlayerMovementComponent::GetDoubleJumpCooldownAlpha() const { if (DoubleJumpCooldown <= 0.f) return bDoubleJumpUsed ? 1.f : 0.f; return FMath::Clamp(DoubleJumpCooldown / FMath::Max(DoubleJumpBaseCooldownSec, 0.01f), 0.f, 1.f); }

// CHANGE 1: Removed SLIDE PUMP block — ground GP is now GP Pulse, not a boost state
void UPcQPlayerMovementComponent::TriggerOnBeatFlash()
{
	RecordHit();
	OnBeatFlashTimer = OnBeatFlashDuration;

	// CD resets are gated to once per beat cycle so the same action can't be
	// spammed within the beat window. The first on-beat action gets the full
	// reward; any subsequent ones in the same window still register but don't
	// loop-reset cooldowns.
	bool bGrantReset = false;
	if (UPcMusicAnalysisSubsystem* Sub = GetWorld()->GetSubsystem<UPcMusicAnalysisSubsystem>())
	{
		if (Sub->IsReadyForPlayback())
		{
			const int32 NextBeat = Sub->GetNextGameplayBeatTimeMS();
			if (NextBeat != LastBeatResetTimestampMS)
			{
				LastBeatResetTimestampMS = NextBeat;
				bGrantReset = true;
			}
		}
	}
	else { bGrantReset = true; } // no subsystem — always grant (editor / standalone)

	if (bGrantReset)
	{
		BoostCooldown      = 0.f;
		DoubleJumpCooldown = 0.f;
		bDoubleJumpUsed    = false;
		bFreeChargeUsedThisBeat = false; // new beat — free charge available

		FrenzyGauge = FMath::Min(1.f, FrenzyGauge + FrenzyFillOnBeat);

		if (bSTierActive && bSTierLocked)
			STierLockTimer = STierLockSec;

		if (!bSTierActive && FrenzyGauge >= FrenzyThresh_S)
		{
			if (UPcMusicAnalysisSubsystem* Sub = GetWorld()->GetSubsystem<UPcMusicAnalysisSubsystem>())
			{
				if (Sub->IsEnhanced())
				{
					bSTierActive   = true;
					bSTierLocked   = true;
					STierLockTimer = STierLockSec;
					PushCombo(TEXT("FRENZY S ★"), FLinearColor(1.f, 0.15f, 0.2f));
				}
			}
		}
	}

	OnActiveBeatAction.Broadcast();
}

void UPcQPlayerMovementComponent::PushCombo(const FString& Label, FLinearColor Color)
{
	OnComboEvent.Broadcast(Label, Color);
	// Every action the player takes spikes the player wave equally.
	// We don't weight actions — intent expressed through timing is what matters.
	PlayerPulse = FMath::Min(PlayerPulse + 0.85f, 1.5f);  // spike, allow slight overshoot
}

void UPcQPlayerMovementComponent::RecordHit()
{
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	HitTimestamps[HitWriteIdx % HitHistorySize] = Now;
	HitWriteIdx++;
	HitCount = FMath::Min(HitCount + 1, HitHistorySize);

	if (HitCount >= 2)
	{
		float Total = 0.f; int32 Pairs = 0;
		for (int32 i = 1; i < HitCount; ++i)
		{
			const int32 A = (HitWriteIdx - i - 1 + HitHistorySize) % HitHistorySize;
			const int32 B = (HitWriteIdx - i     + HitHistorySize) % HitHistorySize;
			const float Dt = HitTimestamps[B] - HitTimestamps[A];
			if (Dt > 0.05f && Dt < 3.f) { Total += Dt; Pairs++; }
		}
		if (Pairs > 0) PlayerBPM = 60.f / (Total / Pairs);
	}
}

// CHANGE 2: GPPulse replaces Slide in snap state — PowerBoost stays as Jump fallthrough
ESnapAction UPcQPlayerMovementComponent::GetActiveSnap() const
{
	if (IsFalling()) return CanBufferLanding() ? ESnapAction::LandingJump : ESnapAction::DoubleJump;
	if (bGPPulseActive) return ESnapAction::GPPulse;
	return ESnapAction::Jump;
}

void UPcQPlayerMovementComponent::ActivateBoost()
{
	BhopState     = EBhopState::PowerBoost;
	BoostTimer    = GetBeatSnappedDuration(BoostBaseDurationSec);
	BoostCooldown = 0.f;

	FVector Dir2D = FVector(Velocity.X, Velocity.Y, 0.f).GetSafeNormal();
	if (Dir2D.IsZero() && CharacterOwner) Dir2D = CharacterOwner->GetActorForwardVector().GetSafeNormal2D();

	Velocity.X = Dir2D.X * MaxWalkSpeed * BoostSpeedMultiplier;
	Velocity.Y = Dir2D.Y * MaxWalkSpeed * BoostSpeedMultiplier;
	Velocity.Z = 0.f;

	TriggerOnBeatFlash();
	PushCombo(TEXT("BOOST"), FLinearColor(1.f, 0.55f, 0.15f));
}

void UPcQPlayerMovementComponent::ExitBoost()
{
	BhopState     = EBhopState::Active;
	BoostTimer    = 0.f;
	BoostCooldown = GetBeatSnappedDuration(BoostBaseCooldownSec);
}

void UPcQPlayerMovementComponent::NotifyGunFired()
{
	// Gun and snap are equivalent: both reset CDs and trigger the movement echo.
	// Echo: jump on ground, GP Pulse if pulse active, DJ in air.
	TriggerOnBeatFlash();
	OnSnapPressed_Internal();

	if (BhopState == EBhopState::PowerBoost)
		PushCombo(TEXT("BOOST RESET"), FLinearColor(1.f, 0.55f, 0.15f));
	if (bDoubleJumpUsed)
		PushCombo(TEXT("DJ RESET"), FLinearColor(0.27f, 0.67f, 1.f));
}

// CHANGE 3: ESnapAction::Slide case replaced with ESnapAction::GPPulse
void UPcQPlayerMovementComponent::OnSnapPressed_Internal()
{
	SnapPulseTimer = 0.15f;

	switch (GetActiveSnap())
	{
		case ESnapAction::GPPulse:
		{
			// Re-trigger at full on-beat power. ActivateGPPulse calls TriggerOnBeatFlash internally.
			ActivateGPPulse(true);
			OnSnapPulse.Broadcast(ESnapAction::GPPulse);
			OnSnapStateChanged.Broadcast(ESnapAction::GPPulse);
			break;
		}
		case ESnapAction::Jump:
		{
			ExecutePlayerJump(true);
			OnBhopLanded.Broadcast(GetHorizontalSpeed());
			OnSnapPulse.Broadcast(ESnapAction::Jump);
			OnSnapStateChanged.Broadcast(ESnapAction::Jump);
			PushCombo(TEXT("SNAP JUMP"), FLinearColor(0.8f, 1.f, 0.4f));
			break;
		}
		case ESnapAction::LandingJump:
		{
			bJumpInputBuffered   = true;
			JumpInputBufferTimer = JumpInputBufferWindow;
			OnSnapPulse.Broadcast(ESnapAction::LandingJump);
			OnSnapStateChanged.Broadcast(ESnapAction::LandingJump);
			PushCombo(TEXT("SYNC"), FLinearColor(1.f, 0.9f, 0.3f));
			break;
		}
		case ESnapAction::DoubleJump:
		{
			bDoubleJumpUsed    = true;
			DoubleJumpCooldown = GetBeatSnappedDuration(DoubleJumpBaseCooldownSec);
			const FVector Dir2D = FVector(Velocity.X, Velocity.Y, 0.f).GetSafeNormal();
			float ScaledHopSpeed = GetScaledSpeed(BonusHopSpeedBoost);
			if (!Dir2D.IsZero()) { Velocity.X += Dir2D.X * ScaledHopSpeed; Velocity.Y += Dir2D.Y * ScaledHopSpeed; }
			ExecutePlayerJump(false);
			// Free charge: grant one more DJ only if not already used this beat
			if (!bFreeChargeUsedThisBeat) {
				bFreeChargeUsedThisBeat = true;
				bDoubleJumpUsed    = false;
				DoubleJumpCooldown = 0.f;
			}
			OnSnapPulse.Broadcast(ESnapAction::DoubleJump);
			OnSnapStateChanged.Broadcast(ESnapAction::DoubleJump);
			PushCombo(TEXT("SNAP DJ"), FLinearColor(0.4f, 0.75f, 1.f));
			break;
		}
		default: break;
	}
}

void UPcQPlayerMovementComponent::ExecutePlayerJump(bool bFromBoost)
{
	const float PreJumpH  = GetHorizontalSpeed();
	const FVector Dir2D   = FVector(Velocity.X, Velocity.Y, 0.f).GetSafeNormal();

	ApplyFixedBeatJump();

	if (!Dir2D.IsZero() && GetHorizontalSpeed() < PreJumpH) {
		Velocity.X = Dir2D.X * PreJumpH;
		Velocity.Y = Dir2D.Y * PreJumpH;
	}

	if (bFromBoost) {
		FVector WD = Acceleration.GetSafeNormal2D();
		if (WD.IsZero()) WD = Dir2D;
		if (WD.IsZero() && CharacterOwner) WD = CharacterOwner->GetActorForwardVector().GetSafeNormal2D();
		
		if (!WD.IsZero()) {
			const float LaunchSpd = FMath::Max(PreJumpH, MaxWalkSpeed * BoostSpeedMultiplier) + GetScaledSpeed(BoostJumpExtraSpeed);
			Velocity.X = WD.X * LaunchSpd;
			Velocity.Y = WD.Y * LaunchSpd;
		}
		PushCombo(TEXT("BOOST JUMP"), FLinearColor(1.f, 0.55f, 0.15f));
	}
	else if (bGPLandedRecently && GPComboTimer > 0.f) {
		const FVector WishDir = Acceleration.GetSafeNormal2D().IsZero() ? Dir2D : Acceleration.GetSafeNormal2D();
		
		float Bonus = GetScaledSpeed(GPJumpHorizBoost);
		if (bBoostActiveOnGPLand) Bonus *= GPBoostJumpHorizMult;

		Velocity.X += WishDir.X * Bonus;
		Velocity.Y += WishDir.Y * Bonus;

		bGPLandedRecently    = false;
		bBoostActiveOnGPLand = false;
		GPComboTimer         = 0.f;

		TriggerOnBeatFlash();
		const bool bWasBoostChain = (Bonus > GetScaledSpeed(GPJumpHorizBoost));
		PushCombo(bWasBoostChain ? TEXT("GP BOOST JUMP") : TEXT("GP COMBO JUMP"), FLinearColor(1.f, 0.9f, 0.15f));
	}

	if (bBonusHopRequested) {
		const FVector WD = Acceleration.GetSafeNormal2D().IsZero() ? Dir2D : Acceleration.GetSafeNormal2D();
		float ScaledHop = GetScaledSpeed(BonusHopSpeedBoost);
		Velocity.X += WD.X * ScaledHop;
		Velocity.Y += WD.Y * ScaledHop;
		bBonusHopRequested = false;
		TriggerOnBeatFlash();
		PushCombo(TEXT("BEAT JUMP"), FLinearColor(1.f, 1.f, 1.f));
	}

	const float HardCap = MaxWalkSpeed * HardSpeedCapMult;
	const float FinalH  = GetHorizontalSpeed();
	if (FinalH > HardCap) {
		const FVector FinalDir = FVector(Velocity.X, Velocity.Y, 0.f).GetSafeNormal();
		Velocity.X = FinalDir.X * HardCap;
		Velocity.Y = FinalDir.Y * HardCap;
	}

	// Track which synced action is in flight
	ActiveSyncedAction = EPcSyncedAction::Jump;
}

// CHANGE 4: Auto-jump branch removed — coyote window path unchanged
void UPcQPlayerMovementComponent::TriggerBeatJump()
{
	if (BhopState == EBhopState::WallSwim) {
		bWallBeatPending = true;
		return;
	}

	if (IsFalling())
	{
		if (Velocity.Z <= 0.f)
		{
			bJumpQueuedForBeat = true;
			BeatQueueTimer     = BeatCoyoteWindow;
		}
	}
}

bool UPcQPlayerMovementComponent::CanBufferLanding() const
{
	if (!CharacterOwner) return false;
	if (Velocity.Z >= 0.f) return false; 

	float CapsuleHalfHeight = 90.f;
	if (UCapsuleComponent* Cap = CharacterOwner->GetCapsuleComponent()) {
		CapsuleHalfHeight = Cap->GetUnscaledCapsuleHalfHeight();
	}

	const float FallDist = FMath::Abs(Velocity.Z) * JumpInputBufferWindow;
	const float CheckDist = CapsuleHalfHeight + FallDist + 50.f; 

	FVector Start = CharacterOwner->GetActorLocation();
	FHitResult Hit;
	FCollisionQueryParams P; P.AddIgnoredActor(CharacterOwner);
	return GetWorld()->LineTraceSingleByChannel(Hit, Start, Start - FVector(0.f, 0.f, CheckDist), ECC_WorldStatic, P);
}

void UPcQPlayerMovementComponent::OnJumpPressed()
{
	if (BhopState == EBhopState::WallSwim) {
		EjectFromWall(false, true);
		return;
	}

	const bool bOnBeat = IsOnBeat();
	if (bOnBeat) TriggerOnBeatFlash(); // universal reset at the top, gated per-beat

	if (BhopState == EBhopState::GroundPounding)
	{
		if (CanBufferLanding()) {
			bJumpInputBuffered   = true;
			JumpInputBufferTimer = JumpInputBufferWindow;
			return;
		}

		if (DJumpReady() || bOnBeat) {
			BhopState  = EBhopState::Active;
			Velocity.Z = FMath::Max(Velocity.Z, -GPCancelMinUpVelocity);
			bDoubleJumpUsed    = true;
			DoubleJumpCooldown = GetBeatSnappedDuration(DoubleJumpBaseCooldownSec);
			ExecutePlayerJump(false);
			PushCombo(TEXT("GP CANCEL"), FLinearColor(0.7f, 0.7f, 0.7f));
			if (bOnBeat) {
				bDoubleJumpUsed    = false;
				DoubleJumpCooldown = 0.f;
				TriggerOnBeatFlash();
			}
		} else {
			bJumpInputBuffered   = true;
			JumpInputBufferTimer = JumpInputBufferWindow;
		}
		return;
	}

	if (IsMovingOnGround())
	{
		// ── Super Jump: GP Pulse active or GP just landed ─────────────────────
		if (bGPPulseActive || (bGPLandedRecently && GPComboTimer > 0.f))
		{
			if (BhopState == EBhopState::PowerBoost) ExitBoost();
			ActivateSuperJump(bOnBeat);
			return;
		}

		if (BhopState == EBhopState::PowerBoost) {
			ExitBoost();
			ExecutePlayerJump(true);
			OnBhopLanded.Broadcast(GetHorizontalSpeed());
			return;
		}

		if (bOnBeat) bBonusHopRequested = true;

		ExecutePlayerJump(false);
		OnBhopLanded.Broadcast(GetHorizontalSpeed());
		return;
	}

	if (IsFalling())
	{
		if (CanBufferLanding()) {
			bJumpInputBuffered   = true;
			JumpInputBufferTimer = JumpInputBufferWindow;
			return;
		}

		// On-beat bypass only allowed if the free charge hasn't been used yet this beat.
		// This prevents infinite DJ spam within a single beat window.
		const bool bBeatBypass = bOnBeat && !bFreeChargeUsedThisBeat;
		if (DJumpReady() || bBeatBypass) {
			const bool bFreeJump = bBeatBypass;
			bDoubleJumpUsed    = true;
			DoubleJumpCooldown = GetBeatSnappedDuration(DoubleJumpBaseCooldownSec);
			ActiveSyncedAction = EPcSyncedAction::DoubleJump;
			ExecutePlayerJump(false);
			PushCombo(TEXT("DOUBLE JUMP"), FLinearColor(0.27f, 0.67f, 1.f));
			if (bFreeJump) {
				bFreeChargeUsedThisBeat = true; // consume the free charge
				bDoubleJumpUsed    = false;
				DoubleJumpCooldown = 0.f;
				PushCombo(TEXT("DJ RESET"), FLinearColor(0.27f, 0.67f, 1.f));
			}
		} else {
			bJumpInputBuffered   = true;
			JumpInputBufferTimer = JumpInputBufferWindow;
		}
	}
}

void UPcQPlayerMovementComponent::OnJumpReleased() {}

// CHANGE 5: Ground path replaced with ActivateGPPulse. Air path unchanged.
void UPcQPlayerMovementComponent::OnGroundPoundPressed()
{
	if (BhopState == EBhopState::WallSwim) return;

	const bool bOnBeat = IsOnBeat();

	if (IsMovingOnGround())
	{
		ActivateGPPulse(bOnBeat);
		return;
	}
	else if (IsFalling() && BhopState != EBhopState::GroundPounding)
	{
		if (bOnBeat) TriggerOnBeatFlash();

		if (CanBufferLanding()) {
			bGPInputBuffered   = true;
			GPInputBufferTimer = JumpInputBufferWindow; 
			return;
		}

		ExitCurveJump();
		BhopState     = EBhopState::GroundPounding;
		GPCancelTimer = 0.f;
		GPInitiatedZ  = CharacterOwner ? CharacterOwner->GetActorLocation().Z : 0.f;
		Velocity.Z = GroundPoundSlamSpeed;
	}
}

void UPcQPlayerMovementComponent::OnGroundPoundReleased() {}

void UPcQPlayerMovementComponent::OnSnapPressed()
{
	if (!IsOnBeat()) return;
	SnapPulseTimer = 0.15f;
	TriggerOnBeatFlash(); // universal reset, gated per-beat
	OnSnapPressed_Internal();
}

float UPcQPlayerMovementComponent::GetFrenzySpeedMult() const
{
	if (bSTierActive) return FrenzyMult_S;
	// Outside S: gauge drives D→A. Cannot reach S without Enhanced section.
	if (FrenzyGauge >= FrenzyThresh_A) return FrenzyMult_A;
	if (FrenzyGauge >= FrenzyThresh_B) return FrenzyMult_B;
	if (FrenzyGauge >= FrenzyThresh_C) return FrenzyMult_C;
	return FrenzyMult_D;
}

// CHANGE 6: New ActivateGPPulse — velocity burst in WASD dir, opens pulse window
void UPcQPlayerMovementComponent::ActivateGPPulse(bool bOnBeat)
{
	SuperJumpSourceHeight = 0.f; // ground GP has no fall height — min cap applies for super jump
	FVector Dir2D = Acceleration.GetSafeNormal2D();
	if (Dir2D.IsZero()) Dir2D = FVector(Velocity.X, Velocity.Y, 0.f).GetSafeNormal();
	if (Dir2D.IsZero() && CharacterOwner) Dir2D = CharacterOwner->GetActorForwardVector().GetSafeNormal2D();
	GPPulseDirection = Dir2D;

	float BurstSpeed = MaxWalkSpeed + GetScaledSpeed(GPPulseSpeedBoost);
	if (bOnBeat) BurstSpeed += GetScaledSpeed(GPPulseSpeedBoost * GPPulseOnBeatBonus);

	if (!Dir2D.IsZero())
	{
		Velocity.X = Dir2D.X * BurstSpeed;
		Velocity.Y = Dir2D.Y * BurstSpeed;
	}
	Velocity.Z = 0.f;

	// Duration = N beats + one beat window of tolerance so the pulse never expires
	// right as the player is attempting to chain on the beat edge.
	float Interval = 0.8f; // 75 BPM fallback
	if (UPcMusicAnalysisSubsystem* Sub = GetWorld()->GetSubsystem<UPcMusicAnalysisSubsystem>())
		if (Sub->IsReadyForPlayback()) Interval = Sub->GetGameplayBeatIntervalMS() / 1000.f;
	const float WindowTolerance = Interval * OnBeatWindowFraction;
	GPPulseMaxTimer = GPPulseDurationBeats * Interval + WindowTolerance;
	GPPulseTimer    = GPPulseMaxTimer;
	bGPPulseActive  = true;

	if (bOnBeat) TriggerOnBeatFlash();
	PushCombo(bOnBeat ? TEXT("GP PULSE ★") : TEXT("GP PULSE"), FLinearColor(1.f, 0.55f, 0.15f));
}

// =============================================================================
//  SUPER JUMP
//  Triggered by: GP Pulse + jump (ground), or air GP landing + jump.
//  On beat: arc synced to land exactly 1 beat later.
//  Height = clamp(fallHeight * Mult, Min, Max) — minimum cap ensures even
//  a ground-level trigger launches you meaningfully into the air.
// =============================================================================

void UPcQPlayerMovementComponent::ActivateSuperJump(bool bOnBeat)
{
	const float ClampedHeight = FMath::Clamp(
		SuperJumpSourceHeight * SuperJumpHeightMult,
		SuperJumpMinHeightCM,
		SuperJumpMaxHeightCM);

	if (CharacterOwner) CharacterOwner->JumpCurrentCount = 0;

	// Use natural gravity so the arc feels organic.
	// Forced arc timing (ApplyArcWithAirTime) was computing enormous GravityScale
	// to hit a specific height in one beat → felt like a fast plunge on descent.
	// On-beat bonus: extra height multiplier, not forced timing.
	GravityScale = 1.f;
	const float G       = FMath::Abs(GetWorld()->GetDefaultGravityZ());
	const float Height  = bOnBeat ? ClampedHeight : ClampedHeight * 0.75f;
	Velocity.Z = FMath::Sqrt(2.f * G * Height);
	SetMovementMode(MOVE_Falling);

	ActiveSyncedAction = EPcSyncedAction::SuperJump;

	if (bOnBeat)
	{
		TriggerOnBeatFlash();
		PushCombo(TEXT("SUPER JUMP ★"), FLinearColor(1.f, 0.9f, 0.2f));
	}
	else
	{
		PushCombo(TEXT("SUPER JUMP"), FLinearColor(1.f, 0.7f, 0.1f));
	}

	bGPPulseActive        = false;
	GPPulseTimer          = 0.f;
	bGPLandedRecently     = false;
	GPComboTimer          = 0.f;
	SuperJumpSourceHeight = 0.f;
}

// =============================================================================
//  FRENZY DASH BOOST  (S tier — fires on every gameplay beat while grounded)
// =============================================================================

void UPcQPlayerMovementComponent::TriggerFrenzyDashBoost()
{
	if (!IsMovingOnGround() || !bSTierActive) return;

	// Direction: WASD input → current velocity dir → camera forward
	FVector BoostDir = Acceleration.GetSafeNormal2D();
	if (BoostDir.IsZero()) BoostDir = FVector(Velocity.X, Velocity.Y, 0.f).GetSafeNormal();
	if (BoostDir.IsZero() && CharacterOwner)
		BoostDir = CharacterOwner->GetActorForwardVector().GetSafeNormal2D();
	if (BoostDir.IsZero()) return;

	const float BoostSpd = MaxWalkSpeed + GetScaledSpeed(FrenzyDashBoostSpeed);
	Velocity.X = BoostDir.X * BoostSpd;
	Velocity.Y = BoostDir.Y * BoostSpd;
	Velocity.Z = 0.f;

	// Decay timer: half a beat so the excess is gone before the next beat lands.
	// FrenzyDashDecayRate handles the actual speed bleed-off in Tick.
	float Interval = 0.8f;
	if (UPcMusicAnalysisSubsystem* Sub = GetWorld()->GetSubsystem<UPcMusicAnalysisSubsystem>())
		if (Sub->IsReadyForPlayback()) Interval = Sub->GetGameplayBeatIntervalMS() / 1000.f;
	bFrenzyDashActive = true;
	FrenzyDashTimer   = Interval * 0.5f;

	TriggerOnBeatFlash();
	PushCombo(TEXT("DASH BOOST ★"), FLinearColor(1.f, 0.15f, 0.2f));
}

void UPcQPlayerMovementComponent::ApplyJumpVelocity()
{
	if (CharacterOwner) CharacterOwner->JumpCurrentCount = 0;
	if (UPcMusicAnalysisSubsystem* Sub = GetWorld()->GetSubsystem<UPcMusicAnalysisSubsystem>())
	{
		if (Sub->IsReadyForPlayback()) {
			const FPcMovementPreset& P = Sub->GetCurrentPulsePreset();
			const float Interval       = Sub->GetGameplayBeatIntervalMS() / 1000.f;
			float AirTime              = Sub->GetTimeUntilNextGameplayBeat();
			if (AirTime < Interval * 0.25f) AirTime += Interval;
			ApplyArcWithAirTime(P.PeakHeightCM, AirTime);
			return;
		}
	}
	Velocity.Z = FMath::Max(Velocity.Z, JumpZVelocity);
	SetMovementMode(MOVE_Falling);
}

// RESTORED: This is the mathematically synced jump arc!
void UPcQPlayerMovementComponent::ApplyFixedBeatJump()
{
	if (CharacterOwner) CharacterOwner->JumpCurrentCount = 0;
	if (UPcMusicAnalysisSubsystem* Sub = GetWorld()->GetSubsystem<UPcMusicAnalysisSubsystem>())
	{
		if (Sub->IsReadyForPlayback()) {
			const FPcMovementPreset& P = Sub->GetCurrentPulsePreset();
			float Interval = Sub->GetGameplayBeatIntervalMS() / 1000.f;
			ApplyArcWithAirTime(P.PeakHeightCM, Interval); 
			return;
		}
	}
	Velocity.Z = FMath::Max(Velocity.Z, JumpZVelocity);
	SetMovementMode(MOVE_Falling);
}

void UPcQPlayerMovementComponent::ApplyArcWithAirTime(float PeakHeightCM, float AirTimeSec)
{
	if (CharacterOwner) CharacterOwner->JumpCurrentCount = 0;
	if (AirTimeSec <= 0.001f) AirTimeSec = 0.5f; // SAFEGUARD
	
	if (JumpCurve) {
		JumpCurveTimer = 0.f; JumpCurveTotalTime = AirTimeSec; JumpCurvePeakHeight = PeakHeightCM;
		JumpCurveLaunchZ = CharacterOwner ? CharacterOwner->GetActorLocation().Z : 0.f;
		GravityScale = 0.f; bUsingJumpCurve = true;
		const float H0 = JumpCurve->GetFloatValue(0.f)    * PeakHeightCM;
		const float H1 = JumpCurve->GetFloatValue(0.001f) * PeakHeightCM;
		Velocity.Z = (H1 - H0) / (0.001f * AirTimeSec);
		SetMovementMode(MOVE_Falling); return;
	}
	const float T  = AirTimeSec * 0.5f;
	const float G  = (2.f * PeakHeightCM) / (T * T);
	GravityScale   = G / FMath::Abs(GetWorld()->GetDefaultGravityZ());
	Velocity.Z     = G * T;
	SetMovementMode(MOVE_Falling);
}

void UPcQPlayerMovementComponent::ExitCurveJump()
{
	if (!bUsingJumpCurve) return;
	bUsingJumpCurve = false; GravityScale = 1.f;
}

void UPcQPlayerMovementComponent::EnterWallSpring(const FHitResult& Hit)
{
	if (BhopState == EBhopState::WallSwim) return;
	if (WallEjectImmunityTimer > 0.f) return;
	ExitCurveJump();

	BhopState            = EBhopState::WallSwim;
	WallEntryNormal      = Hit.ImpactNormal;
	WallEntrySpeed       = GetHorizontalSpeed();
	WallCompressionTimer = 0.f;
	bWallBeatPending     = false;

	if (UCapsuleComponent* Cap = CharacterOwner ? CharacterOwner->GetCapsuleComponent() : nullptr) {
		WallPrevCollisionProfile = Cap->GetCollisionProfileName();
		Cap->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Ignore);
	}
	SetMovementMode(MOVE_Flying);
	OnWallSwimChanged.Broadcast(1.f);
	PushCombo(TEXT("WALL SPRING"), FLinearColor(0.55f, 1.f, 0.35f));
}

void UPcQPlayerMovementComponent::EjectFromWall(bool bBeatBoost, bool bJumpEject)
{
	if (UCapsuleComponent* Cap = CharacterOwner ? CharacterOwner->GetCapsuleComponent() : nullptr)
		Cap->SetCollisionProfileName(WallPrevCollisionProfile.IsNone() ? FName(TEXT("Pawn")) : WallPrevCollisionProfile);

	BhopState = EBhopState::Active;

	FVector EjectDir = Acceleration.GetSafeNormal2D();
	if (EjectDir.IsNearlyZero()) {
		const FVector Vel2D  = FVector(Velocity.X, Velocity.Y, 0.f);
		const FVector Refl   = Vel2D - 2.f * FVector::DotProduct(Vel2D, -WallEntryNormal) * (-WallEntryNormal);
		EjectDir             = Refl.GetSafeNormal2D();
		if (EjectDir.IsZero()) EjectDir = WallEntryNormal;
	}

	float EjectSpd = FMath::Max(WallEntrySpeed, GetScaledSpeed(Wall_EjectSpeed));
	if (bBeatBoost) EjectSpd += GetScaledSpeed(Wall_BeatEjectBoost);

	if (FVector::DotProduct(EjectDir, WallEntryNormal) < 0.1f)
		EjectDir = (EjectDir + WallEntryNormal * 2.f).GetSafeNormal2D();
	if (EjectDir.IsZero()) EjectDir = WallEntryNormal;

	Velocity.X = EjectDir.X * EjectSpd;
	Velocity.Y = EjectDir.Y * EjectSpd;
	Velocity.Z = bJumpEject ? Wall_JumpEjectUpKick : Wall_AutoEjectUpKick;

	GravityScale = 1.f;
	bDoubleJumpUsed    = false; 
	DoubleJumpCooldown = 0.f;
	WallEjectImmunityTimer = Wall_EjectImmunitySec;
	SetMovementMode(MOVE_Falling);
	OnWallSwimChanged.Broadcast(0.f);

	if (bBeatBoost) {
		TriggerOnBeatFlash();
		PushCombo(TEXT("BEAT EJECT +SPEED"), FLinearColor(0.55f, 1.f, 0.35f));
	} else if (bJumpEject) PushCombo(TEXT("WALL JUMP"), FLinearColor(0.55f, 1.f, 0.35f));
	else PushCombo(TEXT("WALL EJECT"), FLinearColor(0.55f, 1.f, 0.35f));
}

bool UPcQPlayerMovementComponent::IsAboveGround() const
{
	if (!CharacterOwner) return true;
	FVector Start = CharacterOwner->GetActorLocation();
	FHitResult Hit; FCollisionQueryParams P; P.AddIgnoredActor(CharacterOwner);
	return !GetWorld()->LineTraceSingleByChannel(Hit, Start, Start - FVector(0.f, 0.f, Wall_FloorCheckDist), ECC_WorldStatic, P);
}

void UPcQPlayerMovementComponent::HandleImpact(const FHitResult& Hit, float TimeSlice, const FVector& MoveDelta)
{
	if (MovementMode == MOVE_Falling && BhopState != EBhopState::WallSwim && BhopState != EBhopState::GroundPounding) {
		if (FMath::Abs(Hit.ImpactNormal.Z) < 0.4f && GetHorizontalSpeed() >= Wall_EnterMinSpeed) {
			EnterWallSpring(Hit); return;
		}
	}
	Super::HandleImpact(Hit, TimeSlice, MoveDelta);
}

// CHANGE 7: Clear GP Pulse and ActiveSyncedAction on any landing
void UPcQPlayerMovementComponent::ProcessLanded(const FHitResult& Hit, float remainingTime, int32 Iterations)
{
	bGPPulseActive     = false;
	GPPulseTimer       = 0.f;
	ActiveSyncedAction = EPcSyncedAction::None;

	if (BhopState == EBhopState::WallSwim) {
		if (UCapsuleComponent* Cap = CharacterOwner ? CharacterOwner->GetCapsuleComponent() : nullptr)
			Cap->SetCollisionProfileName(WallPrevCollisionProfile.IsNone() ? FName(TEXT("Pawn")) : WallPrevCollisionProfile);
		BhopState = EBhopState::Active; OnWallSwimChanged.Broadcast(0.f);
		Super::ProcessLanded(Hit, remainingTime, Iterations); return;
	}

	if (BhopState == EBhopState::GroundPounding)
	{
		ExitCurveJump();

		// Save fall height for super jump calculation
		const float LandingZ = CharacterOwner ? CharacterOwner->GetActorLocation().Z : 0.f;
		SuperJumpSourceHeight = FMath::Max(GPInitiatedZ - LandingZ, 0.f);
		const FVector WishDir2D = Acceleration.GetSafeNormal2D();
		const bool bHasInput    = !WishDir2D.IsZero();

		BhopState          = EBhopState::PowerBoost;
		BoostTimer         = GetBeatSnappedDuration(BoostBaseDurationSec);
		BoostCooldown      = 0.f;

		const float BaseSpd  = MaxWalkSpeed * BoostSpeedMultiplier;
		// GPPulseSpeedBoost reused as the on-beat landing bonus (same concept, same tunable)
		const float EntrySpd = IsOnBeat() ? BaseSpd + GetScaledSpeed(GPPulseSpeedBoost) : BaseSpd;
		if (bHasInput) {
			Velocity.X = WishDir2D.X * EntrySpd;
			Velocity.Y = WishDir2D.Y * EntrySpd;
		}
		// No input: keep existing horizontal velocity — GP was purely vertical
		Velocity.Z = 0.f;

		bGPLandedRecently    = true;
		bBoostActiveOnGPLand = true;
		GPComboTimer         = GPComboWindowSec;

		if (IsOnBeat()) {
			TriggerOnBeatFlash();
			PushCombo(TEXT("GP SLAM ★"), FLinearColor(1.f, 0.55f, 0.15f));
		} else {
			PushCombo(TEXT("GP SLAM"), FLinearColor(1.f, 0.55f, 0.15f));
		}
		
		Super::ProcessLanded(Hit, remainingTime, Iterations);

		if (bJumpInputBuffered && JumpInputBufferTimer > 0.f)
		{
			bJumpInputBuffered = false; JumpInputBufferTimer = 0.f;
			ExitBoost();
			ExecutePlayerJump(true); 
			OnBhopLanded.Broadcast(GetHorizontalSpeed());
		}
		return;
	}

	if (BhopState == EBhopState::PowerBoost) ExitBoost();
	ExitCurveJump();
	GravityScale = 1.f; // reset after any arc — ExitCurveJump only handles the JumpCurve path
	
	bDoubleJumpUsed    = false;
	DoubleJumpCooldown = 0.f;

	Super::ProcessLanded(Hit, remainingTime, Iterations);

	if (BhopState == EBhopState::Active)
	{
		if (bGPInputBuffered && GPInputBufferTimer > 0.f) {
			bGPInputBuffered = false; GPInputBufferTimer = 0.f;
			OnGroundPoundPressed();
			return;
		}

		if (bJumpInputBuffered && JumpInputBufferTimer > 0.f) {
			bJumpInputBuffered = false; JumpInputBufferTimer = 0.f;
			const bool bWasFromGP = bGPLandedRecently && GPComboTimer > 0.f;
			
			if (IsOnBeat()) {
				bBonusHopRequested = true;
				TriggerOnBeatFlash();
			} 
			ExecutePlayerJump(bWasFromGP);
			OnBhopLanded.Broadcast(GetHorizontalSpeed());
			return;
		}
	}

	if (BhopState == EBhopState::Active && bJumpQueuedForBeat) {
		bJumpQueuedForBeat = false;
		ApplyJumpVelocity();
		OnBhopLanded.Broadcast(GetHorizontalSpeed());
	}
}

void UPcQPlayerMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	if (BhopState == EBhopState::WallSwim) {
		WallCompressionTimer += DeltaTime;
		const float Decay = FMath::Pow(1.f - FMath::Clamp(Wall_CompressionDecay * DeltaTime, 0.f, 0.99f), 1.f);
		Velocity.X *= Decay; Velocity.Y *= Decay;
		if (Velocity.Z < 0.f) Velocity.Z *= Decay;

		if (!IsAboveGround() && Velocity.Z < 0.f) Velocity.Z = 0.f;

		if (bWallBeatPending) {
			bWallBeatPending = false;
			EjectFromWall(true, false);
		} else if (WallCompressionTimer >= Wall_CompressionSec) {
			EjectFromWall(false, false);
		}
	}

	// ── Speed: driven by player tier only, song has no effect ─────────────────
	// S tier logic: entry, lock timer, slow drain, exit
	{
		UPcMusicAnalysisSubsystem* Sub = GetWorld()->GetSubsystem<UPcMusicAnalysisSubsystem>();
		if (bSTierActive)
		{
			// S exits immediately if the Enhanced section ends
			if (!Sub || !Sub->IsEnhanced())
			{
				bSTierActive = false; bSTierLocked = false; STierLockTimer = 0.f;
			}
			else if (bSTierLocked)
			{
				// Gauge doesn't drain while locked — beat actions reset the timer
				STierLockTimer -= DeltaTime;
				if (STierLockTimer <= 0.f) bSTierLocked = false;
			}
			else
			{
				// Lock expired: slow drain until exit threshold
				FrenzyGauge = FMath::Max(0.f, FrenzyGauge - FrenzyDrainPerSec * STierDrainMult * DeltaTime);
				if (FrenzyGauge < STierExitThreshold) { bSTierActive = false; }
			}
		}
		else
		{
			// Normal drain
			FrenzyGauge = FMath::Max(0.f, FrenzyGauge - FrenzyDrainPerSec * DeltaTime);
		}
	}
	MaxWalkSpeed = BaseMaxSpeed * GetFrenzySpeedMult();

	if (BhopState == EBhopState::PowerBoost && IsMovingOnGround()) {
		const float BoostSpd  = FMath::Max(MaxWalkSpeed, 100.f) * BoostSpeedMultiplier;
		const FVector WishDir = Acceleration.GetSafeNormal2D();

		if (!WishDir.IsZero()) {
			const FVector Vel2D  = FVector(Velocity.X, Velocity.Y, 0.f);
			const FVector Target = WishDir * BoostSpd;
			const FVector New2D  = FMath::VInterpTo(Vel2D, Target, DeltaTime, CustomGroundAcceleration);
			Velocity.X = New2D.X; Velocity.Y = New2D.Y;
		} else {
			const float FR = FMath::Pow(1.f - FMath::Clamp(2.f * DeltaTime, 0.f, 0.99f), 1.f);
			Velocity.X *= FR; Velocity.Y *= FR;
		}
		BoostTimer -= DeltaTime;
		if (BoostTimer <= 0.f) ExitBoost();
	}

	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	PreviousFrameSpeed = GetHorizontalSpeed();

	if (BhopState != EBhopState::PowerBoost) {
		const float CurH    = GetHorizontalSpeed();
		const float SafeMaxWalkSpeed = FMath::Max(MaxWalkSpeed, 1.f); // SAFEGUARD
		const float HardCap = SafeMaxWalkSpeed * HardSpeedCapMult;

		if (CurH > SafeMaxWalkSpeed) {
			float NewH;
			if (bFrenzyDashActive)
			{
				// Dedicated fast decay for dash boost — bleeds off by next beat
				NewH = FMath::Clamp(CurH - FrenzyDashDecayRate * DeltaTime, SafeMaxWalkSpeed, HardCap);
			}
			else
			{
				// Proportional decay for regular overspeed momentum
				const float Excess    = CurH - SafeMaxWalkSpeed;
				const float DecayThis = OverspeedDecayRate * DeltaTime * (Excess / SafeMaxWalkSpeed);
				NewH = FMath::Clamp(CurH - DecayThis, SafeMaxWalkSpeed, HardCap);
			}
			const FVector Dir2D   = FVector(Velocity.X, Velocity.Y, 0.f).GetSafeNormal();
			if (!Dir2D.IsZero()) {
				Velocity.X = Dir2D.X * NewH;
				Velocity.Y = Dir2D.Y * NewH;
			}
		}
	}

	// Frenzy dash timer
	if (bFrenzyDashActive)
	{
		FrenzyDashTimer -= DeltaTime;
		if (FrenzyDashTimer <= 0.f) bFrenzyDashActive = false;
	}

	if (bUsingJumpCurve && JumpCurve && JumpCurveTotalTime > 0.f) {
		JumpCurveTimer    = FMath::Min(JumpCurveTimer + DeltaTime, JumpCurveTotalTime);
		const float T     = JumpCurveTimer / JumpCurveTotalTime;
		const float TNext = FMath::Min((JumpCurveTimer + DeltaTime) / JumpCurveTotalTime, 1.f);
		Velocity.Z = (JumpCurve->GetFloatValue(TNext) - JumpCurve->GetFloatValue(T)) * JumpCurvePeakHeight / DeltaTime;
		if (JumpCurveTimer >= JumpCurveTotalTime) {
			const float BS = 0.005f;
			ExitCurveJump();
			Velocity.Z = FMath::Min(
				(JumpCurve->GetFloatValue(1.f) - JumpCurve->GetFloatValue(1.f-BS)) * JumpCurvePeakHeight / (BS * JumpCurveTotalTime),
				-80.f);
		}
	}

	// CHANGE 8: GP Pulse window timer
	if (bGPPulseActive && GPPulseTimer > 0.f)
	{
		GPPulseTimer -= DeltaTime;
		if (GPPulseTimer <= 0.f) { bGPPulseActive = false; GPPulseTimer = 0.f; }
	}

	if (BoostCooldown           > 0.f) BoostCooldown           = FMath::Max(0.f, BoostCooldown           - DeltaTime);
	if (WallEjectImmunityTimer  > 0.f) WallEjectImmunityTimer  = FMath::Max(0.f, WallEjectImmunityTimer  - DeltaTime);
	if (DoubleJumpCooldown      > 0.f) DoubleJumpCooldown      = FMath::Max(0.f, DoubleJumpCooldown      - DeltaTime);
	if (OnBeatFlashTimer        > 0.f) OnBeatFlashTimer        = FMath::Max(0.f, OnBeatFlashTimer        - DeltaTime);
	if (SnapPulseTimer          > 0.f) SnapPulseTimer          = FMath::Max(0.f, SnapPulseTimer          - DeltaTime);
	// Player wave decays toward 0 — same decay rate as we want for song wave
	PlayerPulse = FMath::Max(0.f, PlayerPulse - DeltaTime * 3.5f);
	
	if (GPComboTimer > 0.f) { 
		GPComboTimer -= DeltaTime; 
		if (GPComboTimer <= 0.f) { bGPLandedRecently = false; bBoostActiveOnGPLand = false; } 
	}
	
	if (BhopState == EBhopState::GroundPounding) GPCancelTimer += DeltaTime;
	if (bJumpInputBuffered) { JumpInputBufferTimer -= DeltaTime; if (JumpInputBufferTimer <= 0.f) bJumpInputBuffered = false; }
	if (bGPInputBuffered)   { GPInputBufferTimer   -= DeltaTime; if (GPInputBufferTimer   <= 0.f) bGPInputBuffered   = false; }
	if (bJumpQueuedForBeat) { BeatQueueTimer       -= DeltaTime; if (BeatQueueTimer       <= 0.f) bJumpQueuedForBeat = false; }
}

void UPcQPlayerMovementComponent::PhysWalking(float deltaTime, int32 Iterations)
{
	if (deltaTime < MIN_TICK_TIME) return;
	if (BhopState == EBhopState::PowerBoost) {
		FVector Saved = Acceleration; Acceleration = FVector::ZeroVector;
		Super::PhysWalking(deltaTime, Iterations);
		Acceleration = Saved;
		return;
	}

	float TargetSpeed = MaxWalkSpeed;
	const FVector WishDir   = Acceleration.GetSafeNormal2D();
	FVector       Vel2D(Velocity.X, Velocity.Y, 0.f);
	const float   Spd       = Vel2D.Size();
	const float   EffTarget = (Spd > TargetSpeed && !WishDir.IsZero()) ? Spd : TargetSpeed;
	const FVector NewVel2D  = FMath::VInterpTo(Vel2D, WishDir * EffTarget, deltaTime,
	                          WishDir.IsZero() ? CustomGroundFriction : CustomGroundAcceleration);
	Velocity.X = NewVel2D.X; Velocity.Y = NewVel2D.Y;

	FVector Saved = Acceleration; Acceleration = FVector::ZeroVector;
	Super::PhysWalking(deltaTime, Iterations);
	Acceleration = Saved;
}

void UPcQPlayerMovementComponent::PhysFalling(float deltaTime, int32 Iterations)
{
	if (deltaTime < MIN_TICK_TIME) return;
	if (BhopState == EBhopState::GroundPounding) {
		Acceleration = FVector::ZeroVector;
		Super::PhysFalling(deltaTime, Iterations); return;
	}

	float TargetAirSpeed = MaxWalkSpeed;
	if (UPcMusicAnalysisSubsystem* Sub = GetWorld()->GetSubsystem<UPcMusicAnalysisSubsystem>()) {
		if (Sub->IsReadyForPlayback()) {
			float BaseAirSpd = Sub->GetCurrentPulsePreset().MaxAirSpeed;
			float ScaledAirSpd = GetScaledSpeed(BaseAirSpd);
			if (ScaledAirSpd > 10.f) TargetAirSpeed = ScaledAirSpd;
		}
	}

	const FVector WishDir  = Acceleration.GetSafeNormal2D();
	const FVector Vel2D(Velocity.X, Velocity.Y, 0.f);
	const float   CurAirH  = Vel2D.Size();

	const float EffAirTarget = WishDir.IsZero() ? 0.f : FMath::Max(CurAirH, TargetAirSpeed);
	const FVector NewVel2D = FMath::VInterpTo(Vel2D, WishDir * EffAirTarget, deltaTime,
	                         WishDir.IsZero() ? CustomAirFriction : CustomAirAcceleration);
	Velocity.X = NewVel2D.X; Velocity.Y = NewVel2D.Y;

	FVector Saved = Acceleration; Acceleration = FVector::ZeroVector;
	Super::PhysFalling(deltaTime, Iterations);
	Acceleration = Saved;
}