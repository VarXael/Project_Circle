#include "PcBeatSurfaceComponent.h"
#include "Project_Circle/MusicSystem/MusicGameplaySystem/PcMusicAnalysisSubsystem.h"

UPcBeatSurfaceComponent::UPcBeatSurfaceComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UPcBeatSurfaceComponent::BeginPlay()
{
	Super::BeginPlay();

	if (UPcMusicAnalysisSubsystem* Sub = GetWorld()->GetSubsystem<UPcMusicAnalysisSubsystem>())
		Sub->OnGameplayBeatTriggered.AddDynamic(this, &UPcBeatSurfaceComponent::OnGameplayBeat);
}

// Beat fires — set the flag.  The player polls this at their own pace.
void UPcBeatSurfaceComponent::OnGameplayBeat(float /*BeatTimestamp*/)
{
	bJustPulsed = true;
	PulseTimer  = PulseWindowSec;
}

// Count the pulse window down.  Auto-reset if not consumed.
void UPcBeatSurfaceComponent::TickComponent(float DeltaTime, ELevelTick TickType,
											 FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bJustPulsed)
	{
		PulseTimer -= DeltaTime;
		if (PulseTimer <= 0.f) ConsumePulse();
	}
}