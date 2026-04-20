#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PcBeatSurfaceComponent.generated.h"

// What kind of surface this is.
// The player movement component reads this to decide how to react.
UENUM(BlueprintType)
enum class EPcBeatSurfaceType : uint8
{
	Floor  UMETA(DisplayName = "Floor  (vertical launch)"),
	Wall   UMETA(DisplayName = "Wall   (hold + eject on beat)"),
	Boost  UMETA(DisplayName = "Boost  (horizontal speed kick)")
};

// =============================================================================
//  UPcBeatSurfaceComponent
//
//  Drop this on any actor to make it pulse to the beat.
//
//  Every gameplay beat, the component sets bJustPulsed = true for
//  PulseWindowSec seconds, then resets it automatically.  That's all it does.
//
//  The player movement component samples CurrentFloor.HitResult.GetActor()
//  every tick and checks WantsToLaunch() — no subscriptions, no overlap
//  volumes, no delegates from world to player.  The surface leaves a flag out.
//  The player reads it when they're ready.
//
//  Call ConsumePulse() after reading so the flag resets immediately and
//  doesn't trigger twice (e.g. if tick runs faster than the pulse window).
// =============================================================================

UCLASS(ClassGroup=(Custom), Blueprintable, BlueprintType,
       meta=(BlueprintSpawnableComponent,
             DisplayName="Beat Surface"))
class PROJECT_CIRCLE_API UPcBeatSurfaceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPcBeatSurfaceComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;

	// What kind of beat surface this is — read by the movement component.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beat Surface")
	EPcBeatSurfaceType SurfaceType = EPcBeatSurfaceType::Floor;

	// How long (seconds) bJustPulsed stays true after a beat.
	// Default 0.08s = 80ms window for the player tick to catch it.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beat Surface")
	float PulseWindowSec = 0.08f;

	// Floor: override launch Z velocity.  0 = let the player preset decide.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beat Surface|Floor",
	          meta = (EditCondition = "SurfaceType == EPcBeatSurfaceType::Floor"))
	float FloorLaunchOverride = 0.f;

	// Wall: horizontal eject speed.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beat Surface|Wall",
	          meta = (EditCondition = "SurfaceType == EPcBeatSurfaceType::Wall"))
	float WallEjectSpeed = 1600.f;

	// Wall: small upward kick on eject so the player clears the surface.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beat Surface|Wall",
	          meta = (EditCondition = "SurfaceType == EPcBeatSurfaceType::Wall"))
	float WallEjectUpKick = 250.f;

	// Boost: horizontal speed added when the player slides over this surface.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Beat Surface|Boost",
	          meta = (EditCondition = "SurfaceType == EPcBeatSurfaceType::Boost"))
	float BoostSpeed = 800.f;

	// --- Read interface ---

	// Returns true if a beat fired recently and hasn't been consumed yet.
	UFUNCTION(BlueprintPure) bool WantsToLaunch() const { return bJustPulsed; }

	// Call after reading WantsToLaunch() so it doesn't fire again this window.
	UFUNCTION(BlueprintCallable) void ConsumePulse() { bJustPulsed = false; PulseTimer = 0.f; }

private:
	UFUNCTION() void OnGameplayBeat(float BeatTimestamp);

	bool  bJustPulsed = false;
	float PulseTimer  = 0.f;
};