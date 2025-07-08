#pragma once

#include "CoreMinimal.h"
#include "EditorUtilityObject.h"
#include "MusicAnalysisTypes.h" // We'll need FGameplayRhythmSection
#include "RhythmDataGenerator.generated.h"

class USongConfigurationData;
class UDataTable;

/**
 * A collection of editor utility functions to process song analysis data
 * and generate new assets from it.
 */
UCLASS()
class PROJECT_CIRCLE_API URhythmDataGenerator : public UEditorUtilityObject
{
	GENERATED_BODY()

public:
	/**
	 * Takes an array of rhythm sections and generates a new DataTable asset from them.
	 * Prompts the user for a save location with an intelligently suggested path and name.
	 *
	 * @param SongConfig The source configuration asset, used to generate a default path.
	 * @param SectionsToExport The array of section data to populate the DataTable with.
	 */
	UFUNCTION(BlueprintCallable, Category = "Rhythm Data Generator")
	void GenerateRhythmDataTable(USongConfigurationData* SongConfig, const TArray<FGameplayRhythmSection>& SectionsToExport);
};