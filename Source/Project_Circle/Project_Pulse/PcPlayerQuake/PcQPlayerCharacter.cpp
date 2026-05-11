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
#include "Project_Circle/Project_Pulse/Core/PcQHealthComponent.h"
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
	
	BaseSwordLocation = SwordMesh->GetRelativeLocation();
	BaseSwordRotation = SwordMesh->GetRelativeRotation();
	BaseWeaponLocation = WeaponMesh->GetRelativeLocation();
	BaseWeaponRotation = WeaponMesh->GetRelativeRotation();

	CurrentAmmo = MaxAmmo;

	if (MoveComp)
	{
		MoveComp->OnGroundPulseHit.AddDynamic(this, &APcQPlayerCharacter::HandleGroundPulseHit);
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
		if (IA_GroundPound) EIC->BindAction(IA_GroundPound, ETriggerEvent::Started, this, &APcQPlayerCharacter::Input_GroundPound);
		if (IA_Fire) EIC->BindAction(IA_Fire, ETriggerEvent::Started, this, &APcQPlayerCharacter::Input_Fire);
		if (IA_Melee) EIC->BindAction(IA_Melee, ETriggerEvent::Started, this, &APcQPlayerCharacter::Input_Melee);
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
void APcQPlayerCharacter::Input_Fire()         { TryFire(); }
void APcQPlayerCharacter::Input_Melee()        { TryMelee(); }

void APcQPlayerCharacter::OnGameplayBeat(float)
{
	BeatFOVOffset = CameraBeatPunch;
	if (MoveComp) MoveComp->TriggerGroundPulse();
}

void APcQPlayerCharacter::HandleGroundPulseHit()
{
	if (CurrentAmmo < MaxAmmo || bIsReloading)
	{
		CurrentAmmo = MaxAmmo;
		bIsReloading = false;
		ReloadTimer = 0.f;
		if (MoveComp) MoveComp->OnComboEvent.Broadcast(TEXT("PULSE AMMO REFUND"), FLinearColor(0.2f, 1.f, 0.8f));
	}
}

void APcQPlayerCharacter::HandleSwordHitEnemy(APcQEnemyBase* Enemy)
{
	if (Enemy) {
		UGameplayStatics::ApplyDamage(Enemy, SwordDamage, GetController(), this, nullptr);
		// Reset CD on hit
		SwordCooldownTimer = 0.f; 
	}
}

void APcQPlayerCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UpdateCameraEffects(DeltaTime);
	UpdateWeaponSway(DeltaTime);
	if (PistolCooldown > 0.f) PistolCooldown = FMath::Max(0.f, PistolCooldown - DeltaTime);
	if (SwordCooldownTimer > 0.f) SwordCooldownTimer = FMath::Max(0.f, SwordCooldownTimer - DeltaTime);

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
	CurrentLookDelta = FMath::Vector2DInterpTo(CurrentLookDelta, FVector2D::ZeroVector, DeltaTime, 15.f);
	FVector LocalVel = GetActorRotation().UnrotateVector(GetVelocity());
	float SwayVelZ = FMath::Clamp(LocalVel.Z, -600.f, 600.f);

	if (WeaponMesh)
	{
		FRotator TargetRotSway = FRotator(CurrentLookDelta.Y * SwayRotMultiplier, CurrentLookDelta.X * SwayRotMultiplier, CurrentLookDelta.X * -0.7f);
		FVector TargetLocSway = FVector(LocalVel.X * -0.003f, LocalVel.Y * -0.003f, SwayVelZ * 0.004f);

		TargetLocSway.X = FMath::Clamp(TargetLocSway.X, -8.f, 5.f);
		TargetLocSway.Y = FMath::Clamp(TargetLocSway.Y, -5.f, 5.f);
		TargetLocSway.Z = FMath::Clamp(TargetLocSway.Z, -6.f, 6.f);

		if (BeatFOVOffset > 0.1f) TargetLocSway.Z -= 1.5f; 
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

	if (SwordMesh)
	{
		FRotator TargetSwordRotSway = FRotator(CurrentLookDelta.Y * SwayRotMultiplier, CurrentLookDelta.X * SwayRotMultiplier, CurrentLookDelta.X * 0.7f);
		FVector TargetSwordLocSway = FVector(LocalVel.X * -0.003f, LocalVel.Y * -0.003f, SwayVelZ * 0.004f);

		TargetSwordLocSway.X = FMath::Clamp(TargetSwordLocSway.X, -8.f, 5.f);
		TargetSwordLocSway.Y = FMath::Clamp(TargetSwordLocSway.Y, -5.f, 5.f);
		TargetSwordLocSway.Z = FMath::Clamp(TargetSwordLocSway.Z, -6.f, 6.f);

		if (BeatFOVOffset > 0.1f) TargetSwordLocSway.Z -= 1.5f; 

		// --- PROCEDURAL HORIZONTAL SLASH ---
		if (SwordStrikeTimer > 0.f) {
			SwordStrikeTimer -= DeltaTime;
			float Alpha = FMath::Clamp(SwordStrikeTimer / SwordStrikeMaxTime, 0.f, 1.f);
			float Progress = 1.f - Alpha; // Goes from 0 to 1 during the slash
			
			float Swing = FMath::Sin(Progress * PI); // Starts at 0, peaks at 1 in middle, ends at 0
			
			// X: Push out, Y: Sweep hard from right to left across screen, Z: Dip down slightly
			SwordStrikeLocOffset = FVector(Swing * 45.f, (Progress * -140.f) + 70.f, Swing * -15.f);
			
			// Roll: Tilt blade sideways. Yaw: Swing blade across.
			SwordStrikeRotOffset = FRotator(Swing * -20.f, (Progress * -160.f) + 80.f, Swing * -60.f);
		} else {
			SwordStrikeLocOffset = FMath::VInterpTo(SwordStrikeLocOffset, FVector::ZeroVector, DeltaTime, 12.f);
			SwordStrikeRotOffset = FMath::RInterpTo(SwordStrikeRotOffset, FRotator::ZeroRotator, DeltaTime, 12.f);
		}

		CurrentSwordSwayRot = FMath::RInterpTo(CurrentSwordSwayRot, TargetSwordRotSway, DeltaTime, SwaySmoothness);
		CurrentSwordSwayLoc = FMath::VInterpTo(CurrentSwordSwayLoc, TargetSwordLocSway, DeltaTime, SwaySmoothness);

		SwordMesh->SetRelativeRotation(BaseSwordRotation + CurrentSwordSwayRot + SwordStrikeRotOffset);
		SwordMesh->SetRelativeLocation(BaseSwordLocation + CurrentSwordSwayLoc + SwordStrikeLocOffset);
	}
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

void APcQPlayerCharacter::TryFire()
{
	if (!CameraComp) return;
	if (PistolCooldown > 0.f || bIsReloading) return;

	if (CurrentAmmo <= 0)
	{
		bIsReloading = true;
		ReloadTimer = ReloadDuration;
		if (MoveComp) MoveComp->OnComboEvent.Broadcast(TEXT("RELOADING..."), FLinearColor(1.f, 0.2f, 0.2f));
		return;
	}

	CurrentAmmo--;
	PistolCooldown = PistolBaseCooldownSec;
	const FVector CamLoc = CameraComp->GetComponentLocation();
	const FVector CamForward = CameraComp->GetForwardVector();
	
	FVector VisualMuzzleLoc = WeaponMesh->GetSocketLocation(FName("Muzzle"));
	if (VisualMuzzleLoc == WeaponMesh->GetComponentLocation()) {
		VisualMuzzleLoc += WeaponMesh->GetForwardVector() * 45.f; 
	}

	// ── THICK BULLET SPHERE TRACE ──
	FHitResult Hit;
	FCollisionQueryParams QP; QP.AddIgnoredActor(this);
	FVector End = CamLoc + CamForward * 5000.f;
	FCollisionShape ThickBullet = FCollisionShape::MakeSphere(25.f); // Bridges the gaps!
	
	bool bHitSomething = GetWorld()->SweepSingleByChannel(Hit, CamLoc, End, FQuat::Identity, ECC_Visibility, ThickBullet, QP);

	bool bHitEnemy = false;
	if (bHitSomething)
	{
		APcQEnemyBase* Enemy = Cast<APcQEnemyBase>(Hit.GetActor());
		if (Enemy) 
		{
			bHitEnemy = true;
			
			// ── BEAT CHECK ONLY MATTERS IF YOU ACTUALLY HIT THEM ──
			if (IsOnBeat()) 
			{
				UGameplayStatics::ApplyPointDamage(Enemy, BaseDamage * 3.f, CamForward, Hit, GetController(), this, nullptr);
				DrawDebugLine(GetWorld(), VisualMuzzleLoc, Hit.ImpactPoint, FColor::Cyan, false, 0.4f, 0, 15.f);
				if (MoveComp) {
					MoveComp->NotifyGunFired(true); 
					MoveComp->OnComboEvent.Broadcast(TEXT("POWER SHOT ★"), FLinearColor(0.2f, 1.f, 1.f));
				}
			} 
			else 
			{
				UGameplayStatics::ApplyPointDamage(Enemy, BaseDamage, CamForward, Hit, GetController(), this, nullptr);
				DrawDebugLine(GetWorld(), VisualMuzzleLoc, Hit.ImpactPoint, FColor::Red, false, 0.1f, 0, 2.f);
				if (MoveComp) MoveComp->NotifyGunFired(false);
			}
		} 
		else 
		{
			// Hit Wall
			DrawDebugLine(GetWorld(), VisualMuzzleLoc, Hit.ImpactPoint, FColor::Red, false, 0.1f, 0, 2.f);
			if (MoveComp) MoveComp->NotifyGunFired(false);
		}
	}
	else 
	{
		// Complete Miss
		DrawDebugLine(GetWorld(), VisualMuzzleLoc, End, FColor::Red, false, 0.1f, 0, 2.f);
		if (MoveComp) MoveComp->NotifyGunFired(false);
	}

	CurrentRecoilRot += FRotator(10.f, FMath::RandRange(-1.f, 1.f), FMath::RandRange(-2.f, 2.f));
	CurrentRecoilLoc += FVector(-12.f, 0.f, 4.f);

	if (bHitEnemy)
	{
		// Refresh everything on a successful hit!
		SwordCooldownTimer = 0.f; 
		if (MoveComp) MoveComp->ResetMobilityAbilities();
	}
}

void APcQPlayerCharacter::TryMelee()
{
	// Native 2.5s Cooldown
	if (SwordCooldownTimer > 0.f || !MoveComp || !CameraComp) return;
	
	SwordStrikeTimer = SwordStrikeMaxTime;
	SwordCooldownTimer = MoveComp->Config ? MoveComp->Config->SlashCooldownSec : 2.5f; 

	FVector WorldInput = GetPendingMovementInputVector(); 
	FVector LungeDir = CameraComp->GetForwardVector();

	if (!WorldInput.IsNearlyZero()) {
		FVector CamFwd2D = CameraComp->GetForwardVector().GetSafeNormal2D();
		FVector CamRight2D = CameraComp->GetRightVector().GetSafeNormal2D();
		FVector Input2D = WorldInput.GetSafeNormal2D();
		
		float FwdDot = FVector::DotProduct(Input2D, CamFwd2D);
		float RightDot = FVector::DotProduct(Input2D, CamRight2D);
		
		LungeDir = (CameraComp->GetForwardVector() * FwdDot + CameraComp->GetRightVector() * RightDot).GetSafeNormal();
	}

	MoveComp->DoSwordLunge(LungeDir);
}