#include "PcMusicConfigurationData.h"

#if WITH_EDITOR

#include "PcMusicAnalyzer.h" // Your analysis header
#include "Engine/DataTable.h"
#include "Factories/DataTableFactory.h"
#include "Modules/ModuleManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "IAssetTools.h"
#include "AssetToolsModule.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Misc/PackageName.h"
#include "Editor.h"

#define LOCTEXT_NAMESPACE "SongConfigurationData"

void UPcMusicConfigurationData::GenerateRhythmAssets()
{
	// --- STEP 1: Run analysis ---
	UPcMusicAnalyzer* AnalysisProfile = UPcMusicAnalyzer::RunSongAnalysis(this, this);
	if (!AnalysisProfile)
	{
		UE_LOG(LogTemp, Error, TEXT("Analysis failed. Cannot generate assets."));
		return;
	}
	
	const TArray<FPcMusicGameplayEvents>& SectionsToExport = AnalysisProfile->GetRhythmSections();

	// --- STEP 2: Validation and Setup ---
	if (!this->MusicGameplayNotesProfile) 
	{
		UE_LOG(LogTemp, Error, TEXT("GenerateRhythmAssets: The source 'MusicGameplayNotesProfile' DataTable is not set. Cannot proceed."));
		return;
	}
    // IMPORTANT: Additional validation to prevent crashes
    if (!this->MusicGameplayNotesProfile->GetRowStruct())
    {
        UE_LOG(LogTemp, Error, TEXT("GenerateRhythmAssets: The source 'MusicGameplayNotesProfile' DataTable has no Row Struct assigned. Cannot proceed."));
        return;
    }

	// Clear out old generated assets
	GeneratedMusicEventsProfile = nullptr;
	GeneratedMusicNotesProfile = nullptr;

	FAssetToolsModule& AssetToolsModule = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools");
	IAssetTools& AssetTools = AssetToolsModule.Get();
	const FString SourceAssetPath = this->GetPathName();
	FString DefaultSavePath = FPackageName::GetLongPackagePath(SourceAssetPath);
	TArray<UObject*> GeneratedAssetsToSave;

	// --- Create the RHYTHM PROFILE DataTable ---
	if (SectionsToExport.Num() > 0)
	{
		UDataTableFactory* RhythmDataTableFactory = NewObject<UDataTableFactory>();
		RhythmDataTableFactory->Struct = FPcMusicGameplayEvents::StaticStruct();

		FString RhythmDefaultSaveName = FString::Printf(TEXT("DT_%s_GameplayEventsProfile"), *SongName.ToString());

		UObject* NewRhythmAsset = AssetTools.CreateAssetWithDialog(
			RhythmDefaultSaveName, DefaultSavePath, UDataTable::StaticClass(), RhythmDataTableFactory
		);
		UDataTable* NewRhythmTable = Cast<UDataTable>(NewRhythmAsset);

		if (!NewRhythmTable)
		{
			UE_LOG(LogTemp, Log, TEXT("Rhythm Profile creation was cancelled by the user. Aborting."));
			MarkPackageDirty(); 
			return; // Exit if user cancels the first dialog
		}
		
		for (const FPcMusicGameplayEvents& Section : SectionsToExport)
		{
			const FName RowName = FName(*FString::Printf(TEXT("%d"), Section.StartTimeMS));
			NewRhythmTable->AddRow(RowName, Section);
		}
		FAssetRegistryModule::AssetCreated(NewRhythmTable);
		GeneratedMusicEventsProfile = NewRhythmTable;
		GeneratedAssetsToSave.Add(NewRhythmTable);
	}
	
	// --- **THE FIX**: Duplicate the NOTE DATA DataTable instead of copying row-by-row ---
	FString NoteDefaultSaveName = FString::Printf(TEXT("DT_%s_GameplayNotesProfile"), *SongName.ToString());
    
    // Use DuplicateAssetWithDialog which is safer and simpler
    UObject* NewNoteAsset = AssetTools.DuplicateAssetWithDialog(
        NoteDefaultSaveName, DefaultSavePath, this->MusicGameplayNotesProfile
    );

	UDataTable* NewNoteTable = Cast<UDataTable>(NewNoteAsset);
	if (NewNoteTable)
	{
		// The asset is already a perfect copy, no need to manually add rows!
		FAssetRegistryModule::AssetCreated(NewNoteTable);
		GeneratedMusicNotesProfile = NewNoteTable;
		GeneratedAssetsToSave.Add(NewNoteTable);
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("Note Data Table creation was cancelled by the user."));
	}

	// --- Finalize: Mark dirty and notify ---
	this->MarkPackageDirty();

	FNotificationInfo Info(LOCTEXT("RhythmDataAssetsGenerated", "Successfully generated Rhythm and Note data assets. Please save THIS configuration asset!"));
	Info.ExpireDuration = 8.0f;
	FSlateNotificationManager::Get().AddNotification(Info);

	if (GEditor && GeneratedAssetsToSave.Num() > 0)
	{
		GEditor->SyncBrowserToObjects(GeneratedAssetsToSave);
	}
}

#undef LOCTEXT_NAMESPACE

#endif // WITH_EDITOR