#include "RhythmDataGenerator.h"
#include "SongConfigurationData.h"
#include "Engine/DataTable.h"
#include "Factories/DataTableFactory.h"
#include "Modules/ModuleManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Misc/PackageName.h"
#include "Editor.h"

#define LOCTEXT_NAMESPACE "RhythmDataGenerator"

void URhythmDataGenerator::GenerateRhythmDataTable(USongConfigurationData* SongConfig, const TArray<FRhythmSectionProfile>& SectionsToExport)
{
	// --- 1. VALIDATION ---
	if (!SongConfig || !SongConfig->GameplayMap)
	{
		UE_LOG(LogTemp, Error, TEXT("RhythmDataGenerator: Cannot generate. Missing SongConfig or its GameplayMap."));
		return;
	}
	if (SectionsToExport.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("RhythmDataGenerator: No sections were generated. No DataTable will be created."));
		return;
	}

	FAssetToolsModule& AssetToolsModule = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools");
	const FString SourceAssetPath = SongConfig->GetPathName();
	FString DefaultSavePath = FPackageName::GetLongPackagePath(SourceAssetPath);

	// --- 2. CREATE THE RHYTHM DATATABLE (First Dialog) ---
	UDataTableFactory* RhythmDataTableFactory = NewObject<UDataTableFactory>();
	RhythmDataTableFactory->Struct = FRhythmSectionProfile::StaticStruct();

	FString RhythmDefaultSaveName = FString::Printf(TEXT("DT_RhythmProfile_"));

	// *** CORRECTED CALL #1 ***
	// We provide the Default Name and Default Path as separate arguments.
	UObject* NewRhythmAsset = AssetToolsModule.Get().CreateAssetWithDialog(
		RhythmDefaultSaveName,      // Argument 1: Default Asset Name
		DefaultSavePath,            // Argument 2: Default Package Path
		UDataTable::StaticClass(),  // Argument 3: Asset Class
		RhythmDataTableFactory      // Argument 4: Factory
	);
	
	UDataTable* NewRhythmTable = Cast<UDataTable>(NewRhythmAsset);
	//todo you can actually take the name of the song from the music data struct and add it here: DT_NoteData_NomeOfTheSong
	if (!NewRhythmTable)
	{
		UE_LOG(LogTemp, Log, TEXT("RhythmDataGenerator: Rhythm Profile creation was cancelled by the user."));
		return;
	}

	// --- 3. POPULATE THE RHYTHM DATATABLE ---
	for (const FRhythmSectionProfile& Section : SectionsToExport)
	{
		const FName RowName = FName(*FString::Printf(TEXT("%d"), Section.StartTimeMS));
		NewRhythmTable->AddRow(RowName, Section);
	}
	NewRhythmTable->MarkPackageDirty();
	FAssetRegistryModule::AssetCreated(NewRhythmTable);

	// --- 4. CREATE THE NOTE DATA TABLE (Second Dialog) ---
	UDataTable* NewNoteTable = nullptr;
	UDataTable* SourceNoteTable = SongConfig->GameplayMap;

	UDataTableFactory* NoteDataTableFactory = NewObject<UDataTableFactory>();
	NoteDataTableFactory->Struct = SourceNoteTable->GetRowStruct();
	//todo you can actually take the name of the song from the music data struct and add it here: DT_NoteData_NomeOfTheSong
	FString NoteDefaultSaveName = FString::Printf(TEXT("DT_NoteData"));

	// *** CORRECTED CALL #2 ***
	UObject* NewNoteAsset = AssetToolsModule.Get().CreateAssetWithDialog(
		NoteDefaultSaveName,        // Argument 1: Default Asset Name
		DefaultSavePath,            // Argument 2: Default Package Path
		UDataTable::StaticClass(),  // Argument 3: Asset Class
		NoteDataTableFactory        // Argument 4: Factory
	);

	NewNoteTable = Cast<UDataTable>(NewNoteAsset);
	
	if (NewNoteTable)
	{
		// --- 5. POPULATE THE NEW NOTE DATA TABLE (Manually Copying Rows) ---
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
		UE_LOG(LogTemp, Log, TEXT("RhythmDataGenerator: Note Data Table creation was cancelled by the user."));
	}

	// --- 6. FINALIZE AND NOTIFY ---
	FNotificationInfo Info(LOCTEXT("RhythmDataAssetsGenerated", "Successfully generated Rhythm and Note data assets."));
	Info.ExpireDuration = 5.0f;
	FSlateNotificationManager::Get().AddNotification(Info);

	if (GEditor)
	{
		TArray<UObject*> ObjectsToSync;
		ObjectsToSync.Add(NewRhythmTable);
		if (NewNoteTable)
		{
			ObjectsToSync.Add(NewNoteTable);
		}
		GEditor->SyncBrowserToObjects(ObjectsToSync);
	}
}

#undef LOCTEXT_NAMESPACE