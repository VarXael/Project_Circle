// ==========================================
// FILE: PcFlowMechanicComponent.cpp
// PATH: Source/Project_Circle/PcPlayer/FlowSystem/PcFlowMechanicComponent.cpp
// ==========================================
#include "PcFlowMechanicComponent.h"
#include "Kismet/GameplayStatics.h"

UPcFlowMechanicComponent::UPcFlowMechanicComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UPcFlowMechanicComponent::BeginPlay()
{
	Super::BeginPlay();
	CurrentCharge = 0.0f; 
	bInOverdrive = false;
	CurrentScore = 0.0f;
	CurrentMultiplier = 0.0f;
}

void UPcFlowMechanicComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	
	// Check Overdrive State
	bInOverdrive = (CurrentCharge >= OverdriveThreshold);
}

bool UPcFlowMechanicComponent::TrySpendCharge(float Amount)
{
	if (CurrentCharge >= Amount)
	{
		CurrentCharge -= Amount;
		return true;
	}
	return false;
}

void UPcFlowMechanicComponent::AddCharge(float Amount)
{
	CurrentCharge = FMath::Clamp(CurrentCharge + Amount, 0.0f, MaxCharge);
}

void UPcFlowMechanicComponent::AddScore(float BasePoints)
{
	// Formula: Points * (1 + Multiplier). 
	// If Multiplier is 0, we just get base points.
	// If Multiplier is 5, we get Points * 6.
	float FinalPoints = BasePoints * (1.0f + CurrentMultiplier);
	CurrentScore += FinalPoints;
}

void UPcFlowMechanicComponent::IncreaseMultiplier(float Amount)
{
	CurrentMultiplier += Amount;
}

void UPcFlowMechanicComponent::ResetMultiplier()
{
	CurrentMultiplier = 0.0f;
}

void UPcFlowMechanicComponent::TriggerHitStop(UWorld* WorldContext)
{
	if (!WorldContext) return;
    
	// Freeze time to 5%
	UGameplayStatics::SetGlobalTimeDilation(WorldContext, 0.05f);
    
	// Schedule unfreeze (0.01s dilated = ~0.2s real time)
	WorldContext->GetTimerManager().SetTimer(TimerHandle_HitStop, this, &UPcFlowMechanicComponent::ResetTimeDilation, 0.01f, false);
}

void UPcFlowMechanicComponent::ResetTimeDilation()
{
	if (GetWorld()) UGameplayStatics::SetGlobalTimeDilation(GetWorld(), 1.0f);
}