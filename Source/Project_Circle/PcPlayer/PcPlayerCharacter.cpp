// ==========================================
// FILE: PcPlayerCharacter.cpp
// PATH: Source/Project_Circle/PcPlayer/PcPlayerCharacter.cpp
// ==========================================
#include "PcPlayerCharacter.h"
#include "PcDebugHUD.h"
#include "Project_Circle/SkateSystem/PcSkateComponent.h"
#include "Project_Circle/GravitySystem/PcGravityMovementComponent.h"
#include "Project_Circle/Weapon/PcWeapon.h"
#include "FlowSystem/PcFlowMechanicComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"

APcPlayerCharacter::APcPlayerCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	SpringArmComp = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArmComp"));
	SpringArmComp->SetupAttachment(GetCapsuleComponent());
	SpringArmComp->SetRelativeLocation(FVector(0, 0, 60.0f)); 
	SpringArmComp->TargetArmLength = 0.0f; 
	SpringArmComp->bDoCollisionTest = false; 
	SpringArmComp->bUsePawnControlRotation = false; 
	SpringArmComp->bInheritPitch = true;
	SpringArmComp->bInheritYaw = true;
	SpringArmComp->bInheritRoll = true; 
	SpringArmComp->bEnableCameraRotationLag = false;

	CameraComp = CreateDefaultSubobject<UCameraComponent>(TEXT("CameraComp"));
	CameraComp->SetupAttachment(SpringArmComp);
	CameraComp->bUsePawnControlRotation = false; 

	// Skate attached to Camera
	SkateComp = CreateDefaultSubobject<UPcSkateComponent>(TEXT("HoverboardComp"));
	SkateComp->SetupAttachment(CameraComp);
	SkateComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	if (GetCharacterMovement())
	{
		GetCharacterMovement()->GravityScale = 0.0f;
		GetCharacterMovement()->DefaultLandMovementMode = MOVE_Flying;
	}

	GravityComp = CreateDefaultSubobject<UPcGravityMovementComponent>(TEXT("GravityComp"));
	FlowComp = CreateDefaultSubobject<UPcFlowMechanicComponent>(TEXT("FlowComp"));

	DriftSparksComp = CreateDefaultSubobject<UNiagaraComponent>(TEXT("DriftSparksComp"));
	DriftSparksComp->SetupAttachment(GetCapsuleComponent());
	DriftSparksComp->SetRelativeLocation(FVector(0, 0, -80.0f)); 
	DriftSparksComp->bAutoActivate = false; 
}

void APcPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();
	CurrentSpeed = BaseMoveSpeed;
	
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (PC->PlayerCameraManager)
		{
			PC->PlayerCameraManager->ViewPitchMin = -179.9f;
			PC->PlayerCameraManager->ViewPitchMax = 179.9f;
			PC->PlayerCameraManager->ViewRollMin = -179.9f;
			PC->PlayerCameraManager->ViewRollMax = 179.9f;
		}
	}

	if (GravityComp)
	{
		GravityComp->MovementMode = EPcMovementMode::Skater;
		GravityComp->bOrientRotationToMovement = false; 
		GravityComp->MaxSpeed = 10000.0f; 
		GravityComp->Acceleration = 10000.0f;
		GravityComp->Deceleration = 0.0f; 
		GravityComp->RotationInterpSpeed = 10.0f;
		DriftBodyTurnRate = 140.0f;
	}

	// SPAWN AND ATTACH WEAPON (ACTOR)
	if (StartingWeaponClass)
	{
		FActorSpawnParameters P; P.Owner = this; P.Instigator = this;
		CurrentWeapon = GetWorld()->SpawnActor<APcWeapon>(StartingWeaponClass, GetActorTransform(), P);
		if (CurrentWeapon) CurrentWeapon->AttachToPlayer(this);
	}
}

// ==========================================
// INPUTS
// ==========================================

void APcPlayerCharacter::Input_Move(FVector2D Value) 
{ 
	if (bIsWipeout) { CurrentInput = FVector::ZeroVector; return; }
	CurrentInput = FVector(Value.X, Value.Y, 0.0f);
	
	// Cache non-zero input for the Dash logic
	if (!Value.IsZero())
	{
		LastValidInput = Value;
	}
}

