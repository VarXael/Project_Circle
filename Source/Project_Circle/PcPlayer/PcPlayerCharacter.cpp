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

	// --- 1. LANDING/BUFFER LOGIC (Keep this, it's good game feel) ---
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

	// --- 2. DRIFT LOGIC (DISABLED) ---
	/* 
	bIsDrifting = true; 
	DriftScoreAccumulator = 0.0f;
	DriftBufferTimer = 0.2f; 
	*/

	// --- 3. NEW DASH LOGIC ---
	PerformDash();
}

void APcPlayerCharacter::Input_StopDrift() 
{ 
	// bIsDrifting = false; 
	// if (DriftScoreAccumulator > 10.0f)
	// {
	// 	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	// 		if (APcDebugHUD* HUD = Cast<APcDebugHUD>(PC->GetHUD()))
	// 			HUD->AddStyleMessage(FString::Printf(TEXT("+ Drift %.0f"), DriftScoreAccumulator), EStyleEventType::Neutral);
	// }
	// DriftScoreAccumulator = 0.0f;
}

void APcPlayerCharacter::Input_StartAttack() { if (CurrentWeapon) CurrentWeapon->StartPrimaryFire(); }
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
		// ApplyDamage returns true if Tier drops to 0 (Fatal)
		bool bIsFatal = FlowComp->ApplyDamage(); 
		
		if (bIsFatal)
		{
			TriggerWipeout();
		}
		else
		{
			// Tier Drop -> Trigger Instability
			TriggerWobble();
			
			// START COOLDOWN
			bIsInvulnerable = true;
			InvulnerabilityTimer = InvulnerabilityDuration;

			if (APlayerController* PC = Cast<APlayerController>(GetController()))
				if (APcDebugHUD* HUD = Cast<APcDebugHUD>(PC->GetHUD()))
					HUD->AddStyleMessage("HIT! (Invulnerable)", EStyleEventType::Bad);
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
	// If moving, keep sliding. If stopped, push forward.
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
    
	// PLAY CAMERA SHAKE
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		// You can use a default shake class or make a BP_Shake
		//PC->ClientStartCameraShake(UCameraShakeBase::StaticClass(), 1.0f);
	}
}

