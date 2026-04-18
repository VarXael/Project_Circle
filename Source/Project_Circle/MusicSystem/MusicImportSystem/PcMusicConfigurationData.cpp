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

	// 1. RHYTHM PROFILE (SILENT GENERATION)
	FString RhythmName = FString::Printf(TEXT("DT_%s_RhythmProfile"), *GetName());
	FString RhythmPackagePath = DefaultSavePath + TEXT("/") + RhythmName;
	
	UDataTable* RhythmTable = LoadObject<UDataTable>(nullptr, *RhythmPackagePath);
	if (RhythmTable) {
		RhythmTable->EmptyTable(); // Wipe existing cleanly
	} else {
		UDataTableFactory* RhythmFactory = NewObject<UDataTableFactory>();
		RhythmFactory->Struct = FPcRhythmSectionProfile::StaticStruct();
		RhythmTable = Cast<UDataTable>(AssetTools.CreateAsset(RhythmName, DefaultSavePath, UDataTable::StaticClass(), RhythmFactory));
	}

	if (RhythmTable) {
		for (const FPcRhythmSectionProfile& Section : AnalysisProfile->GetRhythmSections())
			RhythmTable->AddRow(FName(*FString::Printf(TEXT("%d"), Section.StartTimeMS)), Section);
		
		GeneratedRhythmProfile = RhythmTable;
		GeneratedAssetsToSave.Add(RhythmTable);
	}

	// 2. RUNTIME NOTES (SILENT GENERATION)
	FString NoteName = FString::Printf(TEXT("DT_%s_RuntimeNotes"), *GetName());
	FString NotePackagePath = DefaultSavePath + TEXT("/") + NoteName;
	
	UDataTable* NoteTable = LoadObject<UDataTable>(nullptr, *NotePackagePath);
	if (NoteTable) {
		NoteTable->EmptyTable(); // Wipe existing cleanly
	} else {
		UDataTableFactory* NoteFactory = NewObject<UDataTableFactory>();
		NoteFactory->Struct = FPcRuntimeEvent::StaticStruct();
		NoteTable = Cast<UDataTable>(AssetTools.CreateAsset(NoteName, DefaultSavePath, UDataTable::StaticClass(), NoteFactory));
	}

	if (NoteTable) {
		int32 i = 0;
		for (const FPcRuntimeEvent& Ev : AnalysisProfile->GetRuntimeEvents())
			NoteTable->AddRow(FName(*FString::Printf(TEXT("Ev_%d"), i++)), Ev);

		GeneratedNoteData = NoteTable;
		GeneratedAssetsToSave.Add(NoteTable);
	}

	MarkPackageDirty();
	if (GeneratedAssetsToSave.Num() > 0) UPackageTools::SavePackagesForObjects(GeneratedAssetsToSave);
}
#endif