void APcPlayerCharacter::Input_Look(FVector2D Value)
{
	if (Value.IsZero()) return;
	AddActorLocalRotation(FRotator(0.0f, Value.X, 0.0f));
	float NewPitch = CameraPitch + Value.Y;
	NewPitch = FMath::Clamp(NewPitch, -89.0f, 89.0f);
	float PitchDelta = NewPitch - CameraPitch;
	SpringArmComp->AddLocalRotation(FRotator(PitchDelta, 0.0f, 0.0f));
	CameraPitch = NewPitch;
	
	if (CurrentWeapon) CurrentWeapon->ApplyInputForSway(Value);
}

void APcPlayerCharacter::Input_StartDrift() 
{ 
	if (bIsWipeout) return;

	// --- 1. LANDING/BUFFER LOGIC ---
	if (bIsJumping || (GravityComp && GravityComp->IsFalling()))
	{
		DriftInputBufferTimer = PreLandBufferTime; 
	}
	
	if (bPendingLandingResolution)
	{
		if (TimeSinceLanded <= PostLandPerfectWindow) ResolvePerfectLand();
		else ResolveSoftLand();
		bPendingLandingResolution = false; 
	}

	if (bIsWobbling) return;

	// --- 2. NEW DASH LOGIC (Consumes Charge) ---
	PerformDash();
}

void APcPlayerCharacter::Input_StopDrift() 
{ 
	// FIX: Allow player to exit the drift state by releasing the button
	bIsDrifting = false;
}

void APcPlayerCharacter::Input_StartAttack() 
{ 
	if (CurrentWeapon) CurrentWeapon->StartPrimaryFire(); 
}

void APcPlayerCharacter::Input_StopAttack() { if (CurrentWeapon) CurrentWeapon->StopPrimaryFire(); }
void APcPlayerCharacter::Input_FireLaser() { if (CurrentWeapon) CurrentWeapon->FireLaserAttack(); }

void APcPlayerCharacter::Input_JumpTrigger()
{
	if (bIsWipeout) return;

	// LANDING RESOLUTION: BUNNY HOP
	if (bPendingLandingResolution && TimeSinceLanded < BunnyHopWindow)
	{
		ResolveBunnyHop();
		return; 
	}

	if (!bIsJumping) PerformJump(); 
	else InputBufferTimer = 0.2f; 
}

// ==========================================
// GAMEPLAY LOGIC
// ==========================================

void APcPlayerCharacter::TakeHit() 
{ 
	// 1. IGNORE DAMAGE if already Wiping Out OR currently Invulnerable
	if (bIsWipeout || bIsInvulnerable) return;

	if (FlowComp) 
	{ 
		// RESET MULTIPLIER ON HIT
		FlowComp->ResetMultiplier();

		// NEW DAMAGE LOGIC: Lose Charge
		float ChargePenalty = 1.0f;
		
		// If we are at 0 charge, a hit is fatal (Wipeout)
		// Or if the penalty drops us below 0
		if (FlowComp->CurrentCharge <= 0.01f)
		{
			TriggerWipeout();
		}
		else
		{
			// Subtract charge (Add negative)
			FlowComp->AddCharge(-ChargePenalty);

			// Trigger Instability
			TriggerWobble();
			
			// START COOLDOWN
			bIsInvulnerable = true;
			InvulnerabilityTimer = InvulnerabilityDuration;

			if (APlayerController* PC = Cast<APlayerController>(GetController()))
				if (APcDebugHUD* HUD = Cast<APcDebugHUD>(PC->GetHUD()))
					HUD->AddStyleMessage("HIT! (-1 Charge, Multiplier Reset)", EStyleEventType::Bad);
		}
	}
}

