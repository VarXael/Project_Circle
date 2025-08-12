// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GameplayTagContainer.h"
#include "Project_Circle/MusicSystem/MusicImportSystem/PcMusicAnalysisTypes.h"
#include "MusicActionInstance.generated.h"

class APcMusicGameplayManager;
class UPcMusicDirectorSubsystem;
class UMusicAction;

/**
 * The Blueprintable "Brain" for a single musical note. This lightweight UObject is created
 * for each note that enters the gameplay window. Its Blueprint child class contains the
 * entire logical lifecycle of the note (what to do on Prepare, Hit, Miss, and any custom events).
 */
UCLASS(Blueprintable)
class PROJECT_CIRCLE_API UMusicActionInstance : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * [Blueprint] This event is the "BeginPlay" for the note instance.
	 * It fires once after the instance has been created and initialized with its data.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Music Action Instance", meta = (DisplayName = "On Initialized"))
	void K2_OnBeginInitialization();

	/**
	 * [Blueprint] This event fires exactly ONCE when the song's current time reaches this note's StartTimeMS.
	 * This is the primary event for "On Hit Time" logic.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Music Action Instance", meta = (DisplayName = "On Execution"))
	void K2_OnExecution();

	/**
	 * [Blueprint] This event fires after K2_OnCue gets called.
	 * It is used as a way to override in blueprints the C++ Deactivate function default behaviour.
	 * If this function gets overridden, remember to call Deactivate once done with it!
	 */
	UFUNCTION(BlueprintNativeEvent, Category = "Music Action Instance", meta = (DisplayName = "On Execution Completed"))
	void K2_OnExecutionCompleted();

	/**
	 * [Blueprint] This event is a clean up event.
	 * It's default behaviour is to destroy this music action instance once the timestamp has been reached and unbind any delegates.
	 */
	UFUNCTION(BlueprintCallable, Category = "Music Action Instance")
	void Deactivate();

	/**
	 * [Blueprint] This event is the "Tick" for the note instance.
	 * It is called every frame by the Director, providing the current song time.
	 * @param CurrentTimeMs The precise current time of the song in milliseconds.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "Music Action Instance", meta = (DisplayName = "On Music Tick"))
	void K2_OnMusicTick(float CurrentTimeMs);


	/**
	 * [C++] Initializes the instance with its core data. Called by the Director immediately after creation.
	 * This then calls the K2_OnInitialized event to pass control to the Blueprint graph.
	 * @param InNoteData The specific data for this note (timing, etc.) from the master DataTable.
	 * @param InOwnerManager The gameplay manager actor, which provides world context.
	 * @param InDirector
	 */
	void Initialize(const FPcMusicGameplayNotes& InNoteData, APcMusicGameplayManager* InOwnerManager, UPcMusicDirectorSubsystem* InDirector);
	
	/**
	 * [C++] This event is the "Tick" for the note instance.
	 * It links itself to the delegate "OnMusicTick" of the UPcMusicDirectorSubsystem inside the Initialize function.
	 */
	UFUNCTION()
	void MusicTick(float CurrentTimeMs);
	
	/**
	 * [BlueprintCallable] Executes a UMusicAction from the note's Action Library by its tag.
	 * This is the primary way for the Blueprint graph to trigger world events like spawning actors.
	 * @param ActionTag The tag identifying the action to execute from the ActionSet's library.
	 */
	UFUNCTION(BlueprintCallable, Category = "Music Action Instance")
	void ExecuteMusicActionByTag(FGameplayTag ActionTag);

	/**
	 * [Blueprint Read/Write] If true, this will call K2_OnMusicTick every frame.
	 * Keep it false for simple, fire-and-forget notes to improve performance.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Music Action Instance|Ticking")
	bool bShouldPerformMusicTick = false;

	// --- Blueprint-Accessible Getters ---
	
	UFUNCTION(BlueprintPure, Category = "Music Action Instance")
	const FPcMusicGameplayNotes& GetNoteData() const
	{
		return NoteData;
	}
	
	UFUNCTION(BlueprintPure, Category = "Music Action Instance")
	APcMusicGameplayManager* GetOwnerManager() const
	{
		return OwnerManager;
	}

	virtual UWorld* GetWorld() const override;
	virtual void BeginDestroy() override;

private:
	// --- Core Data & References ---

	/** A copy of the specific data for this note, taken from the master DataTable. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "State", meta = (AllowPrivateAccess = "true"))
	FPcMusicGameplayNotes NoteData;
	
	/** A pointer to the central gameplay manager, which provides world context. */
	UPROPERTY()
	TObjectPtr<APcMusicGameplayManager> OwnerManager;

	UPROPERTY()
	TWeakObjectPtr<UPcMusicDirectorSubsystem> DirectorSubsystem;

	bool bHasFiredExecution = false;
};