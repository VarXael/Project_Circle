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
	DefaultCameraZ = CameraComp ? CameraComp->GetRelativeLocation().Z : 60.f;

	if (MoveComp)
		MoveComp->OnActiveBeatAction.AddDynamic(this, &APcQPlayerCharacter::OnActiveBeatAction_Handler);

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
		if (IA_Move)       EIC->BindAction(IA_Move,       ETriggerEvent::Triggered, this, &APcQPlayerCharacter::Input_Move);
		if (IA_Look)       EIC->BindAction(IA_Look,       ETriggerEvent::Triggered, this, &APcQPlayerCharacter::Input_Look);
		if (IA_Jump)
		{
			EIC->BindAction(IA_Jump, ETriggerEvent::Started,   this, &APcQPlayerCharacter::Input_JumpPressed);
			EIC->BindAction(IA_Jump, ETriggerEvent::Completed, this, &APcQPlayerCharacter::Input_JumpReleased);
		}
		if (IA_GroundPound) EIC->BindAction(IA_GroundPound, ETriggerEvent::Started, this, &APcQPlayerCharacter::Input_GroundPound);
		if (IA_Snap)        EIC->BindAction(IA_Snap,        ETriggerEvent::Started, this, &APcQPlayerCharacter::Input_Snap);
		if (IA_Fire)        EIC->BindAction(IA_Fire,        ETriggerEvent::Started, this, &APcQPlayerCharacter::Input_Fire);
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
	AddControllerYawInput(Value.Get<FVector2D>().X  * LookSensitivityX);
	AddControllerPitchInput(Value.Get<FVector2D>().Y * LookSensitivityY);
}

void APcQPlayerCharacter::Input_JumpPressed()  { if (MoveComp) MoveComp->OnJumpPressed(); }
void APcQPlayerCharacter::Input_JumpReleased() { if (MoveComp) MoveComp->OnJumpReleased(); }
void APcQPlayerCharacter::Input_GroundPound()  { if (MoveComp) MoveComp->OnGroundPoundPressed(); }
void APcQPlayerCharacter::Input_Snap()         { if (MoveComp) MoveComp->OnSnapPressed(); }

// =============================================================================
//  BEAT
// =============================================================================

void APcQPlayerCharacter::OnGameplayBeat(float)
{
	// Camera physically thuds on every beat — implicit rhythm feedback.
	BeatFOVOffset = CameraBeatPunch;
	if (MoveComp) MoveComp->TriggerBeatJump();

	// S tier: inject a grounded dash boost on every beat
	if (MoveComp && MoveComp->IsSTierActive())
		MoveComp->TriggerFrenzyDashBoost();
}

void APcQPlayerCharacter::OnActiveBeatAction_Handler()
{
	PistolCooldown = 0.f;
}

// =============================================================================
//  TICK
// =============================================================================

void APcQPlayerCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UpdateCameraEffects(DeltaTime);
	if (PistolCooldown > 0.f) PistolCooldown = FMath::Max(0.f, PistolCooldown - DeltaTime);
}

void APcQPlayerCharacter::UpdateCameraEffects(float DeltaTime)
{
	if (!CameraComp || !MoveComp) return;

	BeatFOVOffset      = FMath::FInterpTo(BeatFOVOffset, 0.f, DeltaTime, 12.f);

	const float Target = MoveComp->IsPowerBoosting() ? 1.f : 0.f;
	CurrentBoostAlpha  = FMath::FInterpTo(CurrentBoostAlpha, Target, DeltaTime, BoostCameraSpeed);

	CameraComp->SetRelativeLocation(FVector(0.f, 0.f,
		FMath::Lerp(DefaultCameraZ, DefaultCameraZ - BoostCameraDropZ, CurrentBoostAlpha)));

	const float BaseFOV = FMath::Lerp(DefaultFOV, DefaultFOV + BoostFOVGain, CurrentBoostAlpha);
	CameraComp->SetFieldOfView(BaseFOV - BeatFOVOffset);
}

// =============================================================================
//  COMBAT
// =============================================================================

float APcQPlayerCharacter::GetPistolCooldownAlpha() const
{
	return PistolCooldown <= 0.f
		? 0.f
		: FMath::Clamp(PistolCooldown / FMath::Max(PistolBaseCooldownSec, 0.01f), 0.f, 1.f);
}