void APcPlayerCharacter::TriggerWipeout()
{
	bIsWipeout = true;
	WipeoutTimer = WipeoutSpinDuration;
	WipeoutMaxDuration = WipeoutSpinDuration; // Cache for Lerp
	bIsDrifting = false;
	
	// FORCE PUSH
	FVector PushDir = GravityComp->GetCurrentVelocity().GetSafeNormal();
	if (PushDir.IsZero()) PushDir = GetActorForwardVector();
	
	CurrentSpeed = WipeoutPushSpeed;
	GravityComp->SetVelocity(PushDir * WipeoutPushSpeed);

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
		if (APcDebugHUD* HUD = Cast<APcDebugHUD>(PC->GetHUD()))
			HUD->AddStyleMessage("!!! WIPEOUT !!!", EStyleEventType::Bad);
}

void APcPlayerCharacter::TriggerWobble()
{
	bIsWobbling = true;
	WobbleTimer = WobbleDuration;
}

void APcPlayerCharacter::ResolvePerfectLand()
{
	bPendingLandingResolution = false;
	
	if (FlowComp) 
	{
		// THE JACKPOT: +2.0 Charge
		FlowComp->AddCharge(2.0f);
		
		// MULTIPLIER INCREASE
		FlowComp->IncreaseMultiplier(1.0f);

		FlowComp->TriggerHitStop(GetWorld());
	}

	CurrentSpeed += PerfectLandSpeedBoost;
	FOVImpulse = BoostFOVImpulse;
	
	// Reset Drift Stamina buffer for comboing
	DriftStamina = MaxDriftStamina;
	InfiniteStaminaTimer = PerfectLandStaminaBuffer;

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
		if (APcDebugHUD* HUD = Cast<APcDebugHUD>(PC->GetHUD()))
			HUD->AddStyleMessage("PERFECT LAND (+2.0 Charge, +1x Mult)", EStyleEventType::Good);
}

void APcPlayerCharacter::ResolveBunnyHop()
{
	bPendingLandingResolution = false;
	
	// VECTOR SNAPPING
	if (!CurrentInput.IsZero() && GravityComp)
	{
		FVector Vel = GravityComp->GetCurrentVelocity();
		float Speed = Vel.Size();
		
		FVector CamFwd = CameraComp->GetForwardVector();
		FVector CamRight = CameraComp->GetRightVector();
		FVector SurfaceNormal = GravityComp->GetSurfaceNormal();
		
		CamFwd = FVector::VectorPlaneProject(CamFwd, SurfaceNormal).GetSafeNormal();
		CamRight = FVector::VectorPlaneProject(CamRight, SurfaceNormal).GetSafeNormal();
		
		FVector NewDir = (CamFwd * CurrentInput.X) + (CamRight * CurrentInput.Y);
		if (!NewDir.IsZero())
		{
			NewDir.Normalize();
			GravityComp->SetVelocity(NewDir * Speed);
		}
	}

	PerformJump(); 
	
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
		if (APcDebugHUD* HUD = Cast<APcDebugHUD>(PC->GetHUD()))
			HUD->AddStyleMessage("Bunny Hop", EStyleEventType::Neutral);
}

void APcPlayerCharacter::ResolveSoftLand()
{
	bPendingLandingResolution = false;
	// if (FlowComp) FlowComp->SetFrozen(false); // No longer needed
	
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
		if (APcDebugHUD* HUD = Cast<APcDebugHUD>(PC->GetHUD()))
			HUD->AddStyleMessage("Soft Land", EStyleEventType::Neutral);
}

void APcPlayerCharacter::PerformJump()
{
	if (!GravityComp) return;

	bIsJumping = true;
	JumpPhaseTime = 0.0f;
	CurrentJumpPeak = JumpPeakHeight;
	
	GravityComp->bSnapToHoverHeight = true; 
	GravityComp->VerticalSmoothing = 0.0f; 

	if (JumpLaunchFX) UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), JumpLaunchFX, GetActorLocation());
}