void APcPlayerCharacter::ResolvePerfectLand()
{
	bPendingLandingResolution = false;
	if (FlowComp) 
	{
		FlowComp->ForceTierUp();
		FlowComp->TriggerHitStop(GetWorld());
	}
	CurrentSpeed += PerfectLandSpeedBoost;
	FOVImpulse = BoostFOVImpulse;
	
	DriftStamina = MaxDriftStamina;
	InfiniteStaminaTimer = PerfectLandStaminaBuffer;

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
		if (APcDebugHUD* HUD = Cast<APcDebugHUD>(PC->GetHUD()))
			HUD->AddStyleMessage("PERFECT LAND!", EStyleEventType::Good);
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
	if (FlowComp) FlowComp->SetFrozen(false);
	
	// Just a visual note, no mechanic change
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

	if (FlowComp) FlowComp->SetFrozen(true);
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
	// 1. BUFFERS (Intent BEFORE landing)
	
	// Buffered Drift? -> Perfect
	if (DriftInputBufferTimer > 0.0f)
	{
		ResolvePerfectLand();
		DriftInputBufferTimer = 0.0f; 
		InputBufferTimer = 0.0f;
		return;
	}

	// Buffered Jump? -> Bhop
	if (InputBufferTimer > 0.0f)
	{
		ResolveBunnyHop();
		InputBufferTimer = 0.0f;
		return;
	}

	// Holding Drift? -> Soft Land
	if (bIsDrifting)
	{
		ResolveSoftLand();
		return;
	}

	// 2. OPEN WINDOW (For intent AFTER landing)
	bPendingLandingResolution = true;
	TimeSinceLanded = 0.0f;
	
	if (FlowComp) FlowComp->SetFrozen(false);
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
		
		// SPIN ANIMATION
		float Alpha = 1.0f - (WipeoutTimer / WipeoutMaxDuration);
		Alpha = FMath::Clamp(Alpha, 0.0f, 1.0f);
		float SpinAngle = FMath::InterpEaseOut(0.0f, 360.0f, Alpha, 2.0f);

		if (SkateComp)
		{
			SkateComp->SetRelativeRotation(FRotator(0, SpinAngle, 0));
		}

		// Apply Drag
		CurrentSpeed = FMath::FInterpTo(CurrentSpeed, 0.0f, DeltaTime, 1.0f);
		GravityComp->SetVelocity(GravityComp->GetCurrentVelocity().GetSafeNormal() * CurrentSpeed);

		if (WipeoutTimer <= 0.0f)
		{
			bIsWipeout = false;
			CurrentSpeed = 0.0f;
			if (SkateComp) SkateComp->SetRelativeRotation(FRotator::ZeroRotator);
		}
		CurrentInput = FVector::ZeroVector; 
		return; // STOP HERE during wipeout
	}

	// --- 2. LANDING WINDOW ---
	if (bPendingLandingResolution)
	{
		TimeSinceLanded += DeltaTime;
		if (TimeSinceLanded > SafeSlideWindow)
		{
			// Window expired -> Safe Land (No penalty)
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

	// --- 5. STANDARD UPDATES ---
	if (DriftBufferTimer > 0.0f) DriftBufferTimer -= DeltaTime;
	if (InfiniteStaminaTimer > 0.0f) InfiniteStaminaTimer -= DeltaTime;

	float Regen = bIsJumping ? StaminaRegenAir : StaminaRegenGround;
	if (!bIsDrifting && InfiniteStaminaTimer <= 0.0f)
	{
		if (bIsJumping || GravityComp->IsFalling())
		{
			DriftStamina = FMath::Clamp(DriftStamina + (Regen * DeltaTime), 0.0f, MaxDriftStamina);
		}
	}

	UpdateJumpLogic(DeltaTime);
	UpdateSkaterPhysics(DeltaTime);
	UpdateVisuals(DeltaTime);

	// UPDATE BOARD VISUALS
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
	float MaxSpeedForTier = BaseMoveSpeed + (FlowComp->CurrentTier * SpeedPerTier);
	
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

	bool bEffectiveDrift = false;
	bool bIsPhysicallyFalling = GravityComp->IsFalling();
	bool bIsAirborne = bIsJumping || bIsPhysicallyFalling;
	bIsAirborneDebug = bIsAirborne;

	if (InputDir.IsZero())
	{
		if (bIsAirborne) CurrentSpeed -= 400.0f * DeltaTime; 
		else CurrentSpeed -= BrakingDeceleration * DeltaTime; 
		
		CurrentSpeed = FMath::Max(0.0f, CurrentSpeed);
		
		if (DriftSparksComp->IsActive()) DriftSparksComp->Deactivate();
		FlowComp->UpdateFlowLogic(DeltaTime, false);
	}
	else if (bIsDrifting)
	{
		bEffectiveDrift = ApplyDriftPhysics(DeltaTime, InputDir, CurrentVelDir, bIsAirborne, MaxSpeedForTier);
	}
	else
	{
		ApplyGripPhysics(DeltaTime, InputDir, CurrentVelDir, bIsAirborne);
		if (DriftSparksComp->IsActive()) DriftSparksComp->Deactivate();
		FlowComp->UpdateFlowLogic(DeltaTime, false);
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

	FlowComp->UpdateFlowLogic(DeltaTime, bEffectiveDrift);
}

void APcPlayerCharacter::ApplyGripPhysics(float DeltaTime, FVector InputDir, FVector& CurrentVelDir, bool bIsAirborne)
{
	// --- STAMINA REGEN (Unchanged) ---
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

	// =========================================================================
	// STATE MACHINE: MOMENTUM vs CONTROL
	// =========================================================================

	// Are we moving faster than the engine allows by default? (e.g. Dashing, Ramps)
	bool bIsOverspeed = CurrentSpeed > (BaseMoveSpeed + 10.0f); // Small epsilon

	if (bIsOverspeed)
	{
		// === MOMENTUM MODE ===
		// The player is gliding/flying. Input STEERS, but does not stop/start speed.
		
		// 1. DRAG (Bleed off speed)
		float DragFactor = 0.0f;
		if (bIsAirborne)
		{
			// AIR: Low drag. Preserve the dash speed for a long time.
			// 0.5f means it takes seconds to drop from 2500 to 800.
			DragFactor = 0.5f; 
		}
		else
		{
			// GROUND: High drag. Slide to normal speed quickly.
			// 5.0f means you slide for ~0.5 seconds.
			DragFactor = 5.0f; 
		}
		
		CurrentSpeed = FMath::FInterpTo(CurrentSpeed, BaseMoveSpeed, DeltaTime, DragFactor);

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
		// Standard movement. We are below or at 800.
		// Logic: If input, be at 800. If no input, be at 0.
		
		if (InputDir.IsZero())
		{
			// INSTANT STOP (Friction)
			// On ground: Snap to 0. In Air: Drag to 0 slower? 
			// You requested "Snappy" on ground, keep "Skater" air.
			
			float StopSpeed = bIsAirborne ? 1.0f : 15.0f; // Air drift vs Ground snap
			CurrentSpeed = FMath::FInterpTo(CurrentSpeed, 0.0f, DeltaTime, StopSpeed);
		}
		else
		{
			// INSTANT GO
			
			// 1. Set Direction
			if (bIsAirborne)
			{
				// Air turn (Smoother than ground)
				float AirTurn = GripSteeringRate * 3.0f;
				CurrentVelDir = FMath::VInterpNormalRotationTo(CurrentVelDir, InputDir, DeltaTime, AirTurn);
			}
			else
			{
				// Ground turn (Instant)
				CurrentVelDir = InputDir;
			}

			// 2. Set Speed (Snap to 800)
			float AccelSpeed = bIsAirborne ? 2.0f : 15.0f; // Ground is instant, Air is building up
			CurrentSpeed = FMath::FInterpTo(CurrentSpeed, BaseMoveSpeed, DeltaTime, AccelSpeed);
		}
	}

	CurrentSpeed = FMath::Max(0.0f, CurrentSpeed);
	DebugSlipAngle = 0.0f;
}

bool APcPlayerCharacter::ApplyDriftPhysics(float DeltaTime, FVector InputDir, FVector& CurrentVelDir, bool bIsAirborne, float MaxSpeedForTier)
{
	// WOBBLE DAMPENING: Sluggish turn rate
	float CurrentTurnRate = bIsWobbling ? DriftBodyTurnRate * 0.2f : DriftBodyTurnRate;

	bool bInPocket = false;

	if (DriftStamina <= 0.0f)
	{
		CurrentVelDir = FMath::VInterpNormalRotationTo(CurrentVelDir, InputDir, DeltaTime, GripSteeringRate); 
		CurrentSpeed -= DriftLinearDrag * 2.0f * DeltaTime; 
		CurrentSpeed = FMath::Max(0.0f, CurrentSpeed);
		if (DriftSparksComp->IsActive()) DriftSparksComp->Deactivate();
		return false; 
	}

	CurrentVelDir = FMath::VInterpNormalRotationTo(CurrentVelDir, InputDir, DeltaTime, CurrentTurnRate);

	float DragForce = 200.0f; 
	if (!bIsAirborne) CurrentSpeed -= DragForce * DeltaTime;

	if (CurrentSpeed < BaseMoveSpeed)
	{
		CurrentSpeed += GroundAcceleration * DeltaTime;
	}
	else
	{
		CurrentSpeed = FMath::FInterpTo(CurrentSpeed, BaseMoveSpeed, DeltaTime, 0.5f);
	}

	float Dot = FVector::DotProduct(CurrentVelDir, InputDir.GetSafeNormal());
	float AngleDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(Dot, -1.0f, 1.0f)));
	DebugSlipAngle = AngleDeg;

	if (AngleDeg > 10.0f && AngleDeg < 90.0f)
	{
		bInPocket = true;
		
		float CurveQuality = FMath::Clamp((AngleDeg - 10.0f) / 35.0f, 0.0f, 1.0f);
		
		if (InfiniteStaminaTimer <= 0.0f)
		{
			float Drain = StaminaDrainRate * CurveQuality * DeltaTime;
			DriftStamina = FMath::Clamp(DriftStamina - Drain, 0.0f, 100.0f);
		}

		if (CurrentSpeed < MaxSpeedForTier * 1.3f)
		{
			float AirBoost = bIsAirborne ? 1.5f : 1.0f; 
			CurrentSpeed += DriftAcceleration * CurveQuality * AirBoost * DeltaTime;
		}

		if (FlowComp->CurrentState == EFlowState::Frozen) FlowComp->SetFrozen(false);
		
		float FlowAmount = 30.0f + (AngleDeg * 0.5f); 
		FlowComp->InjectFlow(FlowAmount * DeltaTime);

		DriftScoreAccumulator += FlowAmount * DeltaTime;

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
	if (!bCanDash || !GravityComp) return;

	// 1. Determine Input to use (Fix for "Always Forward" bug)
	// If CurrentInput is zero (frame timing issue), try the cached one.
	FVector2D InputToUse = FVector2D(CurrentInput.X, CurrentInput.Y);
	if (CurrentInput.IsZero() && !LastValidInput.IsZero())
	{
		// Only use cached input if it was recent? 
		// For now, assuming if you aren't pressing anything, LastValid is fine 
		// or we default to forward if you truly stopped.
		// Actually, let's trust CurrentInput if the player strictly stopped, 
		// but if they are moving in air, CurrentInput SHOULD be valid. 
		// The fallback helps if Input_StartDrift fires before Input_Move.
		InputToUse = LastValidInput;
	}

	// 2. Calculate Direction
	FVector SurfaceNormal = GravityComp->GetSurfaceNormal();
	FVector CamFwd = FVector::VectorPlaneProject(CameraComp->GetForwardVector(), SurfaceNormal).GetSafeNormal();
	FVector CamRight = FVector::VectorPlaneProject(CameraComp->GetRightVector(), SurfaceNormal).GetSafeNormal();

	FVector DashDir;
	if (InputToUse.IsZero())
	{
		DashDir = CamFwd; // True fallback: Look direction
	}
	else
	{
		DashDir = (CamFwd * InputToUse.X) + (CamRight * InputToUse.Y);
		DashDir.Normalize();
	}

	// 3. Apply Speed (Soft Cap Logic)
	// Threshold: The speed we WANT to be at after a dash.
	float TargetDashSpeed = BaseMoveSpeed + DashImpulseStrength;

	if (CurrentSpeed > TargetDashSpeed)
	{
		// CASE: SUPER SPEED (Already going 3000+)
		// Do NOT add speed. Just redirect the momentum.
		// We keep CurrentSpeed exactly as is.
	}
	else
	{
		// CASE: NORMAL / SLOW
		// Snap to the dash speed.
		CurrentSpeed = TargetDashSpeed;
	}

	// 4. Force Physics Update (Instant Redirect)
	GravityComp->SetVelocity(DashDir * CurrentSpeed);

	// 5. Visuals & Cooldown
	FOVImpulse = 20.0f;
	TriggerWobble(); 
	WobbleTimer = 0.15f;
	if (DriftSparksComp) DriftSparksComp->Activate(true);

	bCanDash = false;
	GetWorld()->GetTimerManager().SetTimer(TimerHandle_DashCooldown, this, &APcPlayerCharacter::ResetDashCooldown, DashCooldown, false);

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
		if (APcDebugHUD* HUD = Cast<APcDebugHUD>(PC->GetHUD()))
			HUD->AddStyleMessage("DASH!", EStyleEventType::Neutral);
}

void APcPlayerCharacter::ResetDashCooldown()
{
	bCanDash = true;
}