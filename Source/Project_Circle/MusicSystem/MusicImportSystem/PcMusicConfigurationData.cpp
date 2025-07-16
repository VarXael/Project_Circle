// --- START OF FILE SongConfigurationData.cpp ---

#include "PcMusicConfigurationData.h"

#if WITH_EDITOR

#include "PcMusicAnalyzer.h"
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
#include "PackageTools.h"

#define LOCTEXT_NAMESPACE "SongConfigurationData"

void UPcMusicConfigurationData::GenerateRhythmAssets()
{
	// --- STEP 1: Run analysis ---
	UPcMusicAnalyzer* AnalysisProfile = UPcMusicAnalyzer::RunSongAnalysis(this, this);

	if (!AnalysisProfile)
	{
		UE_LOG(LogTemp, Error, TEXT("Analysis failed. Cannot generate assets."));
		// ... (Notification logic)
		return;
	}
	
	const TArray<FPcRhythmSectionProfile>& SectionsToExport = AnalysisProfile->GetRhythmSections();

	// --- STEP 2: Validation and Setup ---
	if (!this->GameplayMap) 
	{
		UE_LOG(LogTemp, Error, TEXT("GenerateRhythmAssets: GameplayMap is not set."));
		return;
	}
	// ... (other setup)
	GeneratedRhythmProfile = nullptr;
	GeneratedNoteData = nullptr;

	FAssetToolsModule& AssetToolsModule = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools");
	IAssetTools& AssetTools = AssetToolsModule.Get();
	const FString SourceAssetPath = this->GetPathName();
	FString DefaultSavePath = FPackageName::GetLongPackagePath(SourceAssetPath);
	TArray<UObject*> GeneratedAssetsToSave;

	// --- Create the RHYTHM PROFILE DataTable (First Dialog) ---
	UDataTable* NewRhythmTable = nullptr;
	if (SectionsToExport.Num() > 0)
	{
		UDataTableFactory* RhythmDataTableFactory = NewObject<UDataTableFactory>();
		RhythmDataTableFactory->Struct = FPcRhythmSectionProfile::StaticStruct();

		// NAME FOR THE FIRST DIALOG: Based on "RhythmProfile"
		FString RhythmDefaultSaveName = FString::Printf(TEXT("DT_%s_RhythmProfile"), *SongName.ToString());

		UObject* NewRhythmAsset = AssetTools.CreateAssetWithDialog(
			RhythmDefaultSaveName, DefaultSavePath, UDataTable::StaticClass(), RhythmDataTableFactory
		);
		NewRhythmTable = Cast<UDataTable>(NewRhythmAsset);

		if (!NewRhythmTable)
		{
			UE_LOG(LogTemp, Log, TEXT("Rhythm Profile creation was cancelled by the user. Aborting."));
			MarkPackageDirty(); 
			return;
		}

		// (Populate and stage the asset for saving...)
		for (const FPcRhythmSectionProfile& Section : SectionsToExport)
		{
			const FName RowName = FName(*FString::Printf(TEXT("%d"), Section.StartTimeMS));
			NewRhythmTable->AddRow(RowName, Section);
		}
		FAssetRegistryModule::AssetCreated(NewRhythmTable);
		GeneratedRhythmProfile = NewRhythmTable;
		GeneratedAssetsToSave.Add(NewRhythmTable);
	}
	
	// --- Create the NOTE DATA DataTable (Second Dialog) ---
	UDataTable* NewNoteTable = nullptr;
	UDataTable* SourceNoteTable = this->GameplayMap;

	UDataTableFactory* NoteDataTableFactory = NewObject<UDataTableFactory>();
	NoteDataTableFactory->Struct = SourceNoteTable->GetRowStruct();
	
	// NAME FOR THE SECOND DIALOG: Based on "NoteData"
	FString NoteDefaultSaveName = FString::Printf(TEXT("DT_%s_NoteData"), *SongName.ToString());

	UObject* NewNoteAsset = AssetTools.CreateAssetWithDialog(
		NoteDefaultSaveName, DefaultSavePath, UDataTable::StaticClass(), NoteDataTableFactory
	);
	NewNoteTable = Cast<UDataTable>(NewNoteAsset);
	
	if (NewNoteTable)
	{
		// (Populate and stage the asset for saving...)
		const TMap<FName, uint8*>& RowMap = SourceNoteTable->GetRowMap();
		for (auto RowIt = RowMap.CreateConstIterator(); RowIt; ++RowIt)
		{
			NewNoteTable->AddRow(RowIt.Key(), *reinterpret_cast<FTableRowBase*>(RowIt.Value()));
		}
		FAssetRegistryModule::AssetCreated(NewNoteTable);
		GeneratedNoteData = NewNoteTable;
		GeneratedAssetsToSave.Add(NewNoteTable);
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("Note Data Table creation was cancelled by the user."));
	}

	// --- Finalize: Mark dirty and Save all new assets ---
	this->MarkPackageDirty();

	// if (GeneratedAssetsToSave.Num() > 0)
	// {
	// 	PackageTools::SavePackagesForObjects(GeneratedAssetsToSave);
	// }
	
	// (Notification and Sync Browser logic...)
	FNotificationInfo Info(LOCTEXT("RhythmDataAssetsGenerated", "Successfully generated and saved Rhythm and Note data assets. Please save THIS configuration asset!"));
	Info.ExpireDuration = 8.0f;
	FSlateNotificationManager::Get().AddNotification(Info);

	if (GEditor)
	{
		TArray<UObject*> ObjectsToSync = GeneratedAssetsToSave;
		if (ObjectsToSync.Num() > 0)
		{
			GEditor->SyncBrowserToObjects(ObjectsToSync);
		}
	}
}

#undef LOCTEXT_NAMESPACE

#endif // WITH_EDITOR