void APcPlayerCharacter::UpdateJumpLogic(float DeltaTime)
{
	bool bPhysicsSaysFalling = GravityComp->IsFalling();
	bool bIsAirborneNow = bPhysicsSaysFalling || bIsJumping;

	// LANDING TRIGGER
	if (bWasFalling && !bPhysicsSaysFalling && !bIsJumping)
	{
		OnLandedHit();
	}
	bWasFalling = bIsAirborneNow;

	if (bIsJumping)
	{
		JumpPhaseTime += DeltaTime;
		float Alpha = (JumpPhaseTime / WaveDuration); 
		
		if (Alpha >= 1.0f)
		{
			bIsJumping = false;
			GravityComp->HoverHeight = 0.0f; 
			GravityComp->bSnapToHoverHeight = false; 
			GravityComp->VerticalSmoothing = 10.0f; 
		}
		else
		{
			float SineVal = FMath::Sin(Alpha * UE_PI); 
			GravityComp->HoverHeight = SineVal * JumpPeakHeight;
			GravityComp->bSnapToHoverHeight = true; 
			GravityComp->VerticalSmoothing = 0.0f;
		}
	}
	else
	{
		CurrentCameraSink = FMath::FInterpTo(CurrentCameraSink, 0.0f, DeltaTime, LandingSinkSpeed);
	}
}

void APcPlayerCharacter::OnLandedHit()
{
	// 1. BUFFERS
	if (DriftInputBufferTimer > 0.0f)
	{
		ResolvePerfectLand();
		DriftInputBufferTimer = 0.0f; 
		InputBufferTimer = 0.0f;
		return;
	}

	if (InputBufferTimer > 0.0f)
	{
		ResolveBunnyHop();
		InputBufferTimer = 0.0f;
		return;
	}

	// 2. OPEN WINDOW
	bPendingLandingResolution = true;
	TimeSinceLanded = 0.0f;
}

// ==========================================
// MAIN LOOP
// ==========================================

void APcPlayerCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!GravityComp || !FlowComp) return;

	// --- 0. HANDLE INVULNERABILITY TIMER ---
	if (bIsInvulnerable)
	{
		InvulnerabilityTimer -= DeltaTime;
		if (InvulnerabilityTimer <= 0.0f)
		{
			bIsInvulnerable = false;
		}
	}

	// --- 1. HANDLE WIPEOUT ---
	if (bIsWipeout)
	{
		WipeoutTimer -= DeltaTime;
		
		float Alpha = 1.0f - (WipeoutTimer / WipeoutMaxDuration);
		Alpha = FMath::Clamp(Alpha, 0.0f, 1.0f);
		float SpinAngle = FMath::InterpEaseOut(0.0f, 360.0f, Alpha, 2.0f);

		if (SkateComp)
		{
			SkateComp->SetRelativeRotation(FRotator(0, SpinAngle, 0));
		}

		CurrentSpeed = FMath::FInterpTo(CurrentSpeed, 0.0f, DeltaTime, 1.0f);
		GravityComp->SetVelocity(GravityComp->GetCurrentVelocity().GetSafeNormal() * CurrentSpeed);

		if (WipeoutTimer <= 0.0f)
		{
			bIsWipeout = false;
			CurrentSpeed = 0.0f;
			if (SkateComp) SkateComp->SetRelativeRotation(FRotator::ZeroRotator);
		}
		CurrentInput = FVector::ZeroVector; 
		return;
	}

	// --- 2. LANDING WINDOW ---
	if (bPendingLandingResolution)
	{
		TimeSinceLanded += DeltaTime;
		if (TimeSinceLanded > SafeSlideWindow)
		{
			bPendingLandingResolution = false;
		}
	}

	// --- 3. BUFFERS ---
	if (DriftInputBufferTimer > 0.0f) DriftInputBufferTimer -= DeltaTime;
	if (InputBufferTimer > 0.0f) InputBufferTimer -= DeltaTime;

	// --- 4. WOBBLE ---
	if (bIsWobbling)
	{
		WobbleTimer -= DeltaTime;
		if (WobbleTimer <= 0.0f) bIsWobbling = false;
	}

	// --- 5. STAMINA / DRIFT LOGIC (The Fuse) ---
	if (DriftBufferTimer > 0.0f) DriftBufferTimer -= DeltaTime;
	if (InfiniteStaminaTimer > 0.0f) InfiniteStaminaTimer -= DeltaTime;

	// Note: Regen Logic is in ApplyGripPhysics & ApplyDriftPhysics.
	// Here we handle the instant fuse reset when NOT drifting.
	if (!bIsDrifting)
	{
		DriftStamina = MaxDriftStamina;
	}

	// --- 6. STANDARD UPDATES ---
	UpdateJumpLogic(DeltaTime);
	UpdateSkaterPhysics(DeltaTime);
	UpdateVisuals(DeltaTime);

	if (SkateComp && CameraComp && GravityComp)
	{
		FVector VelocityDir = GravityComp->GetCurrentVelocity().GetSafeNormal();
		if (VelocityDir.IsZero()) VelocityDir = GetActorForwardVector();

		SkateComp->UpdateBoardState(
			DeltaTime,
			CurrentSpeed,
			VelocityDir,
			CurrentInput.Y,
			CurrentInput.X,
			bIsDrifting,
			bIsJumping,
			VisualDistToFloor,
			CameraComp->GetRelativeRotation().Pitch,
			bIsWobbling ? 1.0f : 0.0f 
		);
	}

	CurrentInput = FVector::ZeroVector;
}


