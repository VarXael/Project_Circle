#include "PcQPlayerCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
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

	HealthComp = CreateDefaultSubobject<UPcQHealthComponent>(TEXT("HealthComp"));
	bUseControllerRotationYaw = true;
}

void APcQPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();
	DefaultFOV     = CameraComp ? CameraComp->FieldOfView             : 90.f;
	DefaultCameraZ = CameraComp ? CameraComp->GetRelativeLocation().Z  : 60.f;

	if (MoveComp) MoveComp->OnActiveBeatAction.AddDynamic(this, &APcQPlayerCharacter::OnActiveBeatAction_Handler);

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->bShowMouseCursor = false;
		PC->SetInputMode(FInputModeGameOnly());
		if (UEnhancedInputLocalPlayerSubsystem* Sub = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
			if (DefaultMappingContext) Sub->AddMappingContext(DefaultMappingContext, 0);
	}

	if (UPcMusicAnalysisSubsystem* Sub = GetWorld()->GetSubsystem<UPcMusicAnalysisSubsystem>())
		Sub->OnGameplayBeatTriggered.AddDynamic(this, &APcQPlayerCharacter::OnGameplayBeat);
}

void APcQPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (IA_Move)        EIC->BindAction(IA_Move,        ETriggerEvent::Triggered,  this, &APcQPlayerCharacter::Input_Move);
		if (IA_Look)        EIC->BindAction(IA_Look,        ETriggerEvent::Triggered,  this, &APcQPlayerCharacter::Input_Look);
		if (IA_Jump)
		{
			EIC->BindAction(IA_Jump, ETriggerEvent::Started,   this, &APcQPlayerCharacter::Input_JumpPressed);
			EIC->BindAction(IA_Jump, ETriggerEvent::Completed, this, &APcQPlayerCharacter::Input_JumpReleased);
		}
		if (IA_GroundPound) EIC->BindAction(IA_GroundPound, ETriggerEvent::Started,    this, &APcQPlayerCharacter::Input_GroundPound);

		if (IA_Slide)
		{
			EIC->BindAction(IA_Slide, ETriggerEvent::Started,   this, &APcQPlayerCharacter::Input_SlidePressed);
			EIC->BindAction(IA_Slide, ETriggerEvent::Completed, this, &APcQPlayerCharacter::Input_SlideReleased);
		}
		if (IA_Fire)        EIC->BindAction(IA_Fire,        ETriggerEvent::Started,    this, &APcQPlayerCharacter::Input_Fire);
	}
}

void APcQPlayerCharacter::Input_Move(const FInputActionValue& Value)
{
	if (!Controller) return;
	FVector2D MoveVec = Value.Get<FVector2D>();
	FRotator Yaw(0.f, Controller->GetControlRotation().Yaw, 0.f);
	AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::X), MoveVec.Y);
	AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y), MoveVec.X);
}

void APcQPlayerCharacter::Input_Look(const FInputActionValue& Value)
{
	if (!Controller) return;
	AddControllerYawInput(Value.Get<FVector2D>().X * LookSensitivityX);
	AddControllerPitchInput(Value.Get<FVector2D>().Y * LookSensitivityY);
}

void APcQPlayerCharacter::Input_JumpPressed()  { if (MoveComp) MoveComp->OnJumpPressed(); }
void APcQPlayerCharacter::Input_JumpReleased() { if (MoveComp) MoveComp->OnJumpReleased(); }
void APcQPlayerCharacter::Input_GroundPound()  { if (MoveComp) MoveComp->OnGroundPoundPressed(); }

void APcQPlayerCharacter::Input_SlidePressed()
{
	if (!MoveComp) return;
	bSlideHeld      = true;
	SlideHeldTime   = 0.f;
	bSlideActivated = false;
	MoveComp->OnSlidePressed();  
}

void APcQPlayerCharacter::Input_SlideReleased()
{
	if (!MoveComp) return;
	bSlideHeld = false;
	bSlideActivated = false;
	MoveComp->OnSlideReleased();
}

void APcQPlayerCharacter::OnGameplayBeat(float)
{
	BeatFOVOffset = CameraBeatPunch;
	if (MoveComp) MoveComp->TriggerBeatJump();
}

void APcQPlayerCharacter::OnActiveBeatAction_Handler()
{
	PistolCooldown = 0.f;
}

void APcQPlayerCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UpdateCameraEffects(DeltaTime);
	if (PistolCooldown > 0.f) PistolCooldown = FMath::Max(0.f, PistolCooldown - DeltaTime);

	if (bSlideHeld && !bSlideActivated && MoveComp && MoveComp->IsMovingOnGround())
	{
		SlideHeldTime += DeltaTime;
		if (SlideHeldTime >= SlideActivateThreshold)
		{
			bSlideActivated = true;
			MoveComp->ActivateSlide(); 
		}
	}
}

void APcQPlayerCharacter::UpdateCameraEffects(float DeltaTime)
{
	if (!CameraComp || !MoveComp) return;

	BeatFOVOffset      = FMath::FInterpTo(BeatFOVOffset,     0.f, DeltaTime, 12.f);
	CurrentBoostAlpha  = FMath::FInterpTo(CurrentBoostAlpha,  MoveComp->IsPowerBoosting() ? 1.f : 0.f, DeltaTime, BoostCameraSpeed);
	CurrentAutoJumpAlpha = FMath::FInterpTo(CurrentAutoJumpAlpha, MoveComp->IsAutoJumping() ? 1.f : 0.f, DeltaTime, AutoJumpCamSpeed);

	CameraComp->SetRelativeLocation(FVector(0.f, 0.f,
		FMath::Lerp(DefaultCameraZ, DefaultCameraZ - BoostCameraDropZ, CurrentBoostAlpha)));

	float FOV = DefaultFOV;
	FOV += BoostFOVGain    * CurrentBoostAlpha;    
	FOV += AutoJumpFOVBoost * CurrentAutoJumpAlpha; 
	FOV -= BeatFOVOffset;                           
	CameraComp->SetFieldOfView(FOV);
}

float APcQPlayerCharacter::GetPistolCooldownAlpha() const
{
	return PistolCooldown <= 0.f ? 0.f : FMath::Clamp(PistolCooldown / FMath::Max(PistolBaseCooldownSec, 0.01f), 0.f, 1.f);
}

bool APcQPlayerCharacter::IsOnBeat() const
{
	UPcMusicAnalysisSubsystem* Sub = GetWorld()->GetSubsystem<UPcMusicAnalysisSubsystem>();
	if (!Sub || !Sub->IsReadyForPlayback()) return false;
	int32 Now      = Sub->GetCurrentPlaybackTimeMS();
	int32 Next     = Sub->GetNextGameplayBeatTimeMS();
	int32 Interval = FMath::RoundToInt(Sub->GetGameplayBeatIntervalMS());
	int32 Window   = MoveComp ? MoveComp->OnBeatWindowMS : 160;
	return FMath::Min(FMath::Abs(Next - Now), FMath::Abs(Now - (Next - Interval))) <= Window;
}

void APcQPlayerCharacter::Input_Fire() { TryFire(); }

void APcQPlayerCharacter::TryFire()
{
	if (!CameraComp) return;
	if (MoveComp && MoveComp->IsWallSwimming()) return;

	const bool bOnBeat = IsOnBeat();
	if (PistolCooldown > 0.f && !bOnBeat) return;

	FVector CamLoc     = CameraComp->GetComponentLocation();
	FVector CamForward = CameraComp->GetForwardVector();

	if (bOnBeat)
	{
		PistolCooldown = 0.f;
		// NotifyGunFired calls TriggerOnBeatFlash → universal reset (DJ, boost, slide pump, pistol via delegate)
		if (MoveComp) MoveComp->NotifyGunFired();
	}
	else
	{
		UPcMusicAnalysisSubsystem* Sub = GetWorld()->GetSubsystem<UPcMusicAnalysisSubsystem>();
		PistolCooldown = (Sub && Sub->IsReadyForPlayback() && MoveComp)
		               ? MoveComp->GetBeatSnappedDuration(PistolBaseCooldownSec)
		               : PistolBaseCooldownSec;
		if (MoveComp) MoveComp->OnComboEvent.Broadcast(TEXT("SHOT FIRED"), FLinearColor(1.f, 0.25f, 0.25f));
	}

	if (bOnBeat)
	{
		TArray<FOverlapResult> Overlaps;
		FCollisionShape Sphere = FCollisionShape::MakeSphere(5000.f);
		FCollisionQueryParams QP; QP.AddIgnoredActor(this);
		
		// FIXED: Uses Overlap instead of Sweep to avoid physical engine bugs with zero-length rays
		GetWorld()->OverlapMultiByChannel(Overlaps, CamLoc, FQuat::Identity, ECC_Pawn, Sphere, QP);

		APcQEnemyBase* Best = nullptr; float BestDot = 0.90f;
		for (auto& O : Overlaps)
		{
			if (APcQEnemyBase* E = Cast<APcQEnemyBase>(O.GetActor()))
			{
				float D = FVector::DotProduct(CamForward, (E->GetActorLocation() - CamLoc).GetSafeNormal());
				if (D > BestDot) { BestDot = D; Best = E; }
			}
		}
		if (Best) { 
			UGameplayStatics::ApplyDamage(Best, BaseDamage, GetController(), this, nullptr); 
			// THICK BLUE LASER OF DEATH
			DrawDebugLine(GetWorld(), CamLoc, Best->GetActorLocation(), FColor::Cyan, false, 0.8f, 0, 12.0f);
			return; 
		}
	}

	FHitResult Hit; FCollisionQueryParams QP2; QP2.AddIgnoredActor(this);
	FVector End = CamLoc + CamForward * 5000.f;
	if (GetWorld()->LineTraceSingleByChannel(Hit, CamLoc, End, ECC_Visibility, QP2))
		UGameplayStatics::ApplyDamage(Hit.GetActor(), BaseDamage, GetController(), this, nullptr);
	DrawDebugLine(GetWorld(), CamLoc, Hit.bBlockingHit ? Hit.ImpactPoint : End, FColor::Red, false, 0.2f, 0, 1.f);
}