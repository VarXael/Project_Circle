#include "PcQPlayerCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Project_Circle/MusicSystem/MusicGameplaySystem/PcMusicAnalysisSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "Project_Circle/Project_Pulse/Enemies/PcQEnemyBase.h"

// =============================================================================
//  CONSTRUCTION
// =============================================================================

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

// =============================================================================
//  BEGIN PLAY
// =============================================================================

void APcQPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	DefaultFOV     = CameraComp ? CameraComp->FieldOfView             : 90.f;
	DefaultCameraZ = CameraComp ? CameraComp->GetRelativeLocation().Z : 60.f;

	// Lock cursor and set input mode
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->bShowMouseCursor = false;
		PC->SetInputMode(FInputModeGameOnly());

		if (UEnhancedInputLocalPlayerSubsystem* Sub = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
			if (DefaultMappingContext)
				Sub->AddMappingContext(DefaultMappingContext, 0);
	}

	// Subscribe to gameplay beat for pulse + camera feedback
	if (UPcMusicAnalysisSubsystem* Sub = GetWorld()->GetSubsystem<UPcMusicAnalysisSubsystem>())
		Sub->OnGameplayBeatTriggered.AddDynamic(this, &APcQPlayerCharacter::OnGameplayBeat);
}

// =============================================================================
//  INPUT SETUP
// =============================================================================

void APcQPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (IA_Move)
			EIC->BindAction(IA_Move, ETriggerEvent::Triggered, this, &APcQPlayerCharacter::Input_Move);
		if (IA_Look)
			EIC->BindAction(IA_Look, ETriggerEvent::Triggered, this, &APcQPlayerCharacter::Input_Look);
		if (IA_Jump)
		{
			EIC->BindAction(IA_Jump, ETriggerEvent::Started,   this, &APcQPlayerCharacter::Input_JumpPressed);
			EIC->BindAction(IA_Jump, ETriggerEvent::Completed, this, &APcQPlayerCharacter::Input_JumpReleased);
		}
		if (IA_GroundPound)
			EIC->BindAction(IA_GroundPound, ETriggerEvent::Started, this, &APcQPlayerCharacter::Input_GroundPound);
		if (IA_Fire)
			EIC->BindAction(IA_Fire, ETriggerEvent::Started, this, &APcQPlayerCharacter::Input_Fire);
	}
}

// =============================================================================
//  INPUT HANDLERS
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

// =============================================================================
//  BEAT CALLBACK
// =============================================================================

void APcQPlayerCharacter::OnGameplayBeat(float /*BeatTimestamp*/)
{
	// Camera FOV dip — implicit rhythm feedback for the player.
	BeatFOVOffset = CameraBeatPunch;

	// Forward pulse to movement system.
	if (MoveComp) MoveComp->TriggerGroundPulse();
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

	// Beat FOV punch decays quickly
	BeatFOVOffset = FMath::FInterpTo(BeatFOVOffset, 0.f, DeltaTime, 12.f);

	// Slide camera effect (replaces old boost effect)
	const float SlideTarget = MoveComp->IsDashing() ? 1.f : 0.f;
	CurrentSlideAlpha = FMath::FInterpTo(CurrentSlideAlpha, SlideTarget, DeltaTime, SlideCameraSpeed);

	CameraComp->SetRelativeLocation(FVector(0.f, 0.f,
		FMath::Lerp(DefaultCameraZ, DefaultCameraZ - SlideCameraDropZ, CurrentSlideAlpha)));

	const float BaseFOV = FMath::Lerp(DefaultFOV, DefaultFOV + SlideFOVGain, CurrentSlideAlpha);
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

	const int32 Now      = Sub->GetCurrentPlaybackTimeMS();
	const int32 Next     = Sub->GetNextGameplayBeatTimeMS();
	const int32 Interval = FMath::RoundToInt(Sub->GetGameplayBeatIntervalMS());
	const int32 Prev     = Next - Interval;
	const int32 Window   = MoveComp ? MoveComp->GetOnBeatWindowMs() : 160;

	return FMath::Min(FMath::Abs(Next - Now), FMath::Abs(Now - Prev)) <= Window;
}

void APcQPlayerCharacter::Input_Fire() { TryFire(); }

void APcQPlayerCharacter::TryFire()
{
	if (!CameraComp)           return;
	if (PistolCooldown > 0.f)  return;

	PistolCooldown = PistolBaseCooldownSec;

	const FVector CamLoc     = CameraComp->GetComponentLocation();
	const FVector CamForward = CameraComp->GetForwardVector();
	const FVector End        = CamLoc + CamForward * 5000.f;

	// Standard line trace — no rhythm gating, no auto-aim.
	FHitResult Hit;
	FCollisionQueryParams QP; QP.AddIgnoredActor(this);

	if (GetWorld()->LineTraceSingleByChannel(Hit, CamLoc, End, ECC_Visibility, QP))
	{
		const bool bOnBeat = IsOnBeat(); // used only for resource refunds, not to gate fire

		UGameplayStatics::ApplyDamage(Hit.GetActor(), BaseDamage, GetController(), this, nullptr);

		// Notify movement component so it can refund DJ / slide gauge as designed.
		if (MoveComp) MoveComp->NotifyGunFired(bOnBeat);

		DrawDebugLine(GetWorld(), CamLoc, Hit.ImpactPoint,
		              bOnBeat ? FColor::Yellow : FColor::Red, false, 0.2f, 0, 1.f);
	}
	else
	{
		DrawDebugLine(GetWorld(), CamLoc, End, FColor::Red, false, 0.2f, 0, 1.f);
	}
}