void APcPlayerCharacter::UpdateVisuals(float DeltaTime)
{
	float TargetRestingFOV = BaseFOV; 
	CurrentFOVMod = FMath::FInterpTo(CurrentFOVMod, TargetRestingFOV - BaseFOV, DeltaTime, 2.0f);
	FOVImpulse = FMath::FInterpTo(FOVImpulse, 0.0f, DeltaTime, 5.0f);

	if (CameraComp)
	{
		CameraComp->SetFieldOfView(BaseFOV + CurrentFOVMod + FOVImpulse);
		
		FVector NewLoc = CameraComp->GetRelativeLocation();
		NewLoc.Z = -CurrentCameraSink; 
		CameraComp->SetRelativeLocation(NewLoc);

		float TargetRoll = CurrentInput.Y * CameraTiltAmount; 
		if (bIsDrifting) TargetRoll *= 2.0f;
		CurrentCameraRoll = FMath::FInterpTo(CurrentCameraRoll, TargetRoll, DeltaTime, CameraTiltSpeed);

		FRotator NewRot = CameraComp->GetRelativeRotation();
		NewRot.Roll = CurrentCameraRoll;
		CameraComp->SetRelativeRotation(NewRot);
	}

	VisualDistToFloor = -1.0f;
	if (bIsJumping || GravityComp->IsFalling())
	{
		FVector Start = GetActorLocation();
		FVector Down = -GravityComp->GetSurfaceNormal();
		FVector End = Start + (Down * 600.0f); 
		FHitResult Hit;
		FCollisionQueryParams Params; Params.AddIgnoredActor(this);
		if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_WorldStatic, Params))
		{
			VisualDistToFloor = Hit.Distance;
		}
	}
}

