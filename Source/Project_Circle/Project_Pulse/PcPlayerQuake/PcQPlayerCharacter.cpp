#include "PcQPlayerCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
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

	CurrentAmmo = MaxAmmo;

	if (MoveComp)
	{
		// Listen for the floor pulse to refund our ammo!
		MoveComp->OnGroundPulseHit.AddDynamic(this, &APcQPlayerCharacter::HandleGroundPulseHit);
	}

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
		if (IA_Move) EIC->BindAction(IA_Move, ETriggerEvent::Triggered, this, &APcQPlayerCharacter::Input_Move);
		if (IA_Look) EIC->BindAction(IA_Look, ETriggerEvent::Triggered, this, &APcQPlayerCharacter::Input_Look);
		if (IA_Jump) {
			EIC->BindAction(IA_Jump, ETriggerEvent::Started,   this, &APcQPlayerCharacter::Input_JumpPressed);
			EIC->BindAction(IA_Jump, ETriggerEvent::Completed, this, &APcQPlayerCharacter::Input_JumpReleased);
		}
		if (IA_GroundPound) EIC->BindAction(IA_GroundPound, ETriggerEvent::Started, this, &APcQPlayerCharacter::Input_GroundPound);
		if (IA_Fire) EIC->BindAction(IA_Fire, ETriggerEvent::Started, this, &APcQPlayerCharacter::Input_Fire);
	}
}

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
	AddControllerYawInput(LookInput.X * LookSensitivityX);
	AddControllerPitchInput(LookInput.Y * LookSensitivityY);
}

void APcQPlayerCharacter::Input_JumpPressed()  { if (MoveComp) MoveComp->OnJumpPressed(); }
void APcQPlayerCharacter::Input_JumpReleased() { if (MoveComp) MoveComp->OnJumpReleased(); }
void APcQPlayerCharacter::Input_GroundPound()  { if (MoveComp) MoveComp->OnGroundPoundPressed(); }

void APcQPlayerCharacter::OnGameplayBeat(float)
{
	BeatFOVOffset = CameraBeatPunch;
	if (MoveComp) MoveComp->TriggerGroundPulse();
}

void APcQPlayerCharacter::HandleGroundPulseHit()
{
	// You caught a floor pulse! Instant ammo refund!
	if (CurrentAmmo < MaxAmmo || bIsReloading)
	{
		CurrentAmmo = MaxAmmo;
		bIsReloading = false;
		ReloadTimer = 0.f;
		if (MoveComp) MoveComp->OnComboEvent.Broadcast(TEXT("PULSE AMMO REFUND"), FLinearColor(0.2f, 1.f, 0.8f));
	}
}

void APcQPlayerCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UpdateCameraEffects(DeltaTime);
	UpdateWeaponSway(DeltaTime);
	if (PistolCooldown > 0.f) PistolCooldown = FMath::Max(0.f, PistolCooldown - DeltaTime);

	// Handle Manual Reload
	if (bIsReloading)
	{
		ReloadTimer -= DeltaTime;
		if (ReloadTimer <= 0.f)
		{
			CurrentAmmo = MaxAmmo;
			bIsReloading = false;
			if (MoveComp) MoveComp->OnComboEvent.Broadcast(TEXT("RELOADED"), FLinearColor(0.5f, 0.5f, 0.5f));
		}
	}
}

void APcQPlayerCharacter::UpdateCameraEffects(float DeltaTime)
{
	if (!CameraComp || !MoveComp) return;
	BeatFOVOffset = FMath::FInterpTo(BeatFOVOffset, 0.f, DeltaTime, 12.f);
	const float SlideTarget = MoveComp->IsDashing() ? 1.f : 0.f;
	CurrentSlideAlpha = FMath::FInterpTo(CurrentSlideAlpha, SlideTarget, DeltaTime, SlideCameraSpeed);
	CameraComp->SetRelativeLocation(FVector(0.f, 0.f, FMath::Lerp(DefaultCameraZ, DefaultCameraZ - SlideCameraDropZ, CurrentSlideAlpha)));
	const float BaseFOV = FMath::Lerp(DefaultFOV, DefaultFOV + SlideFOVGain, CurrentSlideAlpha);
	CameraComp->SetFieldOfView(BaseFOV - BeatFOVOffset);
}

void APcQPlayerCharacter::UpdateWeaponSway(float DeltaTime)
{
	if (!WeaponMesh) return;
	CurrentLookDelta = FMath::Vector2DInterpTo(CurrentLookDelta, FVector2D::ZeroVector, DeltaTime, 15.f);
	
	FRotator TargetRotSway = FRotator(CurrentLookDelta.Y * SwayRotMultiplier, CurrentLookDelta.X * SwayRotMultiplier, CurrentLookDelta.X * -0.7f);
	FVector LocalVel = GetActorRotation().UnrotateVector(GetVelocity());
	float SwayVelZ = FMath::Clamp(LocalVel.Z, -600.f, 600.f);
	FVector TargetLocSway = FVector(LocalVel.X * -0.003f, LocalVel.Y * -0.003f, SwayVelZ * 0.004f);

	TargetLocSway.X = FMath::Clamp(TargetLocSway.X, -8.f, 5.f);
	TargetLocSway.Y = FMath::Clamp(TargetLocSway.Y, -5.f, 5.f);
	TargetLocSway.Z = FMath::Clamp(TargetLocSway.Z, -6.f, 6.f);

	if (BeatFOVOffset > 0.1f) TargetLocSway.Z -= 1.5f; 

	// Add a little drop effect if we are currently reloading
	if (bIsReloading)
	{
		TargetLocSway.Z -= 10.f;
		TargetRotSway.Pitch -= 20.f;
	}

	CurrentRecoilRot = FMath::RInterpTo(CurrentRecoilRot, FRotator::ZeroRotator, DeltaTime, RecoilRecoverySpeed);
	CurrentRecoilLoc = FMath::VInterpTo(CurrentRecoilLoc, FVector::ZeroVector, DeltaTime, RecoilRecoverySpeed);
	CurrentSwayRot = FMath::RInterpTo(CurrentSwayRot, TargetRotSway, DeltaTime, SwaySmoothness);
	CurrentSwayLoc = FMath::VInterpTo(CurrentSwayLoc, TargetLocSway, DeltaTime, SwaySmoothness);

	WeaponMesh->SetRelativeRotation(BaseWeaponRotation + CurrentSwayRot + CurrentRecoilRot);
	WeaponMesh->SetRelativeLocation(BaseWeaponLocation + CurrentSwayLoc + CurrentRecoilLoc);
}

float APcQPlayerCharacter::GetPistolCooldownAlpha() const {
	return PistolCooldown <= 0.f ? 0.f : FMath::Clamp(PistolCooldown / FMath::Max(PistolBaseCooldownSec, 0.01f), 0.f, 1.f);
}

bool APcQPlayerCharacter::IsOnBeat() const
{
	UPcMusicAnalysisSubsystem* Sub = GetWorld()->GetSubsystem<UPcMusicAnalysisSubsystem>();
	if (!Sub || !Sub->IsReadyForPlayback()) return false;
	const int32 Now = Sub->GetCurrentPlaybackTimeMS();
	const int32 Next = Sub->GetNextGameplayBeatTimeMS();
	const int32 Interval = FMath::RoundToInt(Sub->GetGameplayBeatIntervalMS());
	const int32 Prev = Next - Interval;
	const int32 Window = MoveComp ? MoveComp->GetOnBeatWindowMs() : 160;
	return FMath::Min(FMath::Abs(Next - Now), FMath::Abs(Now - Prev)) <= Window;
}

void APcQPlayerCharacter::Input_Fire() { TryFire(); }

void APcQPlayerCharacter::TryFire()
{
	if (!CameraComp) return;
	if (PistolCooldown > 0.f || bIsReloading) return;

	// Check Ammo!
	if (CurrentAmmo <= 0)
	{
		bIsReloading = true;
		ReloadTimer = ReloadDuration;
		if (MoveComp) MoveComp->OnComboEvent.Broadcast(TEXT("RELOADING..."), FLinearColor(1.f, 0.2f, 0.2f));
		return;
	}

	CurrentAmmo--;
	PistolCooldown = PistolBaseCooldownSec;
	const bool bOnBeat = IsOnBeat();
	const FVector CamLoc = CameraComp->GetComponentLocation();
	const FVector CamForward = CameraComp->GetForwardVector();
	
	FVector VisualMuzzleLoc = WeaponMesh->GetSocketLocation(FName("Muzzle"));
	if (VisualMuzzleLoc == WeaponMesh->GetComponentLocation()) {
		VisualMuzzleLoc += WeaponMesh->GetForwardVector() * 45.f; 
	}

	bool bHitEnemy = false;

	if (bOnBeat)
	{
		if (MoveComp) MoveComp->NotifyGunFired(true); 

		CurrentRecoilRot += FRotator(12.f, FMath::RandRange(-2.f, 2.f), FMath::RandRange(-5.f, 5.f));
		CurrentRecoilLoc += FVector(-18.f, 0.f, 6.f);

		TArray<FOverlapResult> Overlaps;
		FCollisionQueryParams QP; QP.AddIgnoredActor(this);
		GetWorld()->OverlapMultiByChannel(Overlaps, CamLoc, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(5000.f), QP);

		APcQEnemyBase* Best = nullptr;
		float BestDot = 0.85f; 
		for (const FOverlapResult& O : Overlaps) {
			if (APcQEnemyBase* E = Cast<APcQEnemyBase>(O.GetActor())) {
				const float D = FVector::DotProduct(CamForward, (E->GetActorLocation() - CamLoc).GetSafeNormal());
				if (D > BestDot) { BestDot = D; Best = E; }
			}
		}

		FVector BeamEnd = CamLoc + CamForward * 5000.f;
		if (Best) {
			UGameplayStatics::ApplyDamage(Best, BaseDamage * 3.f, GetController(), this, nullptr); // Critical Hit!
			BeamEnd = Best->GetActorLocation();
			bHitEnemy = true;
		}
		
		DrawDebugLine(GetWorld(), VisualMuzzleLoc, BeamEnd, FColor::Cyan, false, 0.4f, 0, 15.f);
		if (MoveComp) MoveComp->OnComboEvent.Broadcast(TEXT("POWER SHOT ★"), FLinearColor(0.2f, 1.f, 1.f));
	}
	else
	{
		if (MoveComp) MoveComp->NotifyGunFired(false);

		CurrentRecoilRot += FRotator(2.5f, FMath::RandRange(-0.5f, 0.5f), FMath::RandRange(-1.f, 1.f));
		CurrentRecoilLoc += FVector(-5.f, 0.f, 2.f);

		FHitResult Hit;
		FCollisionQueryParams QP2; QP2.AddIgnoredActor(this);
		const FVector End = CamLoc + CamForward * 5000.f;
		
		if (GetWorld()->LineTraceSingleByChannel(Hit, CamLoc, End, ECC_Visibility, QP2)) {
			UGameplayStatics::ApplyDamage(Hit.GetActor(), BaseDamage, GetController(), this, nullptr);
			DrawDebugLine(GetWorld(), VisualMuzzleLoc, Hit.ImpactPoint, FColor::Red, false, 0.1f, 0, 2.f);
			
			if (Cast<APcQEnemyBase>(Hit.GetActor())) bHitEnemy = true;
		} else {
			DrawDebugLine(GetWorld(), VisualMuzzleLoc, End, FColor::Red, false, 0.1f, 0, 2.f);
		}
	}

	// ── IF WE HIT AN ENEMY, RESET MOBILITY! ──
	if (bHitEnemy && MoveComp)
	{
		MoveComp->ResetMobilityAbilities();
	}
}