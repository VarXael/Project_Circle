// ==========================================
// FILE: PcPlayerCharacter.cpp
// PATH: E:\GameDev\Unreal Engine Projects\Project_Circle\Source\Project_Circle\PcPlayer\PcPlayerCharacter.cpp
// ==========================================
#include "PcPlayerCharacter.h"
#include "PcDebugHUD.h"
#include "Project_Circle/GravitySystem/PcGravityMovementComponent.h"
#include "Project_Circle/Weapon/PcWeapon.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

APcPlayerCharacter::APcPlayerCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	// 1. SPRING ARM SETUP
	SpringArmComp = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArmComp"));
	SpringArmComp->SetupAttachment(GetCapsuleComponent());
	SpringArmComp->SetRelativeLocation(FVector(0, 0, 60.0f)); // Head Height
	SpringArmComp->TargetArmLength = 0.0f; // FPS View
	
	// Critical Settings for Sphere Navigation
	SpringArmComp->bDoCollisionTest = false; // Prevent camera jumping when backing into curved walls
	
	// FIX: Set to FALSE. We handle rotation manually to keep camera aligned with gravity.
	SpringArmComp->bUsePawnControlRotation = false; 
	
	SpringArmComp->bInheritPitch = true;
	SpringArmComp->bInheritYaw = true;
	SpringArmComp->bInheritRoll = true; // Essential for walking on walls/ceilings

	// 2. CAMERA SETUP
	CameraComp = CreateDefaultSubobject<UCameraComponent>(TEXT("CameraComp"));
	CameraComp->SetupAttachment(SpringArmComp);
	CameraComp->bUsePawnControlRotation = false; // Camera strictly follows the SpringArm

	// 3. DISABLE STANDARD MOVEMENT
	if (GetCharacterMovement())
	{
		GetCharacterMovement()->GravityScale = 0.0f;
		GetCharacterMovement()->DefaultLandMovementMode = MOVE_Flying;
	}

	// 4. GRAVITY COMPONENT
	GravityComp = CreateDefaultSubobject<UPcGravityMovementComponent>(TEXT("GravityComp"));
}

void APcPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();
	
	CurrentSpeed = BaseMoveSpeed;
	FuseTimer = 0.0f; 

	// --- UNLOCK CAMERA LIMITS ---
	// We allow full 360 rotation on Pitch and Roll.
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
	}

	if (StartingWeaponClass)
	{
		FActorSpawnParameters P; P.Owner = this; P.Instigator = this;
		CurrentWeapon = GetWorld()->SpawnActor<APcWeapon>(StartingWeaponClass, GetActorTransform(), P);
		if (CurrentWeapon) CurrentWeapon->AttachToPlayer(this);
	}
}

// --- INPUTS ---

void APcPlayerCharacter::Input_Move(FVector2D Value) 
{ 
	CurrentInput = FVector(Value.X, Value.Y, 0.0f); 
}

void APcPlayerCharacter::Input_Look(FVector2D Value)
{
	if (Value.IsZero()) return;

	// FIX: Manual Local Rotation Implementation
	
	// 1. YAW (Mouse X) -> Rotate the Actor Capsule around its local Z axis (Up).
	// Because the GravityComponent keeps "Up" aligned with the planet, this turns us "Left/Right" correctly.
	AddActorLocalRotation(FRotator(0.0f, Value.X, 0.0f));

	// 2. PITCH (Mouse Y) -> Rotate the Spring Arm locally.
	// We manually track pitch to clamp it (prevent somersaulting the camera).
	float NewPitch = CameraPitch + Value.Y;
	NewPitch = FMath::Clamp(NewPitch, -89.0f, 89.0f);
	
	float PitchDelta = NewPitch - CameraPitch;
	SpringArmComp->AddLocalRotation(FRotator(PitchDelta, 0.0f, 0.0f));
	
	CameraPitch = NewPitch;

	// 3. Weapon Sway
	if (CurrentWeapon) CurrentWeapon->ApplyInputForSway(Value);
}

