#include "PcQPlayerCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Project_Circle/MusicSystem/MusicGameplaySystem/PcMusicAnalysisSubsystem.h"

APcQPlayerCharacter::APcQPlayerCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UPcQPlayerMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	MoveComp = Cast<UPcQPlayerMovementComponent>(GetCharacterMovement());
	CameraComp = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	CameraComp->SetupAttachment(GetCapsuleComponent());
	CameraComp->SetRelativeLocation(FVector(0.f, 0.f, 60.f));
	CameraComp->bUsePawnControlRotation = true;
	bUseControllerRotationYaw = true;
}

void APcQPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->bShowMouseCursor = false;
		PC->SetInputMode(FInputModeGameOnly());
		if (UEnhancedInputLocalPlayerSubsystem* Sub = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
			if (DefaultMappingContext) Sub->AddMappingContext(DefaultMappingContext, 0);
	}

	if (UPcMusicAnalysisSubsystem* Sub = GetWorld()->GetSubsystem<UPcMusicAnalysisSubsystem>()) {
		Sub->OnGameplayBeatTriggered.AddDynamic(this, &APcQPlayerCharacter::OnGameplayBeat);
		Sub->OnGameplayBPMChanged.AddDynamic(this, &APcQPlayerCharacter::OnGameplayBPMChanged);
	}
}

void APcQPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {
		if (IA_Move) EIC->BindAction(IA_Move, ETriggerEvent::Triggered, this, &APcQPlayerCharacter::Input_Move);
		if (IA_Look) EIC->BindAction(IA_Look, ETriggerEvent::Triggered, this, &APcQPlayerCharacter::Input_Look);
		if (IA_Jump) {
			EIC->BindAction(IA_Jump, ETriggerEvent::Started, this, &APcQPlayerCharacter::Input_JumpPressed);
			EIC->BindAction(IA_Jump, ETriggerEvent::Completed, this, &APcQPlayerCharacter::Input_JumpReleased);
		}
	}
}

void APcQPlayerCharacter::Input_Move(const FInputActionValue& Value) {
	if (!Controller) return;
	FVector2D MoveVec = Value.Get<FVector2D>();
	FRotator Yaw(0.f, Controller->GetControlRotation().Yaw, 0.f);
	AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::X), MoveVec.Y);
	AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y), MoveVec.X);
}

void APcQPlayerCharacter::Input_Look(const FInputActionValue& Value) {
	if (!Controller) return;
	AddControllerYawInput(Value.Get<FVector2D>().X * LookSensitivityX);
	AddControllerPitchInput(Value.Get<FVector2D>().Y * LookSensitivityY);
}

void APcQPlayerCharacter::Input_JumpPressed() { if (MoveComp) MoveComp->OnJumpPressed(); }
void APcQPlayerCharacter::Input_JumpReleased() { if (MoveComp) MoveComp->OnJumpReleased(); }

void APcQPlayerCharacter::OnGameplayBeat(float) { if (MoveComp) MoveComp->TriggerBeatJump(); }

void APcQPlayerCharacter::OnGameplayBPMChanged(float NewGameplayBPM) {
	if (MoveComp) {
		if (UPcMusicAnalysisSubsystem* Sub = GetWorld()->GetSubsystem<UPcMusicAnalysisSubsystem>())
			MoveComp->UpdateBPM(NewGameplayBPM, Sub->GetCurrentSubdivision(), Sub->GetCurrentPresetOverride());
	}
}