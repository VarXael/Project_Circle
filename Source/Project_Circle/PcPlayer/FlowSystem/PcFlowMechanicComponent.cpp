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
	CurrentTier = 0;
	FlowPercent = 0.0f;
}

void UPcFlowMechanicComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void UPcFlowMechanicComponent::UpdateFlowLogic(float DeltaTime, bool bIsDrifting)
{
	if (CurrentState == EFlowState::Frozen) return;

	// 1. DETERMINE STATE
	if (bIsDrifting) 
	{
		CurrentState = EFlowState::Charging;
		DecayDelayTimer = DecayDelay; 
	}
	else if (CurrentTier > 0 || FlowPercent > 0.0f) 
	{
		if (DecayDelayTimer > 0.0f)
		{
			DecayDelayTimer -= DeltaTime;
			CurrentState = EFlowState::Stable; 
		}
		else
		{
			CurrentState = EFlowState::Draining;
		}
	}
	else 
	{
		CurrentState = EFlowState::Stable;
	}

	// 2. HANDLE PANIC
	if (FlowPercent <= 0.0f && CurrentTier > 0)
	{
		CurrentState = EFlowState::Panic;
		PanicTimer -= DeltaTime;
		
		if (bIsDrifting) 
		{
			InjectFlow(5.0f * DeltaTime); 
			PanicTimer = PanicDuration; 
		}
		else if (PanicTimer <= 0.0f)
		{
			DemoteTier();
		}
		return;
	}

	// 3. NORMAL LOGIC
	PanicTimer = PanicDuration; 

	if (!bIsDrifting)
	{
		if (DecayDelayTimer <= 0.0f)
		{
			float TierMult = 1.0f + (CurrentTier * 0.5f);
			float Drop = BaseDecayRate * TierMult * DeltaTime;
			FlowPercent = FMath::Clamp(FlowPercent - Drop, 0.0f, 100.0f);
		}
	}
}

void UPcFlowMechanicComponent::InjectFlow(float Amount)
{
	if (CurrentState == EFlowState::Frozen) return;
	
	FlowPercent += Amount;
	DecayDelayTimer = DecayDelay;

	if (FlowPercent >= 100.0f)
	{
		PromoteTier();
	}
}

void UPcFlowMechanicComponent::SetFrozen(bool bFreeze)
{
	CurrentState = bFreeze ? EFlowState::Frozen : EFlowState::Draining;
}

void UPcFlowMechanicComponent::ForceTierUp()
{
	if (CurrentTier < MaxTiers)
	{
		CurrentTier++;
		FlowPercent = 10.0f; // Start fresh in new tier
	}
	else
	{
		FlowPercent = 100.0f; 
	}
	CurrentState = EFlowState::Stable;
	DecayDelayTimer = DecayDelay;
}

bool UPcFlowMechanicComponent::ApplyDamage()
{
	// If we have tiers to lose, lose one
	if (CurrentTier > 0)
	{
		CurrentTier--;
		FlowPercent = 99.0f; // Top of the lower tier (Second chance)
		DecayDelayTimer = DecayDelay;
		return false; // Did not wipeout
	}

	// If at Tier 0, we die/wipeout
	return true; 
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

void UPcFlowMechanicComponent::PromoteTier()
{
	if (CurrentTier < MaxTiers)
	{
		CurrentTier++;
		FlowPercent = 5.0f; 
	}
	else
	{
		FlowPercent = 100.0f; 
	}
}

void UPcFlowMechanicComponent::DemoteTier()
{
	if (CurrentTier > 0)
	{
		CurrentTier--;
		FlowPercent = 50.0f; 
	}
}