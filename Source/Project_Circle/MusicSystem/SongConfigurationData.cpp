// --- START OF FILE SongConfigurationData.cpp ---

#include "SongConfigurationData.h"

// We only compile this code in the editor.
#if WITH_EDITOR

// --- Add all necessary includes for the generation logic ---
#include "URhythmAnalysisProfile.h" // To run the analysis
#include "Engine/DataTable.h"
#include "Factories/DataTableFactory.h"
#include "Modules/ModuleManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Misc/PackageName.h"
#include "Editor.h"

// Define a local LOCTEXT_NAMESPACE for our notifications
#define LOCTEXT_NAMESPACE "SongConfigurationData"

void USongConfigurationData::GenerateRhythmAssets()
{
	// --- STEP 1: Run the analysis to get the data to export ---
	UURhythmAnalysisProfile* AnalysisProfile = UURhythmAnalysisProfile::RunSongAnalysis(this, this);

	if (!AnalysisProfile)
	{
		UE_LOG(LogTemp, Error, TEXT("Analysis failed. Cannot generate assets. Check the logs for details."));
		FNotificationInfo Info(LOCTEXT("AnalysisFailed", "Analysis Failed. Cannot generate assets."));
		Info.ExpireDuration = 5.0f;
		FSlateNotificationManager::Get().AddNotification(Info);
		return;
	}

	// Get the sections array from the analysis result.
	const TArray<FRhythmSectionProfile>& SectionsToExport = AnalysisProfile->GetRhythmSections();

	// --- STEP 2: The asset generation logic (copied and adapted from RhythmDataGenerator) ---
	
	// --- 2a. VALIDATION ---
	// Note: We use 'this' instead of a 'SongConfig' parameter
	if (!this->GameplayMap) 
	{
		UE_LOG(LogTemp, Error, TEXT("GenerateRhythmAssets: Cannot generate. GameplayMap is not set in this asset."));
		return;
	}
	if (SectionsToExport.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("GenerateRhythmAssets: Analysis produced no sections. No Rhythm Profile DataTable will be created."));
        // We might still want to create the Note Data Table, so we don't return here.
	}

	FAssetToolsModule& AssetToolsModule = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools");
	const FString SourceAssetPath = this->GetPathName();
	FString DefaultSavePath = FPackageName::GetLongPackagePath(SourceAssetPath);

	// --- 2b. CREATE THE RHYTHM DATATABLE (First Dialog) ---
	UDataTable* NewRhythmTable = nullptr;
	if (SectionsToExport.Num() > 0) // Only create the rhythm table if there's data for it
	{
		UDataTableFactory* RhythmDataTableFactory = NewObject<UDataTableFactory>();
		RhythmDataTableFactory->Struct = FRhythmSectionProfile::StaticStruct();

		// Use the Data Asset's name for a better default file name
		FString RhythmDefaultSaveName = FString::Printf(TEXT("DT_%s_RhythmProfile"), *this->GetName());

		UObject* NewRhythmAsset = AssetToolsModule.Get().CreateAssetWithDialog(
			RhythmDefaultSaveName,
			DefaultSavePath,
			UDataTable::StaticClass(),
			RhythmDataTableFactory
		);
	
		NewRhythmTable = Cast<UDataTable>(NewRhythmAsset);

		if (!NewRhythmTable)
		{
			UE_LOG(LogTemp, Log, TEXT("Rhythm Profile creation was cancelled by the user. Aborting generation."));
			return; // If the user cancels the first dialog, stop everything.
		}

		// --- POPULATE THE RHYTHM DATATABLE ---
		for (const FRhythmSectionProfile& Section : SectionsToExport)
		{
			const FName RowName = FName(*FString::Printf(TEXT("%d"), Section.StartTimeMS));
			NewRhythmTable->AddRow(RowName, Section);
		}
		NewRhythmTable->MarkPackageDirty();
		FAssetRegistryModule::AssetCreated(NewRhythmTable);
	}
	
	// --- 2c. CREATE THE NOTE DATA TABLE (Second Dialog) ---
	UDataTable* NewNoteTable = nullptr;
	UDataTable* SourceNoteTable = this->GameplayMap;

	UDataTableFactory* NoteDataTableFactory = NewObject<UDataTableFactory>();
	NoteDataTableFactory->Struct = SourceNoteTable->GetRowStruct();
	
	FString NoteDefaultSaveName = FString::Printf(TEXT("DT_%s_NoteData"), *this->GetName());

	UObject* NewNoteAsset = AssetToolsModule.Get().CreateAssetWithDialog(
		NoteDefaultSaveName,
		DefaultSavePath,
		UDataTable::StaticClass(),
		NoteDataTableFactory
	);

	NewNoteTable = Cast<UDataTable>(NewNoteAsset);
	
	if (NewNoteTable)
	{
		// --- POPULATE THE NEW NOTE DATA TABLE (Manually Copying Rows) ---
		const TMap<FName, uint8*>& RowMap = SourceNoteTable->GetRowMap();
		for (auto RowIt = RowMap.CreateConstIterator(); RowIt; ++RowIt)
		{
			NewNoteTable->AddRow(RowIt.Key(), *reinterpret_cast<FTableRowBase*>(RowIt.Value()));
		}

		NewNoteTable->MarkPackageDirty();
		FAssetRegistryModule::AssetCreated(NewNoteTable);
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("Note Data Table creation was cancelled by the user."));
	}

	// --- 2d. FINALIZE AND NOTIFY ---
	FNotificationInfo Info(LOCTEXT("RhythmDataAssetsGenerated", "Successfully generated Rhythm and Note data assets."));
	Info.ExpireDuration = 5.0f;
	FSlateNotificationManager::Get().AddNotification(Info);

	if (GEditor)
	{
		TArray<UObject*> ObjectsToSync;
		if (NewRhythmTable)
		{
			ObjectsToSync.Add(NewRhythmTable);
		}
		if (NewNoteTable)
		{
			ObjectsToSync.Add(NewNoteTable);
		}
		
		if (ObjectsToSync.Num() > 0)
		{
			GEditor->SyncBrowserToObjects(ObjectsToSync);
		}
	}
}

// Undefine the local namespace
#undef LOCTEXT_NAMESPACE

#endif // WITH_EDITOR

// --- END OF FILE ---