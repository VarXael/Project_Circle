#include "PcQPlayerCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Project_Circle/MusicSystem/MusicGameplaySystem/PcMusicAnalysisSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Project_Circle/Project_Pulse/Enemies/PcQEnemyBase.h"

APcQPlayerCharacter::APcQPlayerCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UPcQPlayerMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	PrimaryActorTick.bCanEverTick = true;
	MoveComp = Cast<UPcQPlayerMovementComponent>(GetCharacterMovement());

	CameraComp = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	CameraComp->SetupAttachment(GetCapsuleComponent());
	CameraComp->SetRelativeLocation(FVector(0.f, 0.f, 60.f));
	CameraComp->bUsePawnControlRotation = true;

	WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
	WeaponMesh->SetupAttachment(CameraComp);
	WeaponMesh->SetRelativeLocation(BaseWeaponLocation);
	WeaponMesh->SetRelativeRotation(BaseWeaponRotation);
	WeaponMesh->CastShadow = false;

	HealthComp = CreateDefaultSubobject<UPcQHealthComponent>(TEXT("HealthComp"));
	bUseControllerRotationYaw = true;
}

void APcQPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();
	DefaultFOV     = CameraComp ? CameraComp->FieldOfView             : 90.f;
	DefaultCameraZ = CameraComp ? CameraComp->GetRelativeLocation().Z : 60.f;

	if (MoveComp)
	{
		MoveComp->OnActiveBeatAction.AddDynamic(this, &APcQPlayerCharacter::OnActiveBeatAction_Handler);
		// Auto-inject the config into the movement component so you only set it once!
		if (PlayerConfig) MoveComp->MoveConfig = PlayerConfig;
	}

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->bShowMouseCursor = false;
		PC->SetInputMode(FInputModeGameOnly());
		
		if (UEnhancedInputLocalPlayerSubsystem* Sub = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			if (PlayerConfig && PlayerConfig->DefaultMappingContext)
			{
				Sub->AddMappingContext(PlayerConfig->DefaultMappingContext, 0);
			}
		}
	}

	if (UPcMusicAnalysisSubsystem* Sub = GetWorld()->GetSubsystem<UPcMusicAnalysisSubsystem>())
		Sub->OnGameplayBeatTriggered.AddDynamic(this, &APcQPlayerCharacter::OnGameplayBeat);
}

void APcQPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (PlayerConfig)
		{
			if (PlayerConfig->IA_Move)        EIC->BindAction(PlayerConfig->IA_Move,        ETriggerEvent::Triggered, this, &APcQPlayerCharacter::Input_Move);
			if (PlayerConfig->IA_Look)        EIC->BindAction(PlayerConfig->IA_Look,        ETriggerEvent::Triggered, this, &APcQPlayerCharacter::Input_Look);
			
			if (PlayerConfig->IA_Jump)
			{
				EIC->BindAction(PlayerConfig->IA_Jump, ETriggerEvent::Started,   this, &APcQPlayerCharacter::Input_JumpPressed);
				EIC->BindAction(PlayerConfig->IA_Jump, ETriggerEvent::Completed, this, &APcQPlayerCharacter::Input_JumpReleased);
			}
			
			if (PlayerConfig->IA_GroundPound) EIC->BindAction(PlayerConfig->IA_GroundPound, ETriggerEvent::Started,   this, &APcQPlayerCharacter::Input_GroundPound);
			if (PlayerConfig->IA_Dash)        EIC->BindAction(PlayerConfig->IA_Dash,        ETriggerEvent::Started,   this, &APcQPlayerCharacter::Input_Dash);
			if (PlayerConfig->IA_AirHop)      EIC->BindAction(PlayerConfig->IA_AirHop,      ETriggerEvent::Started,   this, &APcQPlayerCharacter::Input_AirHop);
			if (PlayerConfig->IA_Fire)        EIC->BindAction(PlayerConfig->IA_Fire,        ETriggerEvent::Started,   this, &APcQPlayerCharacter::Input_Fire);
		}
	}
}

// =============================================================================
//  INPUT
// =============================================================================

void APcQPlayerCharacter::Input_Move(const FInputActionValue& Value)
{
	if (!Controller) return;
	const FVector2D MoveVec = Value.Get<FVector2D>();
	const FRotator  Yaw(0.f, Controller->GetControlRotation().Yaw, 0.f);
	AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::X), MoveVec.Y);
	AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y), MoveVec.X);
}

void APcQPlayerCharacter::Input_Look(const FInputActionValue& Value)
{
	if (!Controller) return;
	FVector2D LookInput = Value.Get<FVector2D>();
	
	CurrentLookDelta = LookInput;

	float SensX = PlayerConfig ? PlayerConfig->LookSensitivityX : 0.4f;
	float SensY = PlayerConfig ? PlayerConfig->LookSensitivityY : 0.4f;

	AddControllerYawInput(LookInput.X * SensX);
	AddControllerPitchInput(LookInput.Y * SensY);
}

void APcQPlayerCharacter::Input_JumpPressed()  { if (MoveComp) MoveComp->OnJumpPressed(); }
void APcQPlayerCharacter::Input_JumpReleased() { if (MoveComp) MoveComp->OnJumpReleased(); }
void APcQPlayerCharacter::Input_GroundPound()  { if (MoveComp) MoveComp->OnGroundPoundPressed(); }
void APcQPlayerCharacter::Input_Dash()         { if (MoveComp) MoveComp->OnDashPressed(); }
void APcQPlayerCharacter::Input_AirHop()       { if (MoveComp) MoveComp->OnAirHopPressed(); }

// =============================================================================
//  BEAT
// =============================================================================

void APcQPlayerCharacter::OnGameplayBeat(float)
{
	BeatFOVOffset = CameraBeatPunch;
	if (MoveComp) MoveComp->TriggerBeatJump();
}

void APcQPlayerCharacter::OnActiveBeatAction_Handler() {}

// =============================================================================
//  TICK
// =============================================================================

void APcQPlayerCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UpdateCameraEffects(DeltaTime);
	UpdateWeaponSway(DeltaTime);
}

void APcQPlayerCharacter::UpdateCameraEffects(float DeltaTime)
{
	if (!CameraComp || !MoveComp) return;

	BeatFOVOffset = FMath::FInterpTo(BeatFOVOffset, 0.f, DeltaTime, 12.f);

	const float Target = MoveComp->IsPowerBoosting() ? 1.f : 0.f;
	CurrentBoostAlpha  = FMath::FInterpTo(CurrentBoostAlpha, Target, DeltaTime, BoostCameraSpeed);

	CameraComp->SetRelativeLocation(FVector(0.f, 0.f,
		FMath::Lerp(DefaultCameraZ, DefaultCameraZ - BoostCameraDropZ, CurrentBoostAlpha)));

	const float BaseFOV = FMath::Lerp(DefaultFOV, DefaultFOV + BoostFOVGain, CurrentBoostAlpha);
	CameraComp->SetFieldOfView(BaseFOV - BeatFOVOffset);
}

void APcQPlayerCharacter::UpdateWeaponSway(float DeltaTime)
{
	if (!WeaponMesh) return;

	CurrentLookDelta = FMath::Vector2DInterpTo(CurrentLookDelta, FVector2D::ZeroVector, DeltaTime, 15.f);

	FRotator TargetRotSway = FRotator(
		CurrentLookDelta.Y * SwayRotMultiplier, 
		CurrentLookDelta.X * SwayRotMultiplier, 
		CurrentLookDelta.X * -0.7f 
	);

	FVector LocalVel = GetActorRotation().UnrotateVector(GetVelocity());
	
	// CLAMP vertical velocity contribution so we don't swing wildly on falling or jumping
	float SwayVelZ = FMath::Clamp(LocalVel.Z, -600.f, 600.f);
	
	FVector TargetLocSway = FVector(
		LocalVel.X * -0.003f, 
		LocalVel.Y * -0.003f, 
		SwayVelZ *  0.004f 
	);

	TargetLocSway.X = FMath::Clamp(TargetLocSway.X, -8.f, 5.f);
	TargetLocSway.Y = FMath::Clamp(TargetLocSway.Y, -5.f, 5.f);
	TargetLocSway.Z = FMath::Clamp(TargetLocSway.Z, -6.f, 6.f);

	if (BeatFOVOffset > 0.1f) TargetLocSway.Z -= 1.5f; 

	CurrentRecoilRot = FMath::RInterpTo(CurrentRecoilRot, FRotator::ZeroRotator, DeltaTime, RecoilRecoverySpeed);
	CurrentRecoilLoc = FMath::VInterpTo(CurrentRecoilLoc, FVector::ZeroVector, DeltaTime, RecoilRecoverySpeed);

	CurrentSwayRot = FMath::RInterpTo(CurrentSwayRot, TargetRotSway, DeltaTime, SwaySmoothness);
	CurrentSwayLoc = FMath::VInterpTo(CurrentSwayLoc, TargetLocSway, DeltaTime, SwaySmoothness);

	WeaponMesh->SetRelativeRotation(BaseWeaponRotation + CurrentSwayRot + CurrentRecoilRot);
	WeaponMesh->SetRelativeLocation(BaseWeaponLocation + CurrentSwayLoc + CurrentRecoilLoc);
}

// =============================================================================
//  COMBAT
// =============================================================================

bool APcQPlayerCharacter::IsOnBeat() const
{
	UPcMusicAnalysisSubsystem* Sub = GetWorld()->GetSubsystem<UPcMusicAnalysisSubsystem>();
	if (!Sub || !Sub->IsReadyForPlayback()) return false;

	const int32 CurrentTime = Sub->GetCurrentPlaybackTimeMS();
	const int32 NextBeat    = Sub->GetNextGameplayBeatTimeMS();
	const int32 Interval    = FMath::RoundToInt(Sub->GetGameplayBeatIntervalMS());
	const int32 PrevBeat    = NextBeat - Interval;
	
	const float WinFrac = PlayerConfig ? PlayerConfig->OnBeatWindowFraction : 0.20f;
	const int32 Window = FMath::RoundToInt(Interval * WinFrac);

	return FMath::Min(FMath::Abs(NextBeat - CurrentTime), FMath::Abs(CurrentTime - PrevBeat)) <= Window;
}

void APcQPlayerCharacter::Input_Fire() { TryFire(); }

void APcQPlayerCharacter::TryFire()
{
	if (!CameraComp) return;
	if (MoveComp && MoveComp->IsWallSwimming()) return;

	const bool bOnBeat = IsOnBeat();
	const FVector CamLoc     = CameraComp->GetComponentLocation();
	const FVector CamForward = CameraComp->GetForwardVector();

	// Calculate the correct visual socket location so the debug beam comes exactly from the barrel
	FVector VisualMuzzleLoc = WeaponMesh->GetSocketLocation(FName("Muzzle"));
	if (VisualMuzzleLoc == WeaponMesh->GetComponentLocation()) {
		VisualMuzzleLoc += WeaponMesh->GetForwardVector() * 45.f; // Fallback distance if socket doesn't exist
	}

	if (bOnBeat)
	{
		if (MoveComp) MoveComp->NotifyGunFired(); 

		CurrentRecoilRot += FRotator(12.f, FMath::RandRange(-2.f, 2.f), FMath::RandRange(-5.f, 5.f));
		CurrentRecoilLoc += FVector(-18.f, 0.f, 6.f);

		TArray<FOverlapResult> Overlaps;
		FCollisionQueryParams QP; QP.AddIgnoredActor(this);
		
		GetWorld()->OverlapMultiByChannel(Overlaps, CamLoc, FQuat::Identity, ECC_Pawn,
		                                  FCollisionShape::MakeSphere(5000.f), QP);

		APcQEnemyBase* Best = nullptr;
		float BestDot = 0.85f; 
		for (const FOverlapResult& O : Overlaps)
		{
			if (APcQEnemyBase* E = Cast<APcQEnemyBase>(O.GetActor()))
			{
				const float D = FVector::DotProduct(CamForward, (E->GetActorLocation() - CamLoc).GetSafeNormal());
				if (D > BestDot) { BestDot = D; Best = E; }
			}
		}

		FVector BeamEnd = CamLoc + CamForward * 5000.f;
		if (Best)
		{
			UGameplayStatics::ApplyDamage(Best, BaseDamage * 3.f, GetController(), this, nullptr);
			BeamEnd = Best->GetActorLocation();
		}
		
		DrawDebugLine(GetWorld(), VisualMuzzleLoc, BeamEnd, FColor::Cyan, false, 0.4f, 0, 15.f);
		if (MoveComp) MoveComp->OnComboEvent.Broadcast(TEXT("POWER SHOT ★"), FLinearColor(0.2f, 1.f, 1.f));
	}
	else
	{
		CurrentRecoilRot += FRotator(2.5f, FMath::RandRange(-0.5f, 0.5f), FMath::RandRange(-1.f, 1.f));
		CurrentRecoilLoc += FVector(-5.f, 0.f, 2.f);

		FHitResult Hit;
		FCollisionQueryParams QP2; QP2.AddIgnoredActor(this);
		const FVector End = CamLoc + CamForward * 5000.f;
		
		if (GetWorld()->LineTraceSingleByChannel(Hit, CamLoc, End, ECC_Visibility, QP2))
		{
			UGameplayStatics::ApplyDamage(Hit.GetActor(), BaseDamage, GetController(), this, nullptr);
			DrawDebugLine(GetWorld(), VisualMuzzleLoc, Hit.ImpactPoint, FColor::Red, false, 0.1f, 0, 2.f);
		}
		else 
		{
			DrawDebugLine(GetWorld(), VisualMuzzleLoc, End, FColor::Red, false, 0.1f, 0, 2.f);
		}
	}
}