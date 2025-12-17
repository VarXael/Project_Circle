// ==========================================
// FILE: PcEnemyBase.cpp
// PATH: Source/Project_Circle/Enemy/PcEnemyBase.cpp
// ==========================================
#include "PcEnemyBase.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/SceneComponent.h" // Added
#include "Project_Circle/GravitySystem/PcGravityMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

APcEnemyBase::APcEnemyBase()
{
	PrimaryActorTick.bCanEverTick = true;

	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
	RootComponent = MeshComp;

	HitBox = CreateDefaultSubobject<USphereComponent>(TEXT("HitBox"));
	HitBox->SetupAttachment(MeshComp);
	HitBox->InitSphereRadius(80.0f);
	HitBox->SetCollisionProfileName("OverlapAllDynamic");

	// NEW: Initialize the spawn location
	ScoreSpawnLoc = CreateDefaultSubobject<USceneComponent>(TEXT("ScoreSpawnLoc"));
	ScoreSpawnLoc->SetupAttachment(MeshComp);
	// Default it to be 150 units above the center, so it pops over their "head"
	ScoreSpawnLoc->SetRelativeLocation(FVector(0, 0, 150.0f)); 

	GravityComp = CreateDefaultSubobject<UPcGravityMovementComponent>(TEXT("GravityComp"));
	GravityComp->MovementMode = EPcMovementMode::GroundUnit;
	GravityComp->Deceleration = 5000.0f; 
}

void APcEnemyBase::BeginPlay()
{
	Super::BeginPlay();

	if (MeshComp)
	{
		DynamicMat = MeshComp->CreateAndSetMaterialInstanceDynamic(0);
	}
}

void APcEnemyBase::HandleHit()
{
	if (DynamicMat)
	{
		DynamicMat->SetVectorParameterValue(FName("EmissiveColor"), FLinearColor::Red * 10.0f);
	}
	GetWorldTimerManager().SetTimer(TimerHandle_ColorReset, this, &APcEnemyBase::ResetColor, 0.15f, false);
}

void APcEnemyBase::ResetColor()
{
	if (DynamicMat)
	{
		DynamicMat->SetVectorParameterValue(FName("EmissiveColor"), FLinearColor::Black);
	}
}