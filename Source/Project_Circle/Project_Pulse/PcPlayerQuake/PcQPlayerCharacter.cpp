#include "PcQPlayerCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "Project_Circle/MusicSystem/MusicGameplaySystem/PcMusicAnalysisSubsystem.h"
#include "Project_Circle/Project_Pulse/Enemies/PcQEnemyBase.h"
#include "Project_Circle/Project_Pulse/Core/PcQHealthComponent.h"

APcQPlayerCharacter::APcQPlayerCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UPcQPlayerMovementComponent>(
		ACharacter::CharacterMovementComponentName))
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

	DefaultFOV     = CameraComp ? CameraComp->FieldOfView            : 90.f;
	DefaultCameraZ = CameraComp ? CameraComp->GetRelativeLocation().Z : 60.f;

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->bShowMouseCursor = false;
		PC->SetInputMode(FInputModeGameOnly());
		if (UEnhancedInputLocalPlayerSubsystem* Sub =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
			if (DefaultMappingContext) Sub->AddMappingContext(DefaultMappingContext, 0);
	}

	// The character still subscribes to OnActiveBeatAction from the movement
	// component so it can auto-fire when a beat action succeeds.
	// This is the player OPTING IN to beat-sensitive behaviour.
	// The movement component itself is fully decoupled from the beat.
	if (MoveComp)
		MoveComp->OnActiveBeatAction.AddDynamic(this, &APcQPlayerCharacter::OnActiveBeatAction_Handler);
}

// =============================================================================
//  TICK
// =============================================================================

void APcQPlayerCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UpdateCameraEffects(DeltaTime);
}

void APcQPlayerCharacter::UpdateCameraEffects(float DeltaTime)
{
	if (!CameraComp || !MoveComp) return;

	const float SlideTarget  = MoveComp->IsSliding() ? 1.f : 0.f;
	CurrentSlideAlpha        = FMath::FInterpTo(CurrentSlideAlpha, SlideTarget, DeltaTime, SlideCameraSpeed);

	const float NewCameraZ   = FMath::Lerp(DefaultCameraZ, DefaultCameraZ - SlideCameraDropZ, CurrentSlideAlpha);
	CameraComp->SetRelativeLocation(FVector(0.f, 0.f, NewCameraZ));
	CameraComp->SetFieldOfView(FMath::Lerp(DefaultFOV, DefaultFOV - SlideFOVSqueeze, CurrentSlideAlpha));
}

// =============================================================================
//  INPUT SETUP
// =============================================================================

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
		if (IA_GroundPound) EIC->BindAction(IA_GroundPound, ETriggerEvent::Started,   this, &APcQPlayerCharacter::Input_GroundPound);
		if (IA_Slide)
		{
			EIC->BindAction(IA_Slide, ETriggerEvent::Started,   this, &APcQPlayerCharacter::Input_SlidePressed);
			EIC->BindAction(IA_Slide, ETriggerEvent::Completed, this, &APcQPlayerCharacter::Input_SlideReleased);
		}
		if (IA_Fire)        EIC->BindAction(IA_Fire, ETriggerEvent::Started, this, &APcQPlayerCharacter::Input_Fire);
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
	AddControllerYawInput(Value.Get<FVector2D>().X * LookSensitivityX);
	AddControllerPitchInput(Value.Get<FVector2D>().Y * LookSensitivityY);
}

void APcQPlayerCharacter::Input_JumpPressed()    { if (MoveComp) MoveComp->OnJumpPressed(); }
void APcQPlayerCharacter::Input_JumpReleased()   { if (MoveComp) MoveComp->OnJumpReleased(); }
void APcQPlayerCharacter::Input_GroundPound()    { if (MoveComp) MoveComp->OnGroundPoundPressed(); }
void APcQPlayerCharacter::Input_SlidePressed()   { if (MoveComp) MoveComp->OnSlidePressed(); }
void APcQPlayerCharacter::Input_SlideReleased()  { if (MoveComp) MoveComp->OnSlideReleased(); }
void APcQPlayerCharacter::Input_Fire()           { TryFire(); }

// =============================================================================
//  AUTO-FIRE on beat action
// =============================================================================

void APcQPlayerCharacter::OnActiveBeatAction_Handler()
{
	TryFire();
}

// =============================================================================
//  COMBAT
// =============================================================================

bool APcQPlayerCharacter::IsOnBeat() const
{
	// The character opts in to beat-sensitive behaviour here.
	// This is separate from the movement component which has no beat knowledge.
	UPcMusicAnalysisSubsystem* Sub = GetWorld()->GetSubsystem<UPcMusicAnalysisSubsystem>();
	if (!Sub || !Sub->IsReadyForPlayback()) return false;
	const int32 CurrentMS  = Sub->GetCurrentPlaybackTimeMS();
	const int32 NextBeatMS = Sub->GetNextGameplayBeatTimeMS();
	const int32 IntervalMS = FMath::RoundToInt(Sub->GetGameplayBeatIntervalMS());
	const int32 MinDist    = FMath::Min(FMath::Abs(NextBeatMS - CurrentMS),
	                                    FMath::Abs(CurrentMS - (NextBeatMS - IntervalMS)));
	return MinDist <= OnBeatWindowMS;
}

void APcQPlayerCharacter::TryFire()
{
	if (!CameraComp) return;
	if (MoveComp && MoveComp->IsWallSwimming()) return;

	const FVector CamLoc     = CameraComp->GetComponentLocation();
	const FVector CamForward = CameraComp->GetForwardVector();

	if (IsOnBeat())
	{
		// On-beat: generous auto-snap cone (~25°)
		TArray<AActor*> Enemies;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), APcQEnemyBase::StaticClass(), Enemies);

		APcQEnemyBase* BestEnemy = nullptr;
		float          BestDot   = 0.90f;

		for (AActor* EnemyActor : Enemies)
		{
			const FVector Dir = (EnemyActor->GetActorLocation() - CamLoc).GetSafeNormal();
			const float   Dot = FVector::DotProduct(CamForward, Dir);
			if (Dot > BestDot) { BestDot = Dot; BestEnemy = Cast<APcQEnemyBase>(EnemyActor); }
		}

		if (BestEnemy)
		{
			UGameplayStatics::ApplyDamage(BestEnemy, BaseDamage, GetController(), this, nullptr);
			DrawDebugLine(GetWorld(), CamLoc, BestEnemy->GetActorLocation(),
			              FColor::Cyan, false, 0.5f, 0, 5.f);
			UE_LOG(LogTemp, Warning, TEXT("ON-BEAT HIT!"));
			return;
		}
	}

	// Off-beat or no snap target: standard raycast
	FHitResult         Hit;
	FCollisionQueryParams Params; Params.AddIgnoredActor(this);
	const FVector EndLoc = CamLoc + CamForward * 5000.f;

	if (GetWorld()->LineTraceSingleByChannel(Hit, CamLoc, EndLoc, ECC_Visibility, Params))
	{
		UGameplayStatics::ApplyDamage(Hit.GetActor(), BaseDamage, GetController(), this, nullptr);
		DrawDebugLine(GetWorld(), CamLoc, Hit.ImpactPoint, FColor::Red, false, 0.2f, 0, 1.f);
	}
	else
	{
		DrawDebugLine(GetWorld(), CamLoc, EndLoc, FColor::Red, false, 0.2f, 0, 1.f);
	}
}