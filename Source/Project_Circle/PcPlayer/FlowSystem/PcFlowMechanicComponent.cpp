// ==========================================
// FILE: PcFlowMechanicComponent.cpp
// PATH: E:\GameDev\Unreal Engine Projects\Project_Circle\Source\Project_Circle\PcPlayer\PcFlowMechanicComponent.cpp
// ==========================================
#include "PcFlowMechanicComponent.h"

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
		DecayDelayTimer = DecayDelay; // Reset timer while charging
	}
	else if (CurrentTier > 0 || FlowPercent > 0.0f) 
	{
		// Check Delay
		if (DecayDelayTimer > 0.0f)
		{
			DecayDelayTimer -= DeltaTime;
			CurrentState = EFlowState::Stable; // Holding steady
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

	if (bIsDrifting)
	{
		// Charge is handled via explicit InjectFlow calls from Physics
	}
	else
	{
		// Only decay if timer ran out
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
	
	// Reset decay timer whenever we get flow
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

void UPcFlowMechanicComponent::ApplyJumpBonus()
{
	float Bonus = 15.0f; 
	FlowPercent = FMath::Clamp(FlowPercent + Bonus, 0.0f, 100.0f);
	DecayDelayTimer = DecayDelay; // Reset delay on jump too
	
	if (FlowPercent >= 100.0f)
	{
		PromoteTier();
	}
}

void UPcFlowMechanicComponent::PromoteTier()
{
	if (CurrentTier < MaxTiers)
	{
		CurrentTier++;
		FlowPercent = 5.0f; // Sink or Swim
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