void APcPlayerCharacter::Input_StartAttack() { if (CurrentWeapon) CurrentWeapon->StartPrimaryFire(); }
void APcPlayerCharacter::Input_StopAttack() { if (CurrentWeapon) CurrentWeapon->StopPrimaryFire(); }
void APcPlayerCharacter::Input_FireLaser() { if (CurrentWeapon) CurrentWeapon->FireLaserAttack(); }

void APcPlayerCharacter::TakeHit() 
{ 
	if (bIsInvulnerable) return;

	if (FlowStacks > 0)
	{
		PushStyleMessage("HIT! STACKS LOST", 1); 
		FlowStacks = 0;
		CurrentSpeed = BaseMoveSpeed;
	}
}

void APcPlayerCharacter::Input_JumpTrigger()
{
	if (bInPerfectWindow)
	{
		PerformJump(true); 
		return;
	}

	if (!bIsJumping)
	{
		PerformJump(false); 
		return;
	}

	// Buffer input if mid-air
	if (bIsJumping) InputBufferTimer = InputBufferAllowance;
}

void APcPlayerCharacter::PerformJump(bool bIsPerfect)
{
	if (FlowStacks < MaxStacks)
	{
		FlowStacks++;	
	} 
	
	bIsJumping = true;
	JumpPhaseTime = 0.0f;
	bInPerfectWindow = false;
	WindowTimer = 0.0f;
	InputBufferTimer = 0.0f;

	if (bIsPerfect)
	{
		PushStyleMessage("PERFECT BOOST!", 0); 
		FuseTimer = FuseDuration;
		CurrentSpeed += JumpImpulse; 
		SpeedLockTimer = 0.5f; 
		bIsInvulnerable = true;
	}
}

float APcPlayerCharacter::GetFuseFraction() const
{
	if (FuseDuration <= 0.0f) return 0.0f;
	return FMath::Clamp(FuseTimer / FuseDuration, 0.0f, 1.0f);
}

void APcPlayerCharacter::PushStyleMessage(FString Msg, uint8 Type)
{
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (APcDebugHUD* HUD = Cast<APcDebugHUD>(PC->GetHUD()))
		{
			HUD->AddStyleMessage(Msg, (EStyleEventType)Type);
		}
	}
}

FString APcPlayerCharacter::GetDebugInfo() const
{
	return FString::Printf(TEXT("SPD: %.0f / MAX: %.0f\nSTACKS: %d"), CurrentSpeed, GetTargetMaxSpeed(), FlowStacks);
}

// --- GAMEPLAY LOOP ---

void APcPlayerCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!GravityComp) return;

	if (InputBufferTimer > 0.0f) InputBufferTimer -= DeltaTime;

	if (SpeedLockTimer > 0.0f)
	{
		SpeedLockTimer -= DeltaTime;
		bIsInvulnerable = true;
		if (SpeedLockTimer <= 0.0f) bIsInvulnerable = false;
	}

	UpdateFlowFuse(DeltaTime);
	UpdateSkaterPhysics(DeltaTime);
	UpdateJumpLogic(DeltaTime);
}

void APcPlayerCharacter::UpdateFlowFuse(float DeltaTime)
{
	if (bIsJumping || bInPerfectWindow) return; // Frozen

	if (FuseTimer > 0.0f)
	{
		FuseTimer -= DeltaTime;
		if (FuseTimer <= 0.0f)
		{
			if (FlowStacks > 0)
			{
				FlowStacks--;
				PushStyleMessage("DECAY", 2); 
				FuseTimer = FuseDuration; 
			}
			else FuseTimer = 0.0f;
		}
	}
}

void APcPlayerCharacter::UpdateSkaterPhysics(float DeltaTime)
{
	float TargetMaxSpeed = BaseMoveSpeed + (FlowStacks * BonusSpeedPerStack);

	FVector CurrentVel = GravityComp->GetCurrentVelocity();
	FVector SurfaceNormal = GravityComp->GetSurfaceNormal();
	FVector CurrentDir = CurrentVel.GetSafeNormal();
	if (CurrentVel.SizeSquared() < 1.0f) CurrentDir = GetActorForwardVector();

	// FIX: Use Camera Component vectors instead of Control Rotation
	// Since we disabled Control Rotation mapping, we must ask the Camera where it is looking.
	FVector CamFwd = CameraComp->GetForwardVector();
	FVector CamRight = CameraComp->GetRightVector();

	// Project vectors onto the surface plane so inputs are relative to the "Floor"
	CamFwd = FVector::VectorPlaneProject(CamFwd, SurfaceNormal).GetSafeNormal();
	CamRight = FVector::VectorPlaneProject(CamRight, SurfaceNormal).GetSafeNormal();
	
	FVector InputDir = (CamFwd * CurrentInput.X) + (CamRight * CurrentInput.Y);
	InputDir.Normalize();

	// Speed Logic
	if (SpeedLockTimer > 0.0f)
	{
		// During Boost/Lock, only change direction, ignore drag/caps
		if (!InputDir.IsZero())
		{
			FVector NewDir = FMath::VInterpNormalRotationTo(CurrentDir, InputDir, DeltaTime, SteeringRate);
			CurrentDir = NewDir;
		}
	}
	else
	{
		if (InputDir.IsZero())
		{
			CurrentSpeed -= PassiveDrag * 2.0f * DeltaTime;
			if (CurrentSpeed <= 10.0f) 
			{
				CurrentSpeed = 0.0f;
				GravityComp->SetVelocity(FVector::ZeroVector);
				return; 
			}
		}
		else
		{
			// Quick Turn vs Wide Carve
			if (CurrentSpeed < 100.0f)
			{
				CurrentDir = InputDir;
				CurrentSpeed = BaseMoveSpeed * 0.5f;
			}
			else
			{
				FVector NewDir = FMath::VInterpNormalRotationTo(CurrentDir, InputDir, DeltaTime, SteeringRate);
				CurrentDir = NewDir;
			}

			// Carving Bonus
			float Dot = (CurrentDir | InputDir);
			if (Dot < 0.95f) 
			{
				float TurnIntensity = (1.0f - Dot); 
				CurrentSpeed += (CarveAcceleration * 0.5f) * TurnIntensity * DeltaTime;
			}
			else
			{
				if (CurrentSpeed < BaseMoveSpeed) CurrentSpeed += CarveAcceleration * DeltaTime; 
				else CurrentSpeed -= 20.0f * DeltaTime; 
			}
		}
	}

	if (SpeedLockTimer <= 0.0f)
	{
		if (CurrentSpeed > TargetMaxSpeed)
		{
			CurrentSpeed = FMath::FInterpTo(CurrentSpeed, TargetMaxSpeed, DeltaTime, 1.0f); 
		}
		CurrentSpeed = FMath::Clamp(CurrentSpeed, 0.0f, MaxSkimSpeed); 
	}

	GravityComp->SetVelocity(CurrentDir * CurrentSpeed);
}

void APcPlayerCharacter::UpdateJumpLogic(float DeltaTime)
{
	if (bIsJumping)
	{
		JumpPhaseTime += DeltaTime;
		GravityComp->bSnapToHoverHeight = true; 

		if (JumpPhaseTime >= WaveDuration)
		{
			// Landed
			bIsJumping = false;
			JumpPhaseTime = 0.0f;
			bInPerfectWindow = true; 
			WindowTimer = 0.0f;
			GravityComp->HoverHeight = 0.0f;
			if (InputBufferTimer > 0.0f) PerformJump(true);
			return;
		}

		// Calculate Arc
		float Alpha = (JumpPhaseTime / WaveDuration); 
		float SineVal = FMath::Sin(Alpha * PI); 
		GravityComp->HoverHeight = SineVal * JumpPeakHeight;
		return;
	}

	if (bInPerfectWindow)
	{
		GravityComp->bSnapToHoverHeight = true;
		WindowTimer += DeltaTime;

		if (WindowTimer >= PerfectWindowDuration) bInPerfectWindow = false;
		return;
	}

	GravityComp->bSnapToHoverHeight = false; 
	GravityComp->HoverHeight = FMath::FInterpTo(GravityComp->HoverHeight, 0.0f, DeltaTime, 5.0f);
}