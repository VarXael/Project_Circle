#include "PcWeapon.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Project_Circle/Planet/PcPlanet.h" 
#include "Camera/CameraComponent.h"
#include "Project_Circle/PcPlayer/PcPlayerCharacter.h"
#include "Project_Circle/PcPlayer/PcProjectile.h"

APcWeapon::APcWeapon()
{
	PrimaryActorTick.bCanEverTick = true;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	
	WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
	WeaponMesh->SetupAttachment(RootComponent);
	
	MuzzleLocation = CreateDefaultSubobject<USceneComponent>(TEXT("Muzzle"));
	MuzzleLocation->SetupAttachment(WeaponMesh);
	MuzzleLocation->SetRelativeLocation(FVector(60.0f, 0.0f, 10.0f)); 
}

void APcWeapon::BeginPlay()
{
	Super::BeginPlay();
	bIsLaserReady = true; 
}

void APcWeapon::AttachToPlayer(APcPlayerCharacter* TargetPlayer)
{
	OwningPlayer = TargetPlayer;
	if (OwningPlayer && OwningPlayer->CameraComp)
	{
		this->AttachToComponent(OwningPlayer->CameraComp, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		SetActorRelativeLocation(FVector(40.0f, 20.0f, -20.0f)); 
		SetActorRelativeRotation(FRotator(0, 0, 0));
	}
}

void APcWeapon::ApplyInputForSway(FVector2D LookInput)
{
	CurrentLookInput = LookInput;
}

// --- STANDARD FIRE ---
void APcWeapon::StartPrimaryFire()
{
	PerformStandardShot(); 
	GetWorld()->GetTimerManager().SetTimer(TimerHandle_AutoFire, this, &APcWeapon::PerformStandardShot, FireRate, true);
}

void APcWeapon::StopPrimaryFire()
{
	GetWorld()->GetTimerManager().ClearTimer(TimerHandle_AutoFire);
}

void APcWeapon::PerformStandardShot()
{
	// Simple Recoil Kick
	CurrentRecoilLoc += FVector(-5.0f, 0, 0); 
	
	if (ProjectileClass && OwningPlayer)
	{
		FVector SpawnLoc = MuzzleLocation->GetComponentLocation();
		APcPlanet* Planet = OwningPlayer->CurrentPlanet;
		
		// If on planet, Gravity is Outwards. Surface Normal is Inwards (Opposite).
		FVector GravityDir = (Planet) ? Planet->GetGravityDirection(SpawnLoc) : FVector(0,0,-1);
		FVector SurfaceNormal = -GravityDir; 

		FVector CamFwd = OwningPlayer->CameraComp->GetForwardVector();
		FVector ShootDir = FVector::VectorPlaneProject(CamFwd, SurfaceNormal).GetSafeNormal();

		FActorSpawnParameters P; P.Owner = OwningPlayer; P.Instigator = OwningPlayer;
		auto* Proj = GetWorld()->SpawnActor<APcProjectile>(ProjectileClass, SpawnLoc, ShootDir.Rotation(), P);
		if (Proj) Proj->InitializeProjectile(ShootDir, Planet, true);
	}
}

// --- LASER FIRE ---
void APcWeapon::FireLaserAttack()
{
	if (!bIsLaserReady || !OwningPlayer) return;

	bIsLaserReady = false;
	
	// 1. Raycast for Target
	FVector CamLoc = OwningPlayer->CameraComp->GetComponentLocation();
	FVector CamFwd = OwningPlayer->CameraComp->GetForwardVector();
	FVector TraceEnd = CamLoc + (CamFwd * LaserMaxRange); 
	FVector BeamTargetPoint = TraceEnd; 

	FHitResult Hit;
	FCollisionQueryParams P; P.AddIgnoredActor(this); P.AddIgnoredActor(OwningPlayer);

	if (GetWorld()->LineTraceSingleByChannel(Hit, CamLoc, TraceEnd, ECC_Visibility, P))
	{
		BeamTargetPoint = Hit.Location;
		// Apply Damage logic here later...
	}

	// 2. Spawn FX
	if (LaserBeamFX)
	{
		UNiagaraComponent* BeamComp = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			GetWorld(), LaserBeamFX, MuzzleLocation->GetComponentLocation(), FRotator::ZeroRotator, FVector::OneVector, true, true, ENCPoolMethod::None, true
		);

		if (BeamComp)
		{
			// Assumes your Niagara System has a Vector parameter named "BeamEnd"
			BeamComp->SetNiagaraVariableVec3(FString("BeamEnd"), BeamTargetPoint);
		}
	}

	// 3. Cooldown & Recoil
	GetWorld()->GetTimerManager().SetTimer(TimerHandle_LaserCooldown, this, &APcWeapon::ResetLaserCooldown, LaserCooldownDuration, false);
	CurrentRecoilLoc += FVector(-20.0f, 0, 0); // Heavy Kick
}

void APcWeapon::ResetLaserCooldown()
{
	bIsLaserReady = true;
}

// --- ANIMATION ---
void APcWeapon::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Spring Interp
	CurrentRecoilLoc = FMath::VInterpTo(CurrentRecoilLoc, FVector::ZeroVector, DeltaTime, 10.0f);
	CurrentRecoilRot = FMath::RInterpTo(CurrentRecoilRot, FRotator::ZeroRotator, DeltaTime, 10.0f);

	// Sway Logic
	FVector TargetSwayLoc = FVector(0.0f, -CurrentLookInput.X * SwayAmount, CurrentLookInput.Y * SwayAmount);
	CurrentSwayLoc = FMath::VInterpTo(CurrentSwayLoc, TargetSwayLoc, DeltaTime, SwaySpeed);
	
	// Fade input
	CurrentLookInput = FMath::Vector2DInterpTo(CurrentLookInput, FVector2D::ZeroVector, DeltaTime, 10.0f);

	WeaponMesh->SetRelativeLocation(CurrentRecoilLoc + CurrentSwayLoc);
	WeaponMesh->SetRelativeRotation(CurrentRecoilRot);
}