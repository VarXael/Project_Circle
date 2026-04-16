#include "PcMusicConfigurationData.h"

#if WITH_EDITOR
#include "PcMusicAnalyzer.h"
#include "Engine/DataTable.h"
#include "Factories/DataTableFactory.h"
#include "Modules/ModuleManager.h"
#include "IAssetTools.h"
#include "AssetToolsModule.h"
#include "Misc/PackageName.h"
#include "PackageTools.h"
#include "Editor.h"

void UPcMusicConfigurationData::GenerateRhythmAssets()
{
	if (!GameplayMap) return;

	UPcMusicAnalyzer* AnalysisProfile = UPcMusicAnalyzer::RunSongAnalysis(this, this);
	if (!AnalysisProfile) return;

	FAssetToolsModule& AssetToolsModule = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools");
	IAssetTools& AssetTools = AssetToolsModule.Get();
	FString DefaultSavePath = FPackageName::GetLongPackagePath(GetPathName());
	TArray<UObject*> GeneratedAssetsToSave;

	// 1. RHYTHM PROFILE
	UDataTableFactory* RhythmFactory = NewObject<UDataTableFactory>();
	RhythmFactory->Struct = FPcRhythmSectionProfile::StaticStruct();
	FString RhythmName = FString::Printf(TEXT("DT_%s_RhythmProfile"), *GetName());
	
	if (UDataTable* NewRhythmTable = Cast<UDataTable>(AssetTools.CreateAssetWithDialog(RhythmName, DefaultSavePath, UDataTable::StaticClass(), RhythmFactory)))
	{
		for (const FPcRhythmSectionProfile& Section : AnalysisProfile->GetRhythmSections())
			NewRhythmTable->AddRow(FName(*FString::Printf(TEXT("%d"), Section.StartTimeMS)), Section);
		
		GeneratedRhythmProfile = NewRhythmTable;
		GeneratedAssetsToSave.Add(NewRhythmTable);
	}

	// 2. RUNTIME NOTES (Using the new lightweight struct)
	UDataTableFactory* NoteFactory = NewObject<UDataTableFactory>();
	NoteFactory->Struct = FPcRuntimeEvent::StaticStruct();
	FString NoteName = FString::Printf(TEXT("DT_%s_RuntimeNotes"), *GetName());

	if (UDataTable* NewNoteTable = Cast<UDataTable>(AssetTools.CreateAssetWithDialog(NoteName, DefaultSavePath, UDataTable::StaticClass(), NoteFactory)))
	{
		int32 i = 0;
		for (const FPcRuntimeEvent& Ev : AnalysisProfile->GetRuntimeEvents())
			NewNoteTable->AddRow(FName(*FString::Printf(TEXT("Ev_%d"), i++)), Ev);

		GeneratedNoteData = NewNoteTable;
		GeneratedAssetsToSave.Add(NewNoteTable);
	}

	MarkPackageDirty();
	if (GeneratedAssetsToSave.Num() > 0) PackageTools::SavePackagesForObjects(GeneratedAssetsToSave);
}
#endif