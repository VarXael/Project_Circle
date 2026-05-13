#include "PcQPlayerCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Project_Circle/MusicSystem/MusicGameplaySystem/PcMusicAnalysisSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "Engine/DamageEvents.h"
#include "Project_Circle/Project_Pulse/Enemies/PcQEnemyBase.h"
#include "Project_Circle/Project_Pulse/Core/PcQHealthComponent.h"

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
	WeaponMesh->CastShadow = false;

	SwordMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("SwordMesh"));
	SwordMesh->SetupAttachment(CameraComp);
	SwordMesh->CastShadow = false;

	HealthComp = CreateDefaultSubobject<UPcQHealthComponent>(TEXT("HealthComp"));
	bUseControllerRotationYaw = true;
}

void APcQPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();
	DefaultFOV     = CameraComp ? CameraComp->FieldOfView             : 90.f;
	DefaultCameraZ = CameraComp ? CameraComp->GetRelativeLocation().Z : 60.f;
	
	BaseWeaponLocation = WeaponMesh->GetRelativeLocation();
	BaseWeaponRotation = WeaponMesh->GetRelativeRotation();
	BaseSwordLocation = SwordMesh->GetRelativeLocation();
	BaseSwordRotation = SwordMesh->GetRelativeRotation();

	CurrentAmmo = MaxAmmo;
	SwordState = ESwordState::InHand;

	if (MoveComp)
	{
		MoveComp->OnGroundPulseHit.AddDynamic(this, &APcQPlayerCharacter::HandleGroundPulseHit);
		MoveComp->OnMagneticSlam.AddDynamic(this, &APcQPlayerCharacter::HandleMagneticSlam);
		MoveComp->OnSwordHitEnemy.AddDynamic(this, &APcQPlayerCharacter::HandleSwordHitEnemy);
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
		if (IA_Dash) EIC->BindAction(IA_Dash, ETriggerEvent::Started, this, &APcQPlayerCharacter::Input_Dash);
		if (IA_GroundPound) EIC->BindAction(IA_GroundPound, ETriggerEvent::Started, this, &APcQPlayerCharacter::Input_GroundPound); 
		if (IA_Fire) EIC->BindAction(IA_Fire, ETriggerEvent::Started, this, &APcQPlayerCharacter::Input_Fire);
		if (IA_Melee) EIC->BindAction(IA_Melee, ETriggerEvent::Started, this, &APcQPlayerCharacter::Input_Melee);
	}
}

void APcQPlayerCharacter::Input_Move(const FInputActionValue& Value) {
	if (!Controller) return;
	const FVector2D MoveVec = Value.Get<FVector2D>();
	const FRotator  Yaw(0.f, Controller->GetControlRotation().Yaw, 0.f);
	AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::X), MoveVec.Y);
	AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y), MoveVec.X);
}

void APcQPlayerCharacter::Input_Look(const FInputActionValue& Value) {
	if (!Controller) return;
	FVector2D LookInput = Value.Get<FVector2D>();
	CurrentLookDelta = LookInput;
	AddControllerYawInput(LookInput.X * LookSensitivityX);
	AddControllerPitchInput(LookInput.Y * LookSensitivityY);
}

void APcQPlayerCharacter::Input_JumpPressed()  { if (MoveComp) MoveComp->OnJumpPressed(); }
void APcQPlayerCharacter::Input_JumpReleased() { if (MoveComp) MoveComp->OnJumpReleased(); }
void APcQPlayerCharacter::Input_GroundPound()  { if (MoveComp) MoveComp->OnGroundPoundPressed(); } 

void APcQPlayerCharacter::Input_Dash() { 
	if (MoveComp) MoveComp->EnterDash(); 
}

void APcQPlayerCharacter::Input_Fire() { TryFire(); }
void APcQPlayerCharacter::Input_Melee() { TrySwordAction(); }

void APcQPlayerCharacter::OnGameplayBeat(float)
{
	BeatFOVOffset = CameraBeatPunch;
	if (MoveComp) MoveComp->TriggerGroundPulse();
}

void APcQPlayerCharacter::HandleGroundPulseHit()
{
	if (CurrentAmmo < MaxAmmo || bIsReloading) {
		CurrentAmmo = MaxAmmo;
		bIsReloading = false;
		ReloadTimer = 0.f;
		if (MoveComp) MoveComp->OnComboEvent.Broadcast(TEXT("PULSE AMMO REFUND"), FLinearColor(0.2f, 1.f, 0.8f));
	}
}

void APcQPlayerCharacter::HandleMagneticSlam() { MagnetDipAlpha = 1.0f; }

void APcQPlayerCharacter::HandleSwordHitEnemy(APcQEnemyBase* Enemy)
{
	if (Enemy) {
		UGameplayStatics::ApplyDamage(Enemy, SwordDamage, GetController(), this, nullptr);
	}
}

void APcQPlayerCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UpdateCameraEffects(DeltaTime);
	UpdateWeaponSway(DeltaTime);
	UpdateSwordPhysics(DeltaTime);
	
	if (PistolCooldown > 0.f) PistolCooldown = FMath::Max(0.f, PistolCooldown - DeltaTime);

	if (bIsReloading) {
		ReloadTimer -= DeltaTime;
		if (ReloadTimer <= 0.f) {
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
	MagnetDipAlpha = FMath::FInterpTo(MagnetDipAlpha, 0.f, DeltaTime, 15.f);

	const float SlideTarget = MoveComp->IsDashing() && MoveComp->IsMovingOnGround() ? 1.f : 0.f;
	CurrentSlideAlpha = FMath::FInterpTo(CurrentSlideAlpha, SlideTarget, DeltaTime, SlideCameraSpeed);
	
	float CamZ = FMath::Lerp(DefaultCameraZ, DefaultCameraZ - SlideCameraDropZ, CurrentSlideAlpha) - (MagnetDipAlpha * 15.f);
	CameraComp->SetRelativeLocation(FVector(0.f, 0.f, CamZ));
	CameraComp->SetRelativeRotation(FRotator(MagnetDipAlpha * -5.f, 0.f, 0.f));

	const float BaseFOV = FMath::Lerp(DefaultFOV, DefaultFOV + SlideFOVGain, CurrentSlideAlpha);
	CameraComp->SetFieldOfView(BaseFOV - BeatFOVOffset);
}

void APcQPlayerCharacter::UpdateWeaponSway(float DeltaTime)
{
	CurrentLookDelta = FMath::Vector2DInterpTo(CurrentLookDelta, FVector2D::ZeroVector, DeltaTime, 15.f);
	FVector LocalVel = GetActorRotation().UnrotateVector(GetVelocity());
	float SwayVelZ = FMath::Clamp(LocalVel.Z, -600.f, 600.f);

	if (WeaponMesh) {
		FRotator TargetRotSway = FRotator(CurrentLookDelta.Y * SwayRotMultiplier, CurrentLookDelta.X * SwayRotMultiplier, CurrentLookDelta.X * -0.7f);
		FVector TargetLocSway = FVector(LocalVel.X * -0.003f, LocalVel.Y * -0.003f, SwayVelZ * 0.004f);
		TargetLocSway.X = FMath::Clamp(TargetLocSway.X, -8.f, 5.f);
		TargetLocSway.Y = FMath::Clamp(TargetLocSway.Y, -5.f, 5.f);
		TargetLocSway.Z = FMath::Clamp(TargetLocSway.Z, -6.f, 6.f);

		if (BeatFOVOffset > 0.1f) TargetLocSway.Z -= 1.5f; 
		if (bIsReloading) { TargetLocSway.Z -= 10.f; TargetRotSway.Pitch -= 20.f; }

		CurrentRecoilRot = FMath::RInterpTo(CurrentRecoilRot, FRotator::ZeroRotator, DeltaTime, RecoilRecoverySpeed);
		CurrentRecoilLoc = FMath::VInterpTo(CurrentRecoilLoc, FVector::ZeroVector, DeltaTime, RecoilRecoverySpeed);
		CurrentSwayRot = FMath::RInterpTo(CurrentSwayRot, TargetRotSway, DeltaTime, SwaySmoothness);
		CurrentSwayLoc = FMath::VInterpTo(CurrentSwayLoc, TargetLocSway, DeltaTime, SwaySmoothness);

		WeaponMesh->SetRelativeRotation(BaseWeaponRotation + CurrentSwayRot + CurrentRecoilRot);
		WeaponMesh->SetRelativeLocation(BaseWeaponLocation + CurrentSwayLoc + CurrentRecoilLoc);
	}

	if (SwordMesh && SwordState == ESwordState::InHand) {
		FRotator TargetSwordRotSway = FRotator(CurrentLookDelta.Y * SwayRotMultiplier, CurrentLookDelta.X * SwayRotMultiplier, CurrentLookDelta.X * 0.7f);
		FVector TargetSwordLocSway = FVector(LocalVel.X * -0.003f, LocalVel.Y * -0.003f, SwayVelZ * 0.004f);
		TargetSwordLocSway.X = FMath::Clamp(TargetSwordLocSway.X, -8.f, 5.f);
		TargetSwordLocSway.Y = FMath::Clamp(TargetSwordLocSway.Y, -5.f, 5.f);
		TargetSwordLocSway.Z = FMath::Clamp(TargetSwordLocSway.Z, -6.f, 6.f);

		if (BeatFOVOffset > 0.1f) TargetSwordLocSway.Z -= 1.5f; 

		CurrentSwordSwayRot = FMath::RInterpTo(CurrentSwordSwayRot, TargetSwordRotSway, DeltaTime, SwaySmoothness);
		CurrentSwordSwayLoc = FMath::VInterpTo(CurrentSwordSwayLoc, TargetSwordLocSway, DeltaTime, SwaySmoothness);

		SwordMesh->SetRelativeRotation(BaseSwordRotation + CurrentSwordSwayRot);
		SwordMesh->SetRelativeLocation(BaseSwordLocation + CurrentSwordSwayLoc);
	}
}

// ── FIX: TRACE FROM CAMERA CROSSHAIR FIRST ──
void APcQPlayerCharacter::TrySwordAction()
{
	if (!MoveComp || !MoveComp->Config || !CameraComp || !SwordMesh) return;

	if (SwordState == ESwordState::InHand) 
	{
		FVector CamLoc = CameraComp->GetComponentLocation();
		FVector CamForward = CameraComp->GetForwardVector();
		FVector End = CamLoc + CamForward * MoveComp->Config->SwordMaxDistance;

		FCollisionQueryParams QP; QP.AddIgnoredActor(this);
		
		// Pre-calculate where it should stick based on the Crosshair
		GetWorld()->SweepSingleByChannel(SwordTargetHit, CamLoc, End, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(20.f), QP);

		if (SwordTargetHit.bBlockingHit || SwordTargetHit.GetActor()) {
			SwordTargetLocation = SwordTargetHit.ImpactPoint;
			bSwordWillStick = true;
		} else {
			SwordTargetLocation = End;
			bSwordWillStick = false;
		}

		SwordState = ESwordState::Thrown;
		SwordMesh->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		ThrownStartPosition = SwordMesh->GetComponentLocation();
		
		// Set velocity towards the pre-calculated crosshair target
		SwordVelocity = (SwordTargetLocation - ThrownStartPosition).GetSafeNormal() * MoveComp->Config->SwordThrowSpeed;
		
		if (MoveComp) MoveComp->OnComboEvent.Broadcast(TEXT("KUNAI THROWN"), FLinearColor(0.2f, 0.8f, 1.f));
	}
	else if (SwordState == ESwordState::Stuck || SwordState == ESwordState::Returning) 
	{
		bool bStuckInEnemy = (SwordState == ESwordState::Stuck && SwordMesh->GetAttachParent() && Cast<APcQEnemyBase>(SwordMesh->GetAttachParent()->GetOwner()));
		
		// Default: Aim where the sword currently is
		FVector TargetDashPos = SwordMesh->GetComponentLocation();
		
		// FIX: If stuck in an enemy, dash to the ENEMY'S CORE instead of the sword mesh!
		if (bStuckInEnemy) {
			TargetDashPos = SwordMesh->GetAttachParent()->GetOwner()->GetActorLocation();
		}

		float Dist = FVector::Dist(GetActorLocation(), TargetDashPos);
		float PowerPercent = bStuckInEnemy ? 1.0f : FMath::Clamp(Dist / MoveComp->Config->SwordMaxDistance, 0.5f, 1.0f);

		RetrieveSword(); 

		if (bStuckInEnemy) {
			FVector LungeDir = (TargetDashPos - CameraComp->GetComponentLocation()).GetSafeNormal();
			MoveComp->ExecuteRecallDash(LungeDir, PowerPercent, Dist); 
		} else {
			FVector ImpulseDir = CameraComp->GetForwardVector();
			MoveComp->ExecuteRecallImpulse(ImpulseDir, PowerPercent); 
		}
	}
}

// ── FLIES EXACTLY TO PRE-CALCULATED TARGET ──
void APcQPlayerCharacter::UpdateSwordPhysics(float DeltaTime)
{
	if (!SwordMesh || SwordState == ESwordState::InHand || !MoveComp || !MoveComp->Config) return;

	FVector OldLoc = SwordMesh->GetComponentLocation();

	if (SwordState == ESwordState::Thrown) 
	{
		float DistToTarget = FVector::Dist(OldLoc, SwordTargetLocation);
		float MoveStep = MoveComp->Config->SwordThrowSpeed * DeltaTime;

		if (MoveStep >= DistToTarget) 
		{
			if (bSwordWillStick) 
			{
				SwordMesh->SetWorldLocation(SwordTargetLocation);
				SwordMesh->SetWorldRotation((-SwordTargetHit.ImpactNormal).Rotation());
				if (SwordTargetHit.Component.IsValid()) {
					SwordMesh->AttachToComponent(SwordTargetHit.Component.Get(), FAttachmentTransformRules::KeepWorldTransform);
				}

				SwordState = ESwordState::Stuck;
				SwordStuckTimer = MoveComp->Config->SwordStuckDurationSec;

				if (APcQEnemyBase* Enemy = Cast<APcQEnemyBase>(SwordTargetHit.GetActor())) {
					UGameplayStatics::ApplyDamage(Enemy, SwordDamage, GetController(), this, nullptr);
					CurrentAmmo = MaxAmmo;
					bIsReloading = false;
					MoveComp->OnComboEvent.Broadcast(TEXT("KUNAI STICK + RELOAD"), FLinearColor(1.f, 0.2f, 0.2f));
				}
			} 
			else 
			{
				SwordMesh->SetWorldLocation(SwordTargetLocation);
				SwordState = ESwordState::Returning; 
			}
		} 
		else 
		{
			SwordMesh->SetWorldLocation(OldLoc + SwordVelocity * DeltaTime);
			SwordMesh->AddLocalRotation(FRotator(2000.f * DeltaTime, 0.f, 0.f)); 
		}
	}
	else if (SwordState == ESwordState::Stuck) 
	{
		SwordStuckTimer -= DeltaTime;
		
		float Dist = FVector::Dist(GetActorLocation(), SwordMesh->GetComponentLocation());
		if (SwordStuckTimer <= 0.f || Dist > MoveComp->Config->SwordMaxDistance) {
			SwordState = ESwordState::Returning; 
		}
	}
	
	if (SwordState == ESwordState::Returning) 
	{
		FVector TargetLoc = CameraComp->GetComponentLocation() + CameraComp->GetForwardVector() * 40.f;
		FVector Dir = (TargetLoc - OldLoc).GetSafeNormal();
		
		SwordMesh->SetWorldLocation(OldLoc + Dir * MoveComp->Config->SwordReturnSpeed * DeltaTime);
		SwordMesh->AddLocalRotation(FRotator(2000.f * DeltaTime, 0.f, 0.f)); 
	}

	if (SwordState == ESwordState::Stuck || SwordState == ESwordState::Returning) 
	{
		if (FVector::Dist(GetActorLocation(), SwordMesh->GetComponentLocation()) < 100.0f) 
		{
			RetrieveSword(); 
		}
	}
}

void APcQPlayerCharacter::RetrieveSword()
{
	SwordState = ESwordState::InHand;
	SwordMesh->AttachToComponent(CameraComp, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	SwordMesh->SetRelativeLocation(BaseSwordLocation);
	SwordMesh->SetRelativeRotation(BaseSwordRotation);
}

float APcQPlayerCharacter::GetPistolCooldownAlpha() const {
	return PistolCooldown <= 0.f ? 0.f : FMath::Clamp(PistolCooldown / FMath::Max(GunCooldownSec, 0.01f), 0.f, 1.f);
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

void APcQPlayerCharacter::TryFire()
{
	if (!CameraComp) return;
	if (PistolCooldown > 0.f || bIsReloading) return;

	if (CurrentAmmo <= 0) {
		bIsReloading = true;
		ReloadTimer = ReloadDuration;
		if (MoveComp) MoveComp->OnComboEvent.Broadcast(TEXT("RELOADING..."), FLinearColor(1.f, 0.2f, 0.2f));
		return;
	}

	CurrentAmmo--;
	PistolCooldown = GunCooldownSec;
	const FVector CamLoc = CameraComp->GetComponentLocation();
	const FVector CamForward = CameraComp->GetForwardVector();
	FVector End = CamLoc + CamForward * 5000.f;

	FVector VisualMuzzleLoc = WeaponMesh->GetSocketLocation(FName("Muzzle"));
	if (VisualMuzzleLoc == WeaponMesh->GetComponentLocation()) { VisualMuzzleLoc += WeaponMesh->GetForwardVector() * 45.f; }

	TArray<FHitResult> Hits;
	FCollisionQueryParams QP; QP.AddIgnoredActor(this);
	GetWorld()->SweepMultiByChannel(Hits, CamLoc, End, FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(GunBulletRadius), QP);

	FHitResult BestHit;
	bool bHitEnemy = false;
	bool bHeadshot = false;

	for (const FHitResult& H : Hits) {
		if (APcQEnemyBase* Enemy = Cast<APcQEnemyBase>(H.GetActor())) {
			bHitEnemy = true;
			BestHit = H;
			if (H.Component.IsValid() && H.Component->ComponentHasTag(FName("Head"))) { bHeadshot = true; break; }
		} else if (H.bBlockingHit && !bHitEnemy) {
			BestHit = H;
		}
	}

	CurrentRecoilRot += FRotator(10.f, FMath::RandRange(-1.f, 1.f), FMath::RandRange(-2.f, 2.f));
	CurrentRecoilLoc += FVector(-12.f, 0.f, 4.f);

	if (bHitEnemy) {
		float FinalDamage = GunBaseDamage;
		if (bHeadshot) FinalDamage *= GunHeadshotMultiplier;
		
		bool bOnBeat = IsOnBeat();
		if (bOnBeat) FinalDamage *= GunBeatMultiplier;
		
		UGameplayStatics::ApplyPointDamage(BestHit.GetActor(), FinalDamage, CamForward, BestHit, GetController(), this, nullptr);
		
		DrawDebugLine(GetWorld(), VisualMuzzleLoc, BestHit.ImpactPoint, bOnBeat ? FColor::Cyan : FColor::Red, false, 0.4f, 0, bOnBeat ? 15.f : 5.f);

		if (MoveComp) { MoveComp->NotifyGunFired(bOnBeat); MoveComp->ResetMobilityAbilities(); }
		if (bOnBeat && MoveComp) MoveComp->OnComboEvent.Broadcast(TEXT("POWER SHOT ★"), FLinearColor(0.2f, 1.f, 1.f));

		if (bHeadshot) {
			PistolCooldown = 0.f;
			CurrentAmmo = MaxAmmo;
			bIsReloading = false;
			if (MoveComp) MoveComp->OnComboEvent.Broadcast(TEXT("HEADSHOT + RELOAD!"), FLinearColor(1.f, 0.1f, 0.1f));
		}
	} else if (BestHit.bBlockingHit) {
		DrawDebugLine(GetWorld(), VisualMuzzleLoc, BestHit.ImpactPoint, FColor::Red, false, 0.1f, 0, 2.f);
		if (MoveComp) MoveComp->NotifyGunFired(false);
	} else {
		DrawDebugLine(GetWorld(), VisualMuzzleLoc, End, FColor::Red, false, 0.1f, 0, 2.f);
		if (MoveComp) MoveComp->NotifyGunFired(false);
	}
}