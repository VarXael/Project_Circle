#include "PcQPlayerCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Project_Circle/MusicSystem/MusicGameplaySystem/PcMusicAnalysisSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
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
	DefaultFOV     = CameraComp ? CameraComp->FieldOfView            : 90.f;
	DefaultCameraZ = CameraComp ? CameraComp->GetRelativeLocation().Z : 60.f;
	if (MoveComp) MoveComp->OnActiveBeatAction.AddDynamic(this, &APcQPlayerCharacter::OnActiveBeatAction_Handler);
	if (APlayerController* PC = Cast<APlayerController>(GetController())) {
		PC->bShowMouseCursor = false; PC->SetInputMode(FInputModeGameOnly());
		if (UEnhancedInputLocalPlayerSubsystem* Sub = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
			if (DefaultMappingContext) Sub->AddMappingContext(DefaultMappingContext, 0);
	}

	if (UPcMusicAnalysisSubsystem* Sub = GetWorld()->GetSubsystem<UPcMusicAnalysisSubsystem>()) {
		Sub->OnGameplayBeatTriggered.AddDynamic(this, &APcQPlayerCharacter::OnGameplayBeat);
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
		if (IA_GroundPound) EIC->BindAction(IA_GroundPound, ETriggerEvent::Started, this, &APcQPlayerCharacter::Input_GroundPound);
		
		// NEW: Bind the Fire Action
		if (IA_Fire) EIC->BindAction(IA_Fire, ETriggerEvent::Started, this, &APcQPlayerCharacter::Input_Fire);
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
void APcQPlayerCharacter::Input_GroundPound() { if (MoveComp) MoveComp->OnGroundPoundPressed(); }
void APcQPlayerCharacter::OnGameplayBeat(float) { if (MoveComp) MoveComp->TriggerBeatJump(); }

void APcQPlayerCharacter::OnActiveBeatAction_Handler() { TryFire(); }

void APcQPlayerCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UpdateCameraEffects(DeltaTime);
	if (PistolCooldown > 0.f) PistolCooldown = FMath::Max(0.f, PistolCooldown - DeltaTime);
}

void APcQPlayerCharacter::UpdateCameraEffects(float DeltaTime)
{
	if (!CameraComp || !MoveComp) return;
	const float Target = MoveComp->IsPowerBoosting() ? 1.f : 0.f;
	CurrentBoostAlpha  = FMath::FInterpTo(CurrentBoostAlpha, Target, DeltaTime, BoostCameraSpeed);
	CameraComp->SetRelativeLocation(FVector(0.f, 0.f, FMath::Lerp(DefaultCameraZ, DefaultCameraZ - BoostCameraDropZ, CurrentBoostAlpha)));
	CameraComp->SetFieldOfView(FMath::Lerp(DefaultFOV, DefaultFOV + BoostFOVGain, CurrentBoostAlpha));
}

float APcQPlayerCharacter::GetPistolCooldownAlpha() const
{
	return PistolCooldown <= 0.f ? 0.f : FMath::Clamp(PistolCooldown / FMath::Max(PistolBaseCooldownSec, 0.01f), 0.f, 1.f);
}

// =============================================================================
//  COMBAT LOGIC
// =============================================================================

bool APcQPlayerCharacter::IsOnBeat() const
{
	UPcMusicAnalysisSubsystem* MusicSub = GetWorld()->GetSubsystem<UPcMusicAnalysisSubsystem>();
	if (!MusicSub || !MusicSub->IsReadyForPlayback()) return false;

	int32 CurrentTime = MusicSub->GetCurrentPlaybackTimeMS();
	int32 NextBeat = MusicSub->GetNextGameplayBeatTimeMS();
	int32 Interval = FMath::RoundToInt(MusicSub->GetGameplayBeatIntervalMS());
	int32 PrevBeat = NextBeat - Interval;

	int32 DistToNext = FMath::Abs(NextBeat - CurrentTime);
	int32 DistToPrev = FMath::Abs(CurrentTime - PrevBeat);
	
	// 120ms tolerance window (adjust this to make the rhythm window more/less strict)
	return FMath::Min(DistToNext, DistToPrev) <= 120; 
}

void APcQPlayerCharacter::Input_Fire() { TryFire(); }

void APcQPlayerCharacter::TryFire()
{
	if (!CameraComp) return;
	if (MoveComp && MoveComp->IsWallSwimming()) return;

	const bool bOnBeat = IsOnBeat();

	// On-beat = free shot: fires regardless of cooldown, then RESETS CD to 0
	//   so the player can immediately fire again if they want.
	// Off-beat + CD active = blocked.
	// Off-beat + CD ready  = fires normally, starts CD.
	if (PistolCooldown > 0.f && !bOnBeat)
	{
		UE_LOG(LogTemp, Log, TEXT("[Pistol] Blocked — cooldown %.2fs remaining."), PistolCooldown);
		return;
	}

	FVector CamLoc = CameraComp->GetComponentLocation();
	FVector CamForward = CameraComp->GetForwardVector();

	if (bOnBeat)
	{
		// Free shot: reset CD so player can fire again immediately
		PistolCooldown = 0.f;
		UE_LOG(LogTemp, Log, TEXT("[Pistol] On-beat free shot — CD reset to 0."));
	}
	else
	{
		// Normal shot: start CD
		UPcMusicAnalysisSubsystem* Sub = GetWorld()->GetSubsystem<UPcMusicAnalysisSubsystem>();
		PistolCooldown = (Sub && Sub->IsReadyForPlayback() && MoveComp)
		               ? MoveComp->GetBeatSnappedDuration(PistolBaseCooldownSec)
		               : PistolBaseCooldownSec;
	}

	if (MoveComp) MoveComp->NotifyGunFired();

	if (IsOnBeat()) 
	{
		// --- ON BEAT: AUTO-SNAP ---
		TArray<AActor*> Enemies;
		UGameplayStatics::GetAllActorsOfClass(GetWorld(), APcQEnemyBase::StaticClass(), Enemies);

		APcQEnemyBase* BestEnemy = nullptr;
		// 0.90 DotProduct is roughly a 25-degree cone from the center of the screen
		float BestDot = 0.90f; 

		for (AActor* EnemyActor : Enemies)
		{
			FVector DirToEnemy = (EnemyActor->GetActorLocation() - CamLoc).GetSafeNormal();
			float Dot = FVector::DotProduct(CamForward, DirToEnemy);

			if (Dot > BestDot) {
				BestDot = Dot;
				BestEnemy = Cast<APcQEnemyBase>(EnemyActor);
			}
		}

		if (BestEnemy) {
			// Found an enemy in the generous cone! Auto-snap hit.
			UGameplayStatics::ApplyDamage(BestEnemy, BaseDamage, GetController(), this, nullptr);
			
			// Draw a thick CYAN line to show the auto-aim worked
			DrawDebugLine(GetWorld(), CamLoc, BestEnemy->GetActorLocation(), FColor::Cyan, false, 0.5f, 0, 5.0f);
			UE_LOG(LogTemp, Warning, TEXT("PERFECT BEAT HIT!"));
			return; // Exit out so we don't fire the standard raycast
		}
	}
	
	// --- OFF BEAT (Or no enemy in cone): STANDARD RAYCAST ---
	FHitResult HitResult;
	FVector EndLoc = CamLoc + (CamForward * 5000.f);
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);

	bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, CamLoc, EndLoc, ECC_Visibility, QueryParams);
	
	if (bHit) {
		UGameplayStatics::ApplyDamage(HitResult.GetActor(), BaseDamage, GetController(), this, nullptr);
		// Draw a thin RED line to show standard shooting
		DrawDebugLine(GetWorld(), CamLoc, HitResult.ImpactPoint, FColor::Red, false, 0.2f, 0, 1.0f);
	} else {
		DrawDebugLine(GetWorld(), CamLoc, EndLoc, FColor::Red, false, 0.2f, 0, 1.0f);
	}
}