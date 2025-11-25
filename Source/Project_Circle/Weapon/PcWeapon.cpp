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
	CurrentRecoverySpeed = StandardRecoverySpeed;
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

// --- INPUT HANDLER ---
void APcWeapon::ApplyInputForSway(FVector2D LookInput)
{
	// We accumulate input (or overwrite it)
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
	CurrentRecoilLoc += StandardRecoilPos;
	CurrentRecoilRot += StandardRecoilRot;
	CurrentRecoverySpeed = StandardRecoverySpeed; // Standard snaps back fast

	if (ProjectileClass && OwningPlayer)
	{
		FVector SpawnLoc = MuzzleLocation->GetComponentLocation();
		APcPlanet* Planet = OwningPlayer->CurrentPlanet;
		FVector GravityDir = (Planet) ? Planet->GetGravityDirection(SpawnLoc) : FVector(0,0,-1);
		FVector SurfaceNormal = -GravityDir;

		FVector CamFwd = OwningPlayer->CameraComp->GetForwardVector();
		FVector ShootDir = FVector::VectorPlaneProject(CamFwd, SurfaceNormal).GetSafeNormal();

		FActorSpawnParameters P; P.Owner = OwningPlayer; P.Instigator = OwningPlayer;
		auto* Proj = GetWorld()->SpawnActor<APcProjectile>(ProjectileClass, SpawnLoc, ShootDir.Rotation(), P);
		if (Proj) Proj->InitializeProjectile(ShootDir, Planet, true);
	}
}

// --- LASER ATTACK ---
void APcWeapon::FireLaserAttack()
{
	// --- 1. CHECK COOLDOWN ---
	if (!bIsLaserReady) 
	{
		return; // Weapon is hot, cannot fire
	}

	// Lock the weapon
	bIsLaserReady = false;
	
	// --- 2. CALCULATE TARGET (The Hitscan) ---
	FVector CamLoc = OwningPlayer->CameraComp->GetComponentLocation();
	FVector CamFwd = OwningPlayer->CameraComp->GetForwardVector();
	
	// We trace from the Camera (Crosshair) out to Max Range
	FVector TraceEnd = CamLoc + (CamFwd * LaserMaxRange); 
	FVector BeamTargetPoint = TraceEnd; // Default to sky if we miss

	FHitResult Hit;
	FCollisionQueryParams P;
	P.AddIgnoredActor(this); // Ignore Weapon
	P.AddIgnoredActor(OwningPlayer); // Ignore Player

	// Perform the Trace
	if (GetWorld()->LineTraceSingleByChannel(Hit, CamLoc, TraceEnd, ECC_Visibility, P))
	{
		BeamTargetPoint = Hit.Location;
		
		// OPTIONAL: Apply Damage here
		// UGameplayStatics::ApplyDamage(Hit.GetActor(), 100.0f, OwningPlayer->GetController(), this, UDamageType::StaticClass());
	}

	// --- 3. SPAWN THE VISUAL BEAM (STATIC) ---
	if (LaserBeamFX)
	{
		// 1. Get the CURRENT position of the muzzle (Snapshot)
		FVector SpawnLocation = MuzzleLocation->GetComponentLocation();
		FRotator SpawnRotation = FRotator::ZeroRotator; // Rotation doesn't matter for a beam connecting two points

		// 2. Spawn "At Location" instead of "Attached"
		UNiagaraComponent* BeamComp = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			GetWorld(),
			LaserBeamFX,
			SpawnLocation,
			SpawnRotation,
			FVector::OneVector,
			true, // Auto Destroy
			true, // Auto Activate
			ENCPoolMethod::None,
			true // PreCull Check
		);

		if (BeamComp)
		{
			// 3. Set the Beam End (Same as before)
			BeamComp->SetNiagaraVariableVec3(FString("BeamEnd"), BeamTargetPoint);
			
			// Note: Since we spawned "At Location", the system's origin (0,0,0) 
			// is now permanently fixed at the coordinate where you fired.
			// Even if you move the gun, the beam start point stays put.
		}
	}

	// --- 4. SET COOLDOWN TIMER ---
	// Subtract the buffer so it feels responsive (ready slightly before animation ends)
	float ActualCooldown = FMath::Max(0.1f, LaserCooldownDuration - LaserInputBuffer);
	GetWorld()->GetTimerManager().SetTimer(TimerHandle_LaserCooldown, this, &APcWeapon::ResetLaserCooldown, ActualCooldown, false);

	// --- 5. APPLY HEAVY RECOIL ANIMATION ---
	// Calculate speed so the gun settles exactly when the cooldown finishes
	CurrentRecoverySpeed = 5.0f / LaserCooldownDuration; 
	
	CurrentRecoilLoc += LaserRecoilPos; // Big Kick Back
	CurrentRecoilRot += LaserRecoilRot; // Big Muzzle Rise
	
	UE_LOG(LogTemp, Warning, TEXT("LASER BLAST! Hit: %s"), *BeamTargetPoint.ToString());
}

void APcWeapon::ResetLaserCooldown()
{
	bIsLaserReady = true;
	UE_LOG(LogTemp, Log, TEXT("Laser Ready!"));
}

// --- TICK (ANIMATION MIXER) ---
void APcWeapon::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 1. INTERP RECOIL (Spring)
	CurrentRecoilLoc = FMath::VInterpTo(CurrentRecoilLoc, FVector::ZeroVector, DeltaTime, CurrentRecoverySpeed);
	CurrentRecoilRot = FMath::RInterpTo(CurrentRecoilRot, FRotator::ZeroRotator, DeltaTime, CurrentRecoverySpeed);

	// 2. INTERP SWAY (Lag)
	// Target Sway is Inverse of Input (Look Right -> Gun lags Left)
	FVector TargetSwayLoc = FVector(0.0f, -CurrentLookInput.X * SwayAmount, CurrentLookInput.Y * SwayAmount);
	TargetSwayLoc.Y = FMath::Clamp(TargetSwayLoc.Y, -SwayMax, SwayMax);
	TargetSwayLoc.Z = FMath::Clamp(TargetSwayLoc.Z, -SwayMax, SwayMax);

	// Add some rotation sway (Tilt)
	FRotator TargetSwayRot = FRotator(CurrentLookInput.Y * SwayRotationAmount, CurrentLookInput.X * SwayRotationAmount, 0.0f);

	// Smooth the sway
	CurrentSwayLoc = FMath::VInterpTo(CurrentSwayLoc, TargetSwayLoc, DeltaTime, SwaySpeed);
	CurrentSwayRot = FMath::RInterpTo(CurrentSwayRot, TargetSwayRot, DeltaTime, SwaySpeed);

	// Reset Input (so it returns to center if player stops moving mouse)
	CurrentLookInput = FMath::Vector2DInterpTo(CurrentLookInput, FVector2D::ZeroVector, DeltaTime, 10.0f);


	// 3. COMBINE EVERYTHING
	WeaponMesh->SetRelativeLocation(CurrentRecoilLoc + CurrentSwayLoc);
	WeaponMesh->SetRelativeRotation(CurrentRecoilRot + CurrentSwayRot);
}