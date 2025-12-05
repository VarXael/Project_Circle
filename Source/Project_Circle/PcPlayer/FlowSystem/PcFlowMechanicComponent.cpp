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
	// Logic is handled via UpdateFlowLogic called by the Player to ensure sync order
}

void UPcFlowMechanicComponent::UpdateFlowLogic(float DeltaTime, bool bIsDrifting)
{
	if (CurrentState == EFlowState::Frozen) return;

	// 1. DETERMINE STATE
	if (bIsDrifting) CurrentState = EFlowState::Charging;
	else if (CurrentTier > 0 || FlowPercent > 0.0f) CurrentState = EFlowState::Draining;
	else CurrentState = EFlowState::Stable;

	// 2. HANDLE PANIC (0% Logic)
	if (FlowPercent <= 0.0f && CurrentTier > 0)
	{
		CurrentState = EFlowState::Panic;
		PanicTimer -= DeltaTime;
		
		// Rescue!
		if (bIsDrifting) 
		{
			InjectFlow(5.0f * DeltaTime); // Slow recharge from panic
			PanicTimer = PanicDuration; // Reset timer
		}
		else if (PanicTimer <= 0.0f)
		{
			DemoteTier();
		}
		return;
	}

	// 3. NORMAL LOGIC
	PanicTimer = PanicDuration; // Reset panic if we have flow

	if (bIsDrifting)
	{
		// Charge logic is usually handled by InjectFlow calls from physics, 
		// but we can have a passive trickle here if desired. 
		// For now, we rely on physics to call InjectFlow based on Angle.
	}
	else
	{
		// DECAY
		// Higher tiers decay faster
		float TierMult = 1.0f + (CurrentTier * 0.5f);
		float Drop = BaseDecayRate * TierMult * DeltaTime;
		FlowPercent = FMath::Clamp(FlowPercent - Drop, 0.0f, 100.0f);
	}
}

void UPcFlowMechanicComponent::InjectFlow(float Amount)
{
	if (CurrentState == EFlowState::Frozen) return;
	
	FlowPercent += Amount;
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
	// Instead of TIER UP, we just give fuel.
	// We do NOT check for frozen here, because we want this to happen 
	// exactly when the jump starts.
	
	float Bonus = 15.0f; // Give 15% bar instantly
	FlowPercent = FMath::Clamp(FlowPercent + Bonus, 0.0f, 100.0f);
	
	// If this bonus pushes us over 100%, we promote naturally
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
		FlowPercent = 5.0f; // THE 5% RULE. Sink or swim.
		// Visual FX/Sound should trigger here
	}
	else
	{
		FlowPercent = 100.0f; // Cap at max
	}
}

void UPcFlowMechanicComponent::DemoteTier()
{
	if (CurrentTier > 0)
	{
		CurrentTier--;
		FlowPercent = 50.0f; // Grace buffer in lower tier
	}
}