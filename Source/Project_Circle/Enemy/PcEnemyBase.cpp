// ==========================================
// FILE: PcEnemyBase.cpp
// PATH: Source/Project_Circle/Enemy/PcEnemyBase.cpp
// ==========================================
#include "PcEnemyBase.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Project_Circle/GravitySystem/PcGravityMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

APcEnemyBase::APcEnemyBase()
{
	PrimaryActorTick.bCanEverTick = true;

	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
	RootComponent = MeshComp;

	// Common HitBox for all enemies
	HitBox = CreateDefaultSubobject<USphereComponent>(TEXT("HitBox"));
	HitBox->SetupAttachment(MeshComp);
	HitBox->InitSphereRadius(80.0f);
	HitBox->SetCollisionProfileName("OverlapAllDynamic");

	GravityComp = CreateDefaultSubobject<UPcGravityMovementComponent>(TEXT("GravityComp"));
	// Default settings for Gravity
	GravityComp->MovementMode = EPcMovementMode::GroundUnit;
	// High deceleration ensures that if it somehow gets moved, it stops instantly
	GravityComp->Deceleration = 5000.0f; 
}

void APcEnemyBase::BeginPlay()
{
	Super::BeginPlay();

	// Setup Material for Flashing
	if (MeshComp)
	{
		// Try to create index 0. If your mesh has multiple materials, you might need to loop.
		DynamicMat = MeshComp->CreateAndSetMaterialInstanceDynamic(0);
	}
}

void APcEnemyBase::HandleHit()
{
	// Flash RED
	if (DynamicMat)
	{
		// "EmissiveColor" is standard in many UE materials, change parameter name if using custom material
		DynamicMat->SetVectorParameterValue(FName("EmissiveColor"), FLinearColor::Red * 10.0f);
	}

	// Reset Timer
	GetWorldTimerManager().SetTimer(TimerHandle_ColorReset, this, &APcEnemyBase::ResetColor, 0.15f, false);
}

void APcEnemyBase::ResetColor()
{
	if (DynamicMat)
	{
		DynamicMat->SetVectorParameterValue(FName("EmissiveColor"), FLinearColor::Black);
	}
}