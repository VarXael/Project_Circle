#include "PcFlowMechanicComponent.h"
#include "Kismet/GameplayStatics.h"

UPcFlowMechanicComponent::UPcFlowMechanicComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UPcFlowMechanicComponent::BeginPlay()
{
	Super::BeginPlay();
	CurrentCharge = 0.0f; // Start with "Walk of Shame"
	bInOverdrive = false;
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