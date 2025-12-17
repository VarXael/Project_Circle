// ==========================================
// FILE: PcFloatingScore.cpp
// PATH: Source/Project_Circle/UI/PcFloatingScore.cpp
// ==========================================
#include "PcFloatingScore.h"
#include "Components/TextRenderComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/CameraComponent.h"
#include "Kismet/KismetMathLibrary.h" 

APcFloatingScore::APcFloatingScore()
{
	PrimaryActorTick.bCanEverTick = true;

	RootScene = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = RootScene;

	TextComp = CreateDefaultSubobject<UTextRenderComponent>(TEXT("TextComp"));
	TextComp->SetupAttachment(RootScene);
	
	// Default Visuals
	TextComp->SetText(FText::FromString(TEXT("100")));
	TextComp->SetTextRenderColor(FColor::Cyan);
	TextComp->SetHorizontalAlignment(EHorizTextAligment::EHTA_Center);
	TextComp->SetVerticalAlignment(EVerticalTextAligment::EVRTA_TextCenter);
	TextComp->SetWorldSize(60.0f);
	
	SetActorScale3D(FVector::ZeroVector);
}

void APcFloatingScore::BeginPlay()
{
	Super::BeginPlay();
}

void APcFloatingScore::InitializeScore(float ScoreValue, FVector HitLocation)
{
	SetActorLocation(HitLocation);
	
	FString ScoreStr = FString::Printf(TEXT("%.0f"), ScoreValue);
	TextComp->SetText(FText::FromString(ScoreStr));

	if (ScoreValue > 500) TextComp->SetTextRenderColor(FColor::Red);
	else if (ScoreValue > 100) TextComp->SetTextRenderColor(FColor::Yellow);
	else TextComp->SetTextRenderColor(FColor::Cyan);

	// --- ARC TRAJECTORY LOGIC ---
	FVector RandomDir = FMath::VRand();
	
	// Strong Bias Up (Z) to ensure it pops UP like a fountain
	RandomDir.Z = FMath::Abs(RandomDir.Z) + 0.6f; 
	RandomDir.Normalize();

	// Apply Impulse
	float Speed = FMath::RandRange(MinImpulse, MaxImpulse);
	CurrentVelocity = RandomDir * Speed;

	bInitialized = true;
}

void APcFloatingScore::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!bInitialized) return;

	TimeAlive += DeltaTime;
	if (TimeAlive >= LifeTime)
	{
		Destroy();
		return;
	}

	// 1. APPLY GRAVITY (Creates the Arc)
	CurrentVelocity.Z -= SimulatedGravity * DeltaTime;

	// 2. APPLY DRAG (Stops it from flying too far horizontally)
	float FrictionFactor = 1.0f - (Drag * DeltaTime);
	FrictionFactor = FMath::Clamp(FrictionFactor, 0.0f, 1.0f);
	
	// Apply drag to X/Y strongly, less to Z (so it falls properly but stops moving sideways)
	CurrentVelocity.X *= FrictionFactor;
	CurrentVelocity.Y *= FrictionFactor;
	// We let gravity handle Z mostly, but a little drag helps terminal velocity
	CurrentVelocity.Z *= FMath::Max(0.9f, FrictionFactor); 

	// 3. MOVE
	AddActorWorldOffset(CurrentVelocity * DeltaTime);

	// 4. FACE CAMERA
	if (APlayerCameraManager* CamMgr = UGameplayStatics::GetPlayerCameraManager(this, 0))
	{
		FVector Start = GetActorLocation();
		FVector Target = CamMgr->GetCameraLocation();
		FRotator LookAt = (Target - Start).Rotation();
		SetActorRotation(LookAt);
	}

	// 5. ELASTIC BOUNCE SCALE
	float Pct = TimeAlive / LifeTime;
	FVector CurrentScale = FVector::ZeroVector;

	if (Pct < 0.2f) 
	{
		// 0% -> 20%: Fast Elastic Pop
		float PopAlpha = Pct / 0.2f;
		float ElasticVal = UKismetMathLibrary::Ease(0.0f, 1.0f, PopAlpha, EEasingFunc::EaseOut);
		CurrentScale = FMath::Lerp(StartScale, PeakScale, ElasticVal);
	}
	else if (Pct < 0.8f)
	{
		// 20% -> 80%: Slowly settle
		float SettleAlpha = (Pct - 0.2f) / 0.6f;
		CurrentScale = FMath::Lerp(PeakScale, EndScale, SettleAlpha);
	}
	else
	{
		// 80% -> 100%: Fade out
		float FadeAlpha = (Pct - 0.8f) / 0.2f;
		CurrentScale = FMath::Lerp(EndScale, FVector::ZeroVector, FadeAlpha);
	}

	SetActorScale3D(CurrentScale);
}