void APcPlayerCharacter::UpdateSkaterPhysics(float DeltaTime)
{
	FVector CurrentVel = GravityComp->GetCurrentVelocity();
	FVector SurfaceNormal = GravityComp->GetSurfaceNormal();
	FVector CurrentVelDir = CurrentVel.GetSafeNormal();
	if (CurrentVel.SizeSquared() < 1.0f) CurrentVelDir = GetActorForwardVector();

	FVector CamFwd = FVector::VectorPlaneProject(CameraComp->GetForwardVector(), SurfaceNormal).GetSafeNormal();
	FVector CamRight = FVector::VectorPlaneProject(CameraComp->GetRightVector(), SurfaceNormal).GetSafeNormal();
	
	FVector RawInputDir = (CamFwd * CurrentInput.X) + (CamRight * CurrentInput.Y);
	FVector InputDir = FVector::ZeroVector;
	if (RawInputDir.SizeSquared() > 0.01f) InputDir = RawInputDir.GetSafeNormal();

	DebugLastVelocityDir = CurrentVelDir;
	DebugLastInputDir = InputDir;

	bool bIsPhysicallyFalling = GravityComp->IsFalling();
	bool bIsAirborne = bIsJumping || bIsPhysicallyFalling;
	bIsAirborneDebug = bIsAirborne;

	if (InputDir.IsZero())
	{
		if (bIsAirborne) CurrentSpeed -= 400.0f * DeltaTime; 
		else CurrentSpeed -= BrakingDeceleration * DeltaTime; 
		
		CurrentSpeed = FMath::Max(0.0f, CurrentSpeed);
		
		if (DriftSparksComp->IsActive()) DriftSparksComp->Deactivate();
	}
	else if (bIsDrifting)
	{
		// Updated call
		ApplyDriftPhysics(DeltaTime, InputDir, CurrentVelDir, bIsAirborne);
	}
	else
	{
		ApplyGripPhysics(DeltaTime, InputDir, CurrentVelDir, bIsAirborne);
		if (DriftSparksComp->IsActive()) DriftSparksComp->Deactivate();
	}

	// APPLY
	if (bIsJumping)
	{
		GravityComp->SetVelocity(CurrentVelDir * CurrentSpeed);
	}
	else if (bIsPhysicallyFalling)
	{
		FVector CurrentPhysVel = GravityComp->GetCurrentVelocity();
		float VerticalMag = FVector::DotProduct(CurrentPhysVel, SurfaceNormal);
		FVector VerticalVec = SurfaceNormal * VerticalMag;
		GravityComp->SetVelocity((CurrentVelDir * CurrentSpeed) + VerticalVec);
	}
	else
	{
		GravityComp->SetVelocity(CurrentVelDir * CurrentSpeed);
	}
	
	// FIX: Removed call to FlowComp->UpdateFlowLogic(...) as it no longer exists.
}

void APcPlayerCharacter::ApplyGripPhysics(float DeltaTime, FVector InputDir, FVector& CurrentVelDir, bool bIsAirborne)
{
	// --- STAMINA REGEN ---
	if (!bIsAirborne && InfiniteStaminaTimer <= 0.0f)
	{
		float RegenRate = StaminaRegenGround;
		if (!InputDir.IsZero())
		{
			float Dot = FVector::DotProduct(CurrentVelDir, InputDir.GetSafeNormal());
			if (Dot < 0.98f) RegenRate += StaminaRegenTurnBonus;
		}
		DriftStamina = FMath::Clamp(DriftStamina + (RegenRate * DeltaTime), 0.0f, MaxDriftStamina);
	}

	// --- DETERMINE ACTIVE BASE SPEED ---
	// If Overdrive -> 1200. If Normal -> 800.
	float ActiveBaseSpeed = 800.0f;
	if (FlowComp && FlowComp->IsOverdrive())
	{
		ActiveBaseSpeed = 1200.0f;
	}

	// =========================================================================
	// STATE MACHINE: MOMENTUM vs CONTROL
	// =========================================================================

	// Are we moving faster than the active base speed?
	bool bIsOverspeed = CurrentSpeed > (ActiveBaseSpeed + 10.0f);

	if (bIsOverspeed)
	{
		// === MOMENTUM MODE ===
		// The player is gliding/flying. Input STEERS, but does not stop/start speed.
		
		// 1. DRAG (Bleed off speed)
		float DragFactor = 0.0f;
		if (bIsAirborne)
		{
			// AIR: Low drag. Preserve speed.
			DragFactor = 0.5f; 
		}
		else
		{
			// GROUND: High drag. Slide to normal speed.
			DragFactor = 5.0f; 
		}
		
		CurrentSpeed = FMath::FInterpTo(CurrentSpeed, ActiveBaseSpeed, DeltaTime, DragFactor);

		// 2. STEERING (Redirect Momentum)
		if (!InputDir.IsZero())
		{
			if (bIsAirborne)
			{
				// AIR: High control to redirect jumps
				float AirTurn = GripSteeringRate * 3.0f; 
				if (bIsWobbling) AirTurn *= 0.2f;
				CurrentVelDir = FMath::VInterpNormalRotationTo(CurrentVelDir, InputDir, DeltaTime, AirTurn);
			}
			else
			{
				// GROUND: Snap turn (Doom style), but preserving the slide speed
				CurrentVelDir = InputDir;
			}
		}
	}
	else
	{
		// === CONTROL MODE ===
		// Standard movement. We are below or at Base Speed.
		
		if (InputDir.IsZero())
		{
			// INSTANT STOP (Friction)
			// Air drifting vs Ground snap
			float StopSpeed = bIsAirborne ? 1.0f : 15.0f; 
			CurrentSpeed = FMath::FInterpTo(CurrentSpeed, 0.0f, DeltaTime, StopSpeed);
		}
		else
		{
			// INSTANT GO
			
			// 1. Set Direction
			if (bIsAirborne)
			{
				// Air turn (Smoother)
				float AirTurn = GripSteeringRate * 3.0f;
				CurrentVelDir = FMath::VInterpNormalRotationTo(CurrentVelDir, InputDir, DeltaTime, AirTurn);
			}
			else
			{
				// Ground turn (Instant)
				CurrentVelDir = InputDir;
			}

			// 2. Set Speed (Snap to ActiveBaseSpeed)
			float AccelSpeed = bIsAirborne ? 2.0f : 15.0f;
			CurrentSpeed = FMath::FInterpTo(CurrentSpeed, ActiveBaseSpeed, DeltaTime, AccelSpeed);
		}
	}

	CurrentSpeed = FMath::Max(0.0f, CurrentSpeed);
	DebugSlipAngle = 0.0f;
}

// FIX: Signature updated to match Header (4 arguments). Removed MaxSpeedForTier.
bool APcPlayerCharacter::ApplyDriftPhysics(float DeltaTime, FVector InputDir, FVector& CurrentVelDir, bool bIsAirborne)
{
	// --- 1. CONSTANT DRAIN (Requirement: Drain while holding button) ---
	// We drain immediately, regardless of angle.
	float Drain = StaminaDrainRate * DeltaTime;
	DriftStamina = FMath::Clamp(DriftStamina - Drain, 0.0f, MaxDriftStamina);

	// --- 2. CALCULATE FADE (Requirement: Interpolate back to ground mode) ---
	// Alpha 1.0 = Full Stamina (Slide)
	// Alpha 0.0 = Empty Stamina (Grip)
	float BlendAlpha = 0.0f;
	if (MaxDriftStamina > 0.0f) BlendAlpha = DriftStamina / MaxDriftStamina;

	// If empty, force exit to Grip immediately
	if (DriftStamina <= 0.001f)
	{
		CurrentVelDir = FMath::VInterpNormalRotationTo(CurrentVelDir, InputDir, DeltaTime, GripSteeringRate); 
		if (!bIsAirborne) CurrentSpeed -= DriftLinearDrag * 2.0f * DeltaTime;
		CurrentSpeed = FMath::Max(0.0f, CurrentSpeed);
		if (DriftSparksComp->IsActive()) DriftSparksComp->Deactivate();
		return false; 
	}

	// --- 3. BLENDED STEERING ---
	// Lerp between Grip (Snappy) and Drift (Loose) based on remaining stamina.
	float TargetTurnRate = FMath::Lerp(GripSteeringRate, DriftBodyTurnRate, BlendAlpha);

	// Wobble penalty (if hit)
	if (bIsWobbling) TargetTurnRate *= 0.2f;

	// Apply the blended steering
	CurrentVelDir = FMath::VInterpNormalRotationTo(CurrentVelDir, InputDir, DeltaTime, TargetTurnRate);

	// --- 4. SPEED MANAGEMENT ---
	float DragForce = 200.0f; 
	if (!bIsAirborne) CurrentSpeed -= DragForce * DeltaTime;

	// Internal Logic: Overdrive check
	float TargetSpeed = FlowComp->IsOverdrive() ? 1200.0f : 800.0f;

	if (CurrentSpeed < TargetSpeed)
	{
		CurrentSpeed += GroundAcceleration * DeltaTime;
	}
	else
	{
		CurrentSpeed = FMath::FInterpTo(CurrentSpeed, TargetSpeed, DeltaTime, 0.5f);
	}

	// --- 5. POCKET LOGIC (Refund Only) ---
	float Dot = FVector::DotProduct(CurrentVelDir, InputDir.GetSafeNormal());
	float AngleDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Dot, -1.0f, 1.0f)));
	DebugSlipAngle = AngleDeg;

	bool bInPocket = false;
	
	// Pocket = Skill Check for Reward (Charge), NOT for Cost (Stamina)
	if (AngleDeg > 10.0f && AngleDeg < 90.0f)
	{
		bInPocket = true;
		
		float CurveQuality = FMath::Clamp((AngleDeg - 10.0f) / 35.0f, 0.0f, 1.0f);
		
		// REFUND CHARGE
		float ChargeRegen = 0.8f * DeltaTime; 
		FlowComp->AddCharge(ChargeRegen);

		// Physics Boost
		if (CurrentSpeed < TargetSpeed * 1.3f)
		{
			float AirBoost = bIsAirborne ? 1.5f : 1.0f; 
			CurrentSpeed += DriftAcceleration * CurveQuality * AirBoost * DeltaTime;
		}

		// Camera Push
		float PushStrength = (AngleDeg - 5.0f) / 60.0f;
		float TargetSink = DriftCameraSinkAmount * PushStrength;
		CurrentCameraSink = FMath::FInterpTo(CurrentCameraSink, TargetSink, DeltaTime, 5.0f);
		
		if (!DriftSparksComp->IsActive()) DriftSparksComp->Activate();
	}
	else
	{
		if (DriftSparksComp->IsActive()) DriftSparksComp->Deactivate();
	}

	CurrentSpeed = FMath::Max(0.0f, CurrentSpeed);
	return bInPocket;
}

void APcPlayerCharacter::PerformDash()
{
	if (!bCanDash || !GravityComp || !FlowComp) return;

	// --- 1. CHECK & SPEND COST ---
	if (!FlowComp->TrySpendCharge(1.0f))
	{
		return;
	}

	// --- 2. CALCULATE DIRECTION ---
	FVector2D InputToUse = FVector2D(CurrentInput.X, CurrentInput.Y);
	if (CurrentInput.IsZero() && !LastValidInput.IsZero())
	{
		InputToUse = LastValidInput;
	}

	FVector SurfaceNormal = GravityComp->GetSurfaceNormal();
	FVector CamFwd = FVector::VectorPlaneProject(CameraComp->GetForwardVector(), SurfaceNormal).GetSafeNormal();
	FVector CamRight = FVector::VectorPlaneProject(CameraComp->GetRightVector(), SurfaceNormal).GetSafeNormal();

	FVector DashDir;
	if (InputToUse.IsZero())
	{
		DashDir = CamFwd;
	}
	else
	{
		DashDir = (CamFwd * InputToUse.X) + (CamRight * InputToUse.Y);
		DashDir.Normalize();
	}

	// --- 3. APPLY SPEED ---
	float TargetDashSpeed = BaseMoveSpeed + DashImpulseStrength;

	if (CurrentSpeed > TargetDashSpeed)
	{
		// Maintain existing momentum if already super fast
	}
	else
	{
		CurrentSpeed = TargetDashSpeed;
	}

	GravityComp->SetVelocity(DashDir * CurrentSpeed);

	// --- 4. ENTER DRIFT STATE (FIX) ---
	// This is the key fix. The Dash is the entry point.
	// As long as the player holds the button, they remain in this state.
	bIsDrifting = true;

	// --- 5. VISUALS ---
	FOVImpulse = 20.0f;
	TriggerWobble(); 
	WobbleTimer = 0.15f;
	
	if (DriftSparksComp) 
	{
		DriftSparksComp->Activate(true);
	}

	// --- 6. COOLDOWN ---
	bCanDash = false;
	GetWorld()->GetTimerManager().SetTimer(TimerHandle_DashCooldown, this, &APcPlayerCharacter::ResetDashCooldown, DashCooldown, false);

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
		if (APcDebugHUD* HUD = Cast<APcDebugHUD>(PC->GetHUD()))
			HUD->AddStyleMessage("DASH (-1.0)", EStyleEventType::Neutral);
}

void APcPlayerCharacter::ResetDashCooldown()
{
	bCanDash = true;
}