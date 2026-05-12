#include "PcQEnemyBase.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DamageEvents.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Project_Circle/MusicSystem/MusicGameplaySystem/PcMusicAnalysisSubsystem.h"

APcQEnemyBase::APcQEnemyBase()
{
	PrimaryActorTick.bCanEverTick = true;

	CapsuleComp = CreateDefaultSubobject<UCapsuleComponent>(TEXT("CapsuleComp"));
	CapsuleComp->InitCapsuleSize(45.f, 90.f);
	CapsuleComp->SetCollisionProfileName(TEXT("Pawn"));
	
	// FIX: Let the bullets pass THROUGH the capsule so they hit the meshes!
	CapsuleComp->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	CapsuleComp->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	RootComponent = CapsuleComp;

	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(RootComponent);
	BodyMesh->SetRelativeLocation(FVector(0.f, 0.f, -20.f));
	BodyMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block); // Block Bullets!

	HeadMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeadMesh"));
	HeadMesh->SetupAttachment(RootComponent);
	HeadMesh->SetRelativeLocation(FVector(0.f, 0.f, 60.f));
	HeadMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	HeadMesh->ComponentTags.Add(FName("Head"));

	LeftHandMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftHandMesh"));
	LeftHandMesh->SetupAttachment(RootComponent);
	LeftHandMesh->SetRelativeLocation(FVector(0.f, -45.f, 10.f));
	LeftHandMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	RightHandMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightHandMesh"));
	RightHandMesh->SetupAttachment(RootComponent);
	RightHandMesh->SetRelativeLocation(FVector(0.f, 45.f, 10.f));
	RightHandMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
}

void APcQEnemyBase::BeginPlay()
{
	Super::BeginPlay();

	CurrentHealth = MaxHealth;
	bIsDead = false;
	DeathScaleAlpha = 1.0f;

	BaseHeadScale = HeadMesh->GetRelativeScale3D();
	BaseBodyScale = BodyMesh->GetRelativeScale3D();
	BaseLHandScale = LeftHandMesh->GetRelativeScale3D();
	BaseRHandScale = RightHandMesh->GetRelativeScale3D();

	BaseLHandLoc = LeftHandMesh->GetRelativeLocation();
	BaseRHandLoc = RightHandMesh->GetRelativeLocation();

	CurrentHeadRotation = HeadMesh->GetRelativeRotation();
	CurrentLHandRotation = LeftHandMesh->GetRelativeRotation();
	CurrentRHandRotation = RightHandMesh->GetRelativeRotation();

	TArray<UStaticMeshComponent*> Meshes = { BodyMesh, HeadMesh, LeftHandMesh, RightHandMesh };
	for (UStaticMeshComponent* Mesh : Meshes)
	{
		if (Mesh->GetMaterial(0))
		{
			UMaterialInstanceDynamic* DynMat = Mesh->CreateAndSetMaterialInstanceDynamic(0);
			if (DynMat)
			{
				DynamicMaterials.Add(Mesh, DynMat);
				DynMat->SetVectorParameterValue(FName("EdgeColor"), IdleColor); 
			}
		}
	}

	if (UPcMusicAnalysisSubsystem* MusicSub = GetWorld()->GetSubsystem<UPcMusicAnalysisSubsystem>())
	{
		MusicSub->OnGameplayBeatTriggered.AddDynamic(this, &APcQEnemyBase::OnGameplayBeat);
	}
}

void APcQEnemyBase::OnGameplayBeat(float BeatTime)
{
	if (bIsDead) return;
	BeatPulseAlpha = 1.0f;
}

float APcQEnemyBase::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (bIsDead) return 0.f;

	float ActualDamage = DamageAmount;
	bool bIsHeadshot = false;

	if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
	{
		const FPointDamageEvent* PointDamage = (FPointDamageEvent*)&DamageEvent;
		UPrimitiveComponent* HitComp = PointDamage->HitInfo.Component.Get();

		if (HitComp == HeadMesh) {
			ActualDamage *= 2.0f; 
			bIsHeadshot = true;
			HeadHitAlpha = 1.0f;
			HeadSpinVelocity = 2500.f; 
		} else if (HitComp == LeftHandMesh) {
			LHandHitAlpha = 1.0f;
			LHandSpinVelocity = 1500.f; 
		} else if (HitComp == RightHandMesh) {
			RHandHitAlpha = 1.0f;
			RHandSpinVelocity = -1500.f; 
		} else {
			BodyHitAlpha = 1.0f;
		}
	}
	else
	{
		BodyHitAlpha = 1.0f; 
	}

	CurrentHealth -= ActualDamage;

	if (CurrentHealth <= 0.f)
	{
		bIsDead = true;
		CapsuleComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		HeadSpinVelocity = 4000.f; 
		LHandSpinVelocity = 3000.f;
		RHandSpinVelocity = -3000.f;
	}

	return ActualDamage;
}

void APcQEnemyBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// FIX: Slowed down decays so you can actually see the flashes and jumps!
	BeatPulseAlpha = FMath::FInterpTo(BeatPulseAlpha, 0.f, DeltaTime, 6.f);
	HeadHitAlpha   = FMath::FInterpTo(HeadHitAlpha, 0.f, DeltaTime, 8.f);
	BodyHitAlpha   = FMath::FInterpTo(BodyHitAlpha, 0.f, DeltaTime, 8.f);
	LHandHitAlpha  = FMath::FInterpTo(LHandHitAlpha, 0.f, DeltaTime, 8.f);
	RHandHitAlpha  = FMath::FInterpTo(RHandHitAlpha, 0.f, DeltaTime, 8.f);

	HeadSpinVelocity  = FMath::FInterpTo(HeadSpinVelocity, 0.f, DeltaTime, 3.f);
	LHandSpinVelocity = FMath::FInterpTo(LHandSpinVelocity, 0.f, DeltaTime, 5.f);
	RHandSpinVelocity = FMath::FInterpTo(RHandSpinVelocity, 0.f, DeltaTime, 5.f);

	if (FMath::Abs(HeadSpinVelocity) > 1.f)  CurrentHeadRotation.Yaw += HeadSpinVelocity * DeltaTime;
	if (FMath::Abs(LHandSpinVelocity) > 1.f) CurrentLHandRotation.Pitch += LHandSpinVelocity * DeltaTime;
	if (FMath::Abs(RHandSpinVelocity) > 1.f) CurrentRHandRotation.Pitch += RHandSpinVelocity * DeltaTime;

	HeadMesh->SetRelativeRotation(CurrentHeadRotation);
	LeftHandMesh->SetRelativeRotation(CurrentLHandRotation);
	RightHandMesh->SetRelativeRotation(CurrentRHandRotation);

	if (bIsDead)
	{
		DeathScaleAlpha = FMath::FInterpTo(DeathScaleAlpha, 0.f, DeltaTime, 15.f);
		FVector DeathScale = FVector(DeathScaleAlpha);
		BodyMesh->SetRelativeScale3D(DeathScale);
		
		HeadMesh->AddRelativeLocation(FVector(0.f, 0.f, 800.f * DeltaTime));
		LeftHandMesh->AddRelativeLocation(FVector(0.f, -600.f * DeltaTime, 400.f * DeltaTime));
		RightHandMesh->AddRelativeLocation(FVector(0.f, 600.f * DeltaTime, 400.f * DeltaTime));

		HeadMesh->SetRelativeScale3D(BaseHeadScale * DeathScaleAlpha);
		LeftHandMesh->SetRelativeScale3D(BaseLHandScale * DeathScaleAlpha);
		RightHandMesh->SetRelativeScale3D(BaseRHandScale * DeathScaleAlpha);

		if (DeathScaleAlpha < 0.05f) Destroy();
		return;
	}

	float TimeSecs = GetWorld()->GetTimeSeconds();

	float BodyStretch = 1.f + (BeatPulseAlpha * 0.15f) - (BodyHitAlpha * 0.3f);
	float BodySquash  = 1.f - (BeatPulseAlpha * 0.05f) + (BodyHitAlpha * 0.4f);
	BodyMesh->SetRelativeScale3D(BaseBodyScale * FVector(BodySquash, BodySquash, BodyStretch));

	float HeadZ = FMath::Sin(TimeSecs * 3.f) * 6.f + (BeatPulseAlpha * 12.f);
	HeadMesh->SetRelativeLocation(FVector(0.f, 0.f, 60.f + HeadZ));
	HeadMesh->SetRelativeScale3D(BaseHeadScale * (1.f + (HeadHitAlpha * 0.5f)));

	// FIX: Much bigger amplitude on hand bouncing!
	float LHandZ = FMath::Sin(TimeSecs * 4.f) * 15.f + (BeatPulseAlpha * 25.f);
	float RHandZ = FMath::Sin(TimeSecs * 4.1f + 3.14f) * 15.f + (BeatPulseAlpha * 25.f); 
	LeftHandMesh->SetRelativeLocation(BaseLHandLoc + FVector(0.f, 0.f, LHandZ));
	RightHandMesh->SetRelativeLocation(BaseRHandLoc + FVector(0.f, 0.f, RHandZ));
	
	LeftHandMesh->SetRelativeScale3D(BaseLHandScale * (1.f + (LHandHitAlpha * 0.6f)));
	RightHandMesh->SetRelativeScale3D(BaseRHandScale * (1.f + (RHandHitAlpha * 0.6f)));

	UpdateMeshVisuals(BodyMesh, BodyHitAlpha, DeltaTime);
	UpdateMeshVisuals(HeadMesh, HeadHitAlpha, DeltaTime);
	UpdateMeshVisuals(LeftHandMesh, LHandHitAlpha, DeltaTime);
	UpdateMeshVisuals(RightHandMesh, RHandHitAlpha, DeltaTime);
}

void APcQEnemyBase::UpdateMeshVisuals(UStaticMeshComponent* Mesh, float HitAlpha, float DeltaTime)
{
	if (UMaterialInstanceDynamic** MatPtr = DynamicMaterials.Find(Mesh))
	{
		UMaterialInstanceDynamic* Mat = *MatPtr;
		FLinearColor CurrentColor = FLinearColor::LerpUsingHSV(IdleColor, HitColor, HitAlpha);
		
		Mat->SetVectorParameterValue(FName("EdgeColor"), CurrentColor);
		Mat->SetScalarParameterValue(FName("Flash"), HitAlpha); 
		Mat->SetScalarParameterValue(FName("BeatPulse"), BeatPulseAlpha); 
	}
}