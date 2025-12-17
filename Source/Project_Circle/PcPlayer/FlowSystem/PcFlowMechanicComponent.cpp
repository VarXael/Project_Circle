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
	// Formula: Base * (1 + Multiplier)
	// Example: 10 * (1 + 0.5) = 15 points
	float FinalPoints = BasePoints * (1.0f + CurrentMultiplier);
	CurrentScore += FinalPoints;
}

void UPcFlowMechanicComponent::IncreaseMultiplier(float Amount)
{
	// NOTE: We ignore the passed amount usually and use a fixed step to control inflation
	// Or we use the amount but scale it down.
	// Let's assume the passed amount is "1.0" for a kill/hit. 
	// We divide by 10 to make the multiplier grow by 0.1 per hit.
	
	CurrentMultiplier += (Amount * 0.1f);
}

void UPcFlowMechanicComponent::ResetMultiplier()
{
	CurrentMultiplier = 0.0f;
}

void UPcFlowMechanicComponent::TriggerHitStop(UWorld* WorldContext)
{
	if (!WorldContext) return;
	UGameplayStatics::SetGlobalTimeDilation(WorldContext, 0.05f);
	WorldContext->GetTimerManager().SetTimer(TimerHandle_HitStop, this, &UPcFlowMechanicComponent::ResetTimeDilation, 0.01f, false);
}

void UPcFlowMechanicComponent::ResetTimeDilation()
{
	if (GetWorld()) UGameplayStatics::SetGlobalTimeDilation(GetWorld(), 1.0f);
}