bool APcQPlayerCharacter::IsOnBeat() const
{
	UPcMusicAnalysisSubsystem* Sub = GetWorld()->GetSubsystem<UPcMusicAnalysisSubsystem>();
	if (!Sub || !Sub->IsReadyForPlayback()) return false;

	const int32 CurrentTime = Sub->GetCurrentPlaybackTimeMS();
	const int32 NextBeat    = Sub->GetNextGameplayBeatTimeMS();
	const int32 Interval    = FMath::RoundToInt(Sub->GetGameplayBeatIntervalMS());
	const int32 PrevBeat    = NextBeat - Interval;
	const int32 Window      = MoveComp ? MoveComp->GetOnBeatWindowMs() : 160;

	return FMath::Min(FMath::Abs(NextBeat - CurrentTime), FMath::Abs(CurrentTime - PrevBeat)) <= Window;
}

void APcQPlayerCharacter::Input_Fire() { TryFire(); }

void APcQPlayerCharacter::TryFire()
{
	if (!CameraComp) return;
	if (MoveComp && MoveComp->IsWallSwimming()) return;

	const bool bOnBeat = IsOnBeat();
	// Off-beat with cooldown → blocked. On-beat always fires (one free shot per beat).
	if (PistolCooldown > 0.f && !bOnBeat) return;

	const FVector CamLoc     = CameraComp->GetComponentLocation();
	const FVector CamForward = CameraComp->GetForwardVector();

	if (bOnBeat)
	{
		if (MoveComp) MoveComp->NotifyGunFired(); // triggers TriggerOnBeatFlash with per-beat gate

		// If the gate granted a free shot this beat, CD = 0 (one more shot available).
		// If the gate was already used (second on-beat fire in same window), apply normal CD.
		UPcMusicAnalysisSubsystem* Sub = GetWorld()->GetSubsystem<UPcMusicAnalysisSubsystem>();
		const bool bGotFreeShot = MoveComp && Sub && Sub->IsReadyForPlayback() &&
		                          (MoveComp->GetLastBeatGrantTimestampMS() == Sub->GetNextGameplayBeatTimeMS());
		PistolCooldown = bGotFreeShot ? 0.f : MoveComp->GetBeatSnappedDuration(PistolBaseCooldownSec);

		TArray<FOverlapResult> Overlaps;
		FCollisionQueryParams QP; QP.AddIgnoredActor(this);
		GetWorld()->OverlapMultiByChannel(Overlaps, CamLoc, FQuat::Identity, ECC_Pawn,
		                                  FCollisionShape::MakeSphere(5000.f), QP);

		APcQEnemyBase* Best = nullptr;
		float BestDot = 0.90f;
		for (const FOverlapResult& O : Overlaps)
		{
			if (APcQEnemyBase* E = Cast<APcQEnemyBase>(O.GetActor()))
			{
				const float D = FVector::DotProduct(CamForward,
				                    (E->GetActorLocation() - CamLoc).GetSafeNormal());
				if (D > BestDot) { BestDot = D; Best = E; }
			}
		}

		FVector BeamEnd = CamLoc + CamForward * 5000.f;
		if (Best)
		{
			UGameplayStatics::ApplyDamage(Best, BaseDamage, GetController(), this, nullptr);
			BeamEnd = Best->GetActorLocation();
		}
		DrawDebugLine(GetWorld(), CamLoc, BeamEnd, FColor::Cyan, false, 0.5f, 0, 5.f);
		if (MoveComp) MoveComp->OnComboEvent.Broadcast(TEXT("SHOT + CD RESET"), FLinearColor(1.f, 0.35f, 1.f));
		return;
	}

	// Off-beat shot — applies cooldown, line trace only.
	UPcMusicAnalysisSubsystem* Sub = GetWorld()->GetSubsystem<UPcMusicAnalysisSubsystem>();
	PistolCooldown = (Sub && Sub->IsReadyForPlayback() && MoveComp)
	               ? MoveComp->GetBeatSnappedDuration(PistolBaseCooldownSec)
	               : PistolBaseCooldownSec;
	if (MoveComp) MoveComp->OnComboEvent.Broadcast(TEXT("SHOT FIRED"), FLinearColor(1.f, 0.25f, 0.25f));

	FHitResult Hit;
	FCollisionQueryParams QP2; QP2.AddIgnoredActor(this);
	const FVector End = CamLoc + CamForward * 5000.f;
	if (GetWorld()->LineTraceSingleByChannel(Hit, CamLoc, End, ECC_Visibility, QP2))
	{
		UGameplayStatics::ApplyDamage(Hit.GetActor(), BaseDamage, GetController(), this, nullptr);
		DrawDebugLine(GetWorld(), CamLoc, Hit.ImpactPoint, FColor::Red, false, 0.2f, 0, 1.f);
	}
	else DrawDebugLine(GetWorld(), CamLoc, End, FColor::Red, false, 0.2f, 0, 1.f);
}