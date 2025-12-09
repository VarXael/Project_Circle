#include "PcWeapon.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Project_Circle/Planet/PcPlanet.h" 
#include "Camera/CameraComponent.h"
#include "Project_Circle/PcPlayer/PcPlayerCharacter.h"
#include "Project_Circle/PcPlayer/PcProjectile.h"
// Include the component header to access GetSurfaceNormal
#include "Project_Circle/GravitySystem/PcGravityMovementComponent.h"

APcWeapon::APcWeapon()
{
	PrimaryActorTick.bCanEverTick = true;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	
	WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
	WeaponMesh->SetupAttachment(RootComponent);
	
	MuzzleLocation = CreateDefaultSubobject<USceneComponent>(TEXT("Muzzle"));
	MuzzleLocation->SetupAttachment(WeaponMesh);
	MuzzleLocation->SetRelativeLocation(MuzzleOffset); 
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
		SetActorRelativeLocation(WeaponOffset);
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
	CurrentRecoilLoc += FVector(-5.0f, 0, 0); 
	
	if (ProjectileClass && OwningPlayer)
	{
		FVector SpawnLoc = MuzzleLocation->GetComponentLocation();
		
		// ERROR FIX: Use the Player's Component to find Up, not a raw variable.
		FVector SurfaceNormal = FVector::UpVector;
		if (OwningPlayer->GravityComp)
		{
			SurfaceNormal = OwningPlayer->GravityComp->GetSurfaceNormal();
		}

		// Flatten the shot parallel to the ground (Standard FPS logic)
		FVector CamFwd = OwningPlayer->CameraComp->GetForwardVector();
		FVector ShootDir = FVector::VectorPlaneProject(CamFwd, SurfaceNormal).GetSafeNormal();

		FActorSpawnParameters P; P.Owner = OwningPlayer; P.Instigator = OwningPlayer;
		
		auto* Proj = GetWorld()->SpawnActor<APcProjectile>(ProjectileClass, SpawnLoc, ShootDir.Rotation(), P);
		if (Proj) 
		{
			// We pass nullptr for Planet because the Projectile's own GravityComponent 
			// will find the planet automatically in its BeginPlay.
			Proj->InitializeProjectile(ShootDir, nullptr, true);
		}
	}
}

// --- LASER FIRE ---
void APcWeapon::FireLaserAttack()
{
	if (!bIsLaserReady || !OwningPlayer) return;

	bIsLaserReady = false;
	
	FVector CamLoc = OwningPlayer->CameraComp->GetComponentLocation();
	FVector CamFwd = OwningPlayer->CameraComp->GetForwardVector();
	FVector TraceEnd = CamLoc + (CamFwd * LaserMaxRange); 
	FVector BeamTargetPoint = TraceEnd; 

	FHitResult Hit;
	FCollisionQueryParams P; P.AddIgnoredActor(this); P.AddIgnoredActor(OwningPlayer);

	if (GetWorld()->LineTraceSingleByChannel(Hit, CamLoc, TraceEnd, ECC_Visibility, P))
	{
		BeamTargetPoint = Hit.Location;
	}

	if (LaserBeamFX)
	{
		UNiagaraComponent* BeamComp = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			GetWorld(), LaserBeamFX, MuzzleLocation->GetComponentLocation(), FRotator::ZeroRotator, FVector::OneVector, true, true, ENCPoolMethod::None, true
		);

		if (BeamComp)
		{
			// WARNING FIX: Use FName variant instead of FString
			BeamComp->SetVariableVec3(FName("BeamEnd"), BeamTargetPoint);
		}
	}

	GetWorld()->GetTimerManager().SetTimer(TimerHandle_LaserCooldown, this, &APcWeapon::ResetLaserCooldown, LaserCooldownDuration, false);
	CurrentRecoilLoc += FVector(-20.0f, 0, 0); 
}

void APcWeapon::ResetLaserCooldown()
{
	bIsLaserReady = true;
}

// --- ANIMATION ---
void APcWeapon::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	CurrentRecoilLoc = FMath::VInterpTo(CurrentRecoilLoc, FVector::ZeroVector, DeltaTime, 10.0f);
	CurrentRecoilRot = FMath::RInterpTo(CurrentRecoilRot, FRotator::ZeroRotator, DeltaTime, 10.0f);

	FVector TargetSwayLoc = FVector(0.0f, -CurrentLookInput.X * SwayAmount, CurrentLookInput.Y * SwayAmount);
	CurrentSwayLoc = FMath::VInterpTo(CurrentSwayLoc, TargetSwayLoc, DeltaTime, SwaySpeed);
	
	CurrentLookInput = FMath::Vector2DInterpTo(CurrentLookInput, FVector2D::ZeroVector, DeltaTime, 10.0f);

	WeaponMesh->SetRelativeLocation(CurrentRecoilLoc + CurrentSwayLoc);
	WeaponMesh->SetRelativeRotation(CurrentRecoilRot);
}