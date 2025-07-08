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
	if (!SongConfig || !SongConfig->GameplayMap) // Also check for GameplayMap
	{
		UE_LOG(LogTemp, Error, TEXT("RhythmDataGenerator: Cannot generate. Missing SongConfig or its GameplayMap."));
		return;
	}
	if (SectionsToExport.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("RhythmDataGenerator: No sections were generated. No DataTable will be created."));
		return;
	}

	// --- 2. PREPARE SAVE PATH ---
	const FString SourceAssetPath = SongConfig->GetPathName();
	FString DefaultSavePath = FPackageName::GetLongPackagePath(SourceAssetPath) + TEXT("/");
	FString DefaultSaveName = FString::Printf(TEXT("DT_%s_RhythmProfile"), *SongConfig->GetName()); // More specific name

	FAssetToolsModule& AssetToolsModule = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools");

	// --- 3. CREATE THE RHYTHM DATATABLE ASSET ---
	UDataTableFactory* DataTableFactory = NewObject<UDataTableFactory>();
	DataTableFactory->Struct = FRhythmSectionProfile::StaticStruct();

	UObject* NewRhythmAsset = AssetToolsModule.Get().CreateAssetWithDialog(
		UDataTable::StaticClass(),
		DataTableFactory,
		FName(*DefaultSavePath)
	);

	UDataTable* NewRhythmTable = Cast<UDataTable>(NewRhythmAsset);

	if (!NewRhythmTable)
	{
		UE_LOG(LogTemp, Log, TEXT("RhythmDataGenerator: Rhythm Profile creation was cancelled by the user."));
		return;
	}

	// --- 4. POPULATE THE RHYTHM DATATABLE ---
	for (const FRhythmSectionProfile& Section : SectionsToExport)
	{
		const FName RowName = FName(*FString::Printf(TEXT("%d"), Section.StartTimeMS));
		NewRhythmTable->AddRow(RowName, Section); // No need for a copy, AddRow handles const reference
	}

	NewRhythmTable->MarkPackageDirty();
	FAssetRegistryModule::AssetCreated(NewRhythmTable);

	// --- 5. DUPLICATE THE NOTE DATA TABLE (The New Logic) ---
	FString NoteDataSaveName = FString::Printf(TEXT("DT_%s_NoteData"), *SongConfig->GetName());
	FString NoteDataSavePath = FPackageName::GetLongPackagePath(NewRhythmTable->GetPathName()); // Save it in the same folder
	
	UObject* NewNoteAsset = AssetToolsModule.Get().DuplicateAsset(NoteDataSaveName, NoteDataSavePath, SongConfig->GameplayMap);
	UDataTable* NewNoteTable = Cast<UDataTable>(NewNoteAsset);
	
	if(NewNoteTable)
	{
		NewNoteTable->MarkPackageDirty();
		FAssetRegistryModule::AssetCreated(NewNoteTable);
	}

	// --- 6. FINALIZE AND NOTIFY ---
	FNotificationInfo Info(LOCTEXT("RhythmDataAssetsGenerated", "Successfully generated Rhythm and Note data assets."));
	Info.ExpireDuration = 5.0f;
	FSlateNotificationManager::Get().AddNotification(Info);

	// Highlight BOTH new assets in the Content Browser.
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