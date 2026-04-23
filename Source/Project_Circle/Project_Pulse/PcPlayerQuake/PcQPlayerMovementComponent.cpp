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
	return FMath::Min(FMath::Abs(Next - Now), FMath::Abs(Now - (Next - Interval))) <= OnBeatWindowMS;
}

float UPcQPlayerMovementComponent::GetBeatSnappedDuration(float BaseSec) const
{
	UPcMusicAnalysisSubsystem* Sub = GetWorld()->GetSubsystem<UPcMusicAnalysisSubsystem>();
	if (!Sub || !Sub->IsReadyForPlayback()) return BaseSec;
	const float IntervalSec    = Sub->GetGameplayBeatIntervalMS() / 1000.f;
	const float TimeToNext     = Sub->GetTimeUntilNextGameplayBeat();
	const float AlreadyElapsed = IntervalSec - TimeToNext;
	const int32 WholeBeats     = FMath::CeilToInt(BaseSec / IntervalSec);
	return FMath::Max((float)WholeBeats * IntervalSec - AlreadyElapsed, IntervalSec);
}

float UPcQPlayerMovementComponent::GetBoostCooldownAlpha() const { return BoostCooldown <= 0.f ? 0.f : FMath::Clamp(BoostCooldown / FMath::Max(BoostBaseCooldownSec, 0.01f), 0.f, 1.f); }
float UPcQPlayerMovementComponent::GetBoostActiveAlpha() const { if (BhopState != EBhopState::PowerBoost || BoostTimer <= 0.f) return 0.f; return FMath::Clamp(BoostTimer / FMath::Max(BoostBaseDurationSec, 0.01f), 0.f, 1.f); }
float UPcQPlayerMovementComponent::GetDoubleJumpCooldownAlpha() const { if (DoubleJumpCooldown <= 0.f) return bDoubleJumpUsed ? 1.f : 0.f; return FMath::Clamp(DoubleJumpCooldown / FMath::Max(DoubleJumpBaseCooldownSec, 0.01f), 0.f, 1.f); }

void UPcQPlayerMovementComponent::TriggerOnBeatFlash()
{
	// UNIVERSAL RESET: Any on-beat action completely restores everything!
	OnBeatFlashTimer = OnBeatFlashDuration;
	BoostCooldown      = 0.f;
	DoubleJumpCooldown = 0.f;
	bDoubleJumpUsed    = false;
	
	// If currently sliding, refill the slide timer AND give a speed pump!
	if (BhopState == EBhopState::PowerBoost) {
		BoostTimer = GetBeatSnappedDuration(BoostBaseDurationSec);
		
		const float BoostSpd = MaxWalkSpeed * BoostSpeedMultiplier;
		const FVector Dir2D  = FVector(Velocity.X, Velocity.Y, 0.f).GetSafeNormal();
		if (!Dir2D.IsZero() && GetHorizontalSpeed() < BoostSpd)
		{
			Velocity.X = Dir2D.X * BoostSpd;
			Velocity.Y = Dir2D.Y * BoostSpd;
		}
	}

	OnActiveBeatAction.Broadcast(); // This tells the character to reset Pistol CD
}

void UPcQPlayerMovementComponent::PushCombo(const FString& Label, FLinearColor Color)
{
	OnComboEvent.Broadcast(Label, Color);
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

void UPcQPlayerMovementComponent::ActivateSlide()
{
	if (BhopState == EBhopState::PowerBoost) return;

	const bool bOnBeat = IsOnBeat();

	BhopState     = EBhopState::PowerBoost;
	BoostTimer    = BoostBaseDurationSec;
	BoostCooldown = 0.f;
	bAutoJumpActive = false;

	// Snap velocity to boost speed on entry — hold→slide should feel immediate
	FVector Dir = Acceleration.GetSafeNormal2D();
	if (Dir.IsZero()) Dir = FVector(Velocity.X, Velocity.Y, 0.f).GetSafeNormal();
	if (!Dir.IsZero())
	{
		const float EntrySpd = FMath::Max(GetHorizontalSpeed(), MaxWalkSpeed * BoostSpeedMultiplier)
		                      + (bOnBeat ? SlideEntryBoost : 0.f);
		Velocity.X = Dir.X * EntrySpd;
		Velocity.Y = Dir.Y * EntrySpd;
	}

	if (bOnBeat) TriggerOnBeatFlash();
	PushCombo(bOnBeat ? TEXT("PERFECT SLIDE") : TEXT("SLIDE"), FLinearColor(1.f, 0.55f, 0.15f));
}

void UPcQPlayerMovementComponent::ExitBoost()
{
	BhopState     = EBhopState::Active;
	BoostTimer    = 0.f;
	BoostGraceTimer = BoostGraceWindowSec;
	BoostCooldown = GetBeatSnappedDuration(BoostBaseCooldownSec);
}

void UPcQPlayerMovementComponent::NotifyGunFired()
{
	// On-beat gun fire = universal reset: slide pump, DJ reset, boost CD reset, pistol CD reset.
	// This is the single source of truth — the character does NOT need to call TriggerOnBeatFlash separately.
	TriggerOnBeatFlash();
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
			const float LaunchSpd = FMath::Max(PreJumpH, MaxWalkSpeed * BoostSpeedMultiplier) + 600.f;
			Velocity.X = WD.X * LaunchSpd;
			Velocity.Y = WD.Y * LaunchSpd;
		}
		PushCombo(TEXT("BOOST JUMP"), FLinearColor(1.f, 0.55f, 0.15f));
	}
	else if (bGPLandedRecently && GPComboTimer > 0.f) {
		const FVector WishDir = Acceleration.GetSafeNormal2D().IsZero() ? Dir2D : Acceleration.GetSafeNormal2D();
		float Bonus = GPJumpHorizBoost;
		if (bBoostActiveOnGPLand) Bonus *= GPBoostJumpHorizMult;

		Velocity.X += WishDir.X * Bonus;
		Velocity.Y += WishDir.Y * Bonus;

		bGPLandedRecently    = false;
		bBoostActiveOnGPLand = false;
		GPComboTimer         = 0.f;

		TriggerOnBeatFlash();
		const bool bWasBoostChain = (Bonus > GPJumpHorizBoost);
		PushCombo(bWasBoostChain ? TEXT("GP BOOST JUMP") : TEXT("GP COMBO JUMP"), FLinearColor(1.f, 0.9f, 0.15f));
	}

	if (bBonusHopRequested) {
		const FVector WD = Acceleration.GetSafeNormal2D().IsZero() ? Dir2D : Acceleration.GetSafeNormal2D();
		Velocity.X += WD.X * BonusHopSpeedBoost;
		Velocity.Y += WD.Y * BonusHopSpeedBoost;
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
}

void UPcQPlayerMovementComponent::TriggerBeatJump()
{
	if (BhopState == EBhopState::WallSwim) {
		bWallBeatPending = true;
		return;
	}

	if (IsMovingOnGround()) {
		if (BhopState == EBhopState::Active)
		{
			if (bAutoJumpEnabled || bAutoJumpActive)
			{
				TriggerOnBeatFlash();
				ExecutePlayerJump(true);  
				OnBhopLanded.Broadcast(GetHorizontalSpeed());
			}
		}
		// Notice we removed the automatic slide pump from here!
		// To get a slide pump, the player must actively do something on beat.
	}
	else if (!IsFalling() || Velocity.Z <= 0.f) {
		bJumpQueuedForBeat = true;
		BeatQueueTimer     = BeatCoyoteWindow;
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
	if (bOnBeat) TriggerOnBeatFlash();

	if (BhopState == EBhopState::GroundPounding)
	{
		if (CanBufferLanding()) {
			bJumpInputBuffered   = true;
			JumpInputBufferTimer = JumpInputBufferWindow;
			return;
		}

		if (DJumpReady() || bOnBeat) {
			BhopState  = EBhopState::Active;
			Velocity.Z = FMath::Max(Velocity.Z, -600.f);
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
		// DASH PULSE + JUMP -> BOOST JUMP
		if (BhopState == EBhopState::PowerBoost || bSlideImpulseActive) {
			if (BhopState == EBhopState::PowerBoost) ExitBoost();
			bSlideImpulseActive = false; // consume it
			ExecutePlayerJump(true); 
			OnBhopLanded.Broadcast(GetHorizontalSpeed());
			return;
		}

		if (bOnBeat)
		{
			bBonusHopRequested = true;
			if (!bAutoJumpActive)
			{
				bAutoJumpActive = true;
				PushCombo(TEXT("AUTO JUMP ON"), FLinearColor(0.55f, 1.f, 0.35f));
			}
		} 
		else 
		{
			if (bAutoJumpActive) {
				bAutoJumpActive = false;
				PushCombo(TEXT("AUTO JUMP OFF"), FLinearColor(0.5f, 0.5f, 0.5f));
			}
		}

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

		if (DJumpReady() || bOnBeat) {
			const bool bFreeJump = bOnBeat;
			bDoubleJumpUsed    = true;
			DoubleJumpCooldown = GetBeatSnappedDuration(DoubleJumpBaseCooldownSec);
			ExecutePlayerJump(false);
			PushCombo(TEXT("DOUBLE JUMP"), FLinearColor(0.27f, 0.67f, 1.f));
			if (bFreeJump) {
				bDoubleJumpUsed    = false;
				DoubleJumpCooldown = 0.f;
				PushCombo(TEXT("DJ RESET"), FLinearColor(0.27f, 0.67f, 1.f));
			}
		} else {
			bJumpInputBuffered   = true;
			JumpInputBufferTimer = JumpInputBufferWindow;
			bAutoJumpActive      = false;
		}
	}
}

void UPcQPlayerMovementComponent::OnJumpReleased() {}

void UPcQPlayerMovementComponent::OnSlidePressed()
{
	bSlideImpulseActive = true;
	SlideImpulseTimer   = SlideImpulseDuration;

	if (bAutoJumpActive) {
		bAutoJumpActive = false;
		PushCombo(TEXT("AUTO JUMP OFF"), FLinearColor(0.5f, 0.5f, 0.5f));
	}

	const bool bOnBeat = IsOnBeat();
	if (bOnBeat) TriggerOnBeatFlash(); // Universal reset + active pump if sliding

	if (IsMovingOnGround() && BhopState != EBhopState::PowerBoost) {
		FVector DashDir = Acceleration.GetSafeNormal2D();
		if (DashDir.IsZero()) DashDir = FVector(Velocity.X, Velocity.Y, 0.f).GetSafeNormal();
		if (DashDir.IsZero() && CharacterOwner) DashDir = CharacterOwner->GetActorForwardVector().GetSafeNormal2D();

		if (!DashDir.IsZero()) {
			float TargetSpeed = FMath::Max(GetHorizontalSpeed(), DashImpulseSpeed);
			if (bOnBeat) TargetSpeed += SlideEntryBoost; // Reward on-beat dash
			Velocity.X = DashDir.X * TargetSpeed;
			Velocity.Y = DashDir.Y * TargetSpeed;
			PushCombo(bOnBeat ? TEXT("PERFECT DASH") : TEXT("DASH PULSE"), FLinearColor(1.f, 0.65f, 0.2f));
		}
	}
}

void UPcQPlayerMovementComponent::OnSlideReleased() {}

void UPcQPlayerMovementComponent::DoBriefDash() {} // Not used directly anymore, integrated into OnSlidePressed

void UPcQPlayerMovementComponent::OnGroundPoundPressed()
{
	if (BhopState == EBhopState::WallSwim) return;
	bAutoJumpActive = false; 

	const bool bOnBeat = IsOnBeat();
	if (bOnBeat) TriggerOnBeatFlash();

	if (IsFalling() && BhopState != EBhopState::GroundPounding)
	{
		if (CanBufferLanding()) {
			bGPInputBuffered   = true;
			GPInputBufferTimer = JumpInputBufferWindow; 
			return;
		}

		ExitCurveJump();
		BhopState     = EBhopState::GroundPounding;
		GPCancelTimer = 0.f;
		Velocity.Z = GroundPoundSlamSpeed;
	}
}

void UPcQPlayerMovementComponent::OnGroundPoundReleased() {}

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

void UPcQPlayerMovementComponent::ApplyFixedBeatJump()
{
	if (CharacterOwner) CharacterOwner->JumpCurrentCount = 0;
	if (UPcMusicAnalysisSubsystem* Sub = GetWorld()->GetSubsystem<UPcMusicAnalysisSubsystem>())
	{
		if (Sub->IsReadyForPlayback()) {
			const FPcMovementPreset& P = Sub->GetCurrentPulsePreset();
			ApplyArcWithAirTime(P.PeakHeightCM, Sub->GetGameplayBeatIntervalMS() / 1000.f);
			return;
		}
	}
	Velocity.Z = FMath::Max(Velocity.Z, JumpZVelocity);
	SetMovementMode(MOVE_Falling);
}

void UPcQPlayerMovementComponent::ApplyArcWithAirTime(float PeakHeightCM, float AirTimeSec)
{
	if (CharacterOwner) CharacterOwner->JumpCurrentCount = 0;
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
		Cap->SetCollisionProfileName(TEXT("NoCollision"));
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

	float EjectSpd = FMath::Max(WallEntrySpeed, Wall_EjectSpeed);
	if (bBeatBoost) EjectSpd += Wall_BeatEjectBoost;

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

void UPcQPlayerMovementComponent::ProcessLanded(const FHitResult& Hit, float remainingTime, int32 Iterations)
{
	if (BhopState == EBhopState::WallSwim) {
		if (UCapsuleComponent* Cap = CharacterOwner ? CharacterOwner->GetCapsuleComponent() : nullptr)
			Cap->SetCollisionProfileName(WallPrevCollisionProfile.IsNone() ? FName(TEXT("Pawn")) : WallPrevCollisionProfile);
		BhopState = EBhopState::Active; OnWallSwimChanged.Broadcast(0.f);
		Super::ProcessLanded(Hit, remainingTime, Iterations); return;
	}

	if (BhopState == EBhopState::GroundPounding)
	{
		ExitCurveJump();
		const FVector WishDir2D = Acceleration.GetSafeNormal2D();
		const bool bHasInput    = !WishDir2D.IsZero();

		BhopState     = EBhopState::PowerBoost;
		BoostTimer    = GetBeatSnappedDuration(BoostBaseDurationSec);
		BoostCooldown = 0.f;

		const float BaseSpd  = MaxWalkSpeed * BoostSpeedMultiplier;
		const float EntrySpd = IsOnBeat() ? BaseSpd + SlideEntryBoost : BaseSpd;
		if (bHasInput) {
			Velocity.X = WishDir2D.X * EntrySpd;
			Velocity.Y = WishDir2D.Y * EntrySpd;
		} else {
			Velocity.X = 0.f; Velocity.Y = 0.f;
		}
		Velocity.Z = 0.f;

		bGPLandedRecently    = true;
		bBoostActiveOnGPLand = true;
		GPComboTimer         = GPComboWindowSec;

		if (IsOnBeat()) {
			TriggerOnBeatFlash();
			PushCombo(TEXT("GP SLAM +BOOST"), FLinearColor(1.f, 0.55f, 0.15f));
		} else {
			PushCombo(TEXT("GROUND POUND"), FLinearColor(1.f, 0.55f, 0.15f));
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
	
	bDoubleJumpUsed    = false;
	DoubleJumpCooldown = 0.f;

	Super::ProcessLanded(Hit, remainingTime, Iterations);

	if (BhopState == EBhopState::Active)
	{
		if (bGPInputBuffered && GPInputBufferTimer > 0.f) {
			bGPInputBuffered = false; GPInputBufferTimer = 0.f;
			ExecutePlayerJump(true); 
			PushCombo(TEXT("BOOST JUMP"), FLinearColor(1.f, 0.55f, 0.15f));
			OnBhopLanded.Broadcast(GetHorizontalSpeed());
			return;
		}

		if (bJumpInputBuffered && JumpInputBufferTimer > 0.f) {
			bJumpInputBuffered = false; JumpInputBufferTimer = 0.f;
			const bool bWasFromGP = bGPLandedRecently && GPComboTimer > 0.f;
			
			if (IsOnBeat()) {
				bBonusHopRequested = true;
				TriggerOnBeatFlash();
				if (!bAutoJumpActive) {
					bAutoJumpActive = true;
					PushCombo(TEXT("AUTO JUMP ON"), FLinearColor(0.55f, 1.f, 0.35f));
				}
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

	if (BhopState == EBhopState::PowerBoost && IsMovingOnGround()) {
		const float BoostSpd  = MaxWalkSpeed * BoostSpeedMultiplier;
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

	if (UPcMusicAnalysisSubsystem* Sub = GetWorld()->GetSubsystem<UPcMusicAnalysisSubsystem>())
		if (Sub->IsReadyForPlayback())
			MaxWalkSpeed = Sub->GetCurrentPulsePreset().MaxGroundSpeed;

	if (BhopState != EBhopState::PowerBoost) {
		const float CurH    = GetHorizontalSpeed();
		const float HardCap = MaxWalkSpeed * HardSpeedCapMult;

		if (CurH > MaxWalkSpeed) {
			const float Excess    = CurH - MaxWalkSpeed;
			const float DecayThis = OverspeedDecayRate * DeltaTime * (Excess / MaxWalkSpeed);
			const float NewH      = FMath::Max(CurH - DecayThis, MaxWalkSpeed);
			const FVector Dir2D   = FVector(Velocity.X, Velocity.Y, 0.f).GetSafeNormal();
			if (!Dir2D.IsZero()) {
				Velocity.X = Dir2D.X * FMath::Min(NewH, HardCap);
				Velocity.Y = Dir2D.Y * FMath::Min(NewH, HardCap);
			}
		}
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

	if (BoostCooldown           > 0.f) BoostCooldown           = FMath::Max(0.f, BoostCooldown           - DeltaTime);
	if (BoostGraceTimer         > 0.f) BoostGraceTimer         = FMath::Max(0.f, BoostGraceTimer         - DeltaTime);
	if (SlideImpulseTimer       > 0.f) { SlideImpulseTimer -= DeltaTime; if (SlideImpulseTimer <= 0.f) bSlideImpulseActive = false; }
	if (WallEjectImmunityTimer  > 0.f) WallEjectImmunityTimer  = FMath::Max(0.f, WallEjectImmunityTimer  - DeltaTime);
	if (DoubleJumpCooldown      > 0.f) DoubleJumpCooldown      = FMath::Max(0.f, DoubleJumpCooldown      - DeltaTime);
	if (OnBeatFlashTimer        > 0.f) OnBeatFlashTimer        = FMath::Max(0.f, OnBeatFlashTimer        - DeltaTime);
	
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
	if (UPcMusicAnalysisSubsystem* Sub = GetWorld()->GetSubsystem<UPcMusicAnalysisSubsystem>())
		if (Sub->IsReadyForPlayback()) TargetSpeed = Sub->GetCurrentPulsePreset().MaxGroundSpeed;

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
	if (UPcMusicAnalysisSubsystem* Sub = GetWorld()->GetSubsystem<UPcMusicAnalysisSubsystem>())
		if (Sub->IsReadyForPlayback()) TargetAirSpeed = Sub->GetCurrentPulsePreset().MaxAirSpeed;

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