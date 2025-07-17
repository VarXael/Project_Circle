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

#define LOCTEXT_NAMESPACE "PcMusicConfigurationData"

// --- Private Helper Function Declarations ---
namespace PcMusicConfig_Helpers
{
	TArray<FPcMusicGameplayNotes> GenerateSliderSubEvents(const FPcImportedMusicData& SliderData, const TMap<int32, float>& TimingPointMap);
	
	template<typename T>
	UDataTable* CreateOrFindDataTable(const FString& AssetName, const FString& SavePath)
	{
		FAssetToolsModule& AssetToolsModule = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools");
		FString PackageName = SavePath + TEXT("/") + AssetName;
		UPackage* Package = CreatePackage(*PackageName);
		
		UDataTable* DataTable = FindObject<UDataTable>(Package, *AssetName);
		if (!DataTable)
		{
			UDataTableFactory* DataTableFactory = NewObject<UDataTableFactory>();
			DataTableFactory->Struct = T::StaticStruct();
			UObject* NewAsset = AssetToolsModule.Get().CreateAsset(AssetName, SavePath, UDataTable::StaticClass(), DataTableFactory);
			DataTable = Cast<UDataTable>(NewAsset);
		}
		if (DataTable)
		{
			DataTable->EmptyTable();
		}
		return DataTable;
	}
}

// --- The Main Public Function ---

void UPcMusicConfigurationData::GenerateRhythmAssets()
{
	UPcMusicAnalyzer* AnalysisProfile = UPcMusicAnalyzer::RunSongAnalysis(this, this);
	if (!AnalysisProfile) { return; }

	UDataTable* NewRhythmTable = GenerateRhythmProfileTable(AnalysisProfile->GetRhythmSections());
	UDataTable* NewNoteTable = GenerateNotesTable();

	GeneratedMusicEventsProfile = NewRhythmTable;
	GeneratedMusicNotesProfile = NewNoteTable;
	this->MarkPackageDirty();

	TArray<UObject*> GeneratedAssetsToSync;
	if (NewRhythmTable) GeneratedAssetsToSync.Add(NewRhythmTable);
	if (NewNoteTable) GeneratedAssetsToSync.Add(NewNoteTable);
	
	FNotificationInfo Info(LOCTEXT("RhythmDataAssetsGenerated", "Successfully generated data assets. Please SAVE THIS Configuration Asset!"));
	Info.ExpireDuration = 8.0f;
	FSlateNotificationManager::Get().AddNotification(Info);

	if (GEditor && GeneratedAssetsToSync.Num() > 0)
	{
		GEditor->SyncBrowserToObjects(GeneratedAssetsToSync);
	}
}

// --- Private Helper Function Implementations ---

UDataTable* UPcMusicConfigurationData::GenerateRhythmProfileTable(const TArray<FPcMusicGameplayEvents>& SectionsToExport)
{
	if (SectionsToExport.Num() == 0) return nullptr;

	const FString DefaultSavePath = FPackageName::GetLongPackagePath(this->GetPathName());
	const FString RhythmDefaultSaveName = FString::Printf(TEXT("DT_%s_Events"), *SongName.ToString());
	
	UDataTable* NewRhythmTable = PcMusicConfig_Helpers::CreateOrFindDataTable<FPcMusicGameplayEvents>(RhythmDefaultSaveName, DefaultSavePath);
	if (NewRhythmTable)
	{
		for (const FPcMusicGameplayEvents& Section : SectionsToExport)
		{
			const FName RowName = FName(*FString::Printf(TEXT("%d"), Section.StartTimeMS));
			NewRhythmTable->AddRow(RowName, Section);
		}
		FAssetRegistryModule::AssetCreated(NewRhythmTable);
		NewRhythmTable->MarkPackageDirty();
	}
	return NewRhythmTable;
}

UDataTable* UPcMusicConfigurationData::GenerateNotesTable()
{
	if (!this->ImportedMusicDataProfile) return nullptr;

	const FString DefaultSavePath = FPackageName::GetLongPackagePath(this->GetPathName());
	const FString NoteDefaultSaveName = FString::Printf(TEXT("DT_%s_Notes"), *SongName.ToString());

	UDataTable* NewNoteTable = PcMusicConfig_Helpers::CreateOrFindDataTable<FPcMusicGameplayNotes>(NoteDefaultSaveName, DefaultSavePath);
	if (NewNoteTable)
	{
		TArray<FPcImportedMusicData*> SourceRows;
		this->ImportedMusicDataProfile->GetAllRows(TEXT(""), SourceRows);

		// --- THE FIX: Pre-process to gather all timing points first ---
		TMap<int32, float> TimingPointMap;
		for (const FPcImportedMusicData* Row : SourceRows)
		{
			if (Row && Row->EntryType == EPcGameplayEntryType::TimingPoint && Row->Uninherited == 1)
			{
				TimingPointMap.Add(Row->TimestampMS, Row->BeatLength);
			}
		}

		TArray<FPcMusicGameplayNotes> AllGameplayNotes;
		for (const FPcImportedMusicData* SourceRow : SourceRows)
		{
			if (SourceRow && SourceRow->EntryType == EPcGameplayEntryType::HitObject)
			{
				FPcMusicGameplayNotes NoteEvent;
				NoteEvent.StartTimeMS = SourceRow->TimestampMS;
				NoteEvent.ApproachRate = SourceRow->ApproachRate;
				AllGameplayNotes.Add(NoteEvent);

				if (SourceRow->HitObjectType & 2)
				{
					// Now we pass the pre-processed map to the helper function
					AllGameplayNotes.Append(PcMusicConfig_Helpers::GenerateSliderSubEvents(*SourceRow, TimingPointMap));
				}
			}
		}

		AllGameplayNotes.Sort([](const FPcMusicGameplayNotes& A, const FPcMusicGameplayNotes& B) {
			return A.StartTimeMS < B.StartTimeMS;
		});

		for (const FPcMusicGameplayNotes& NoteToAdd : AllGameplayNotes)
		{
			const FName RowName = FName(*FString::Printf(TEXT("%d"), NoteToAdd.StartTimeMS));
			if (NewNoteTable->FindRowUnchecked(RowName))
			{
				const FName UniqueRowName = MakeUniqueObjectName(NewNoteTable, NewNoteTable->GetClass(), RowName);
				NewNoteTable->AddRow(UniqueRowName, NoteToAdd);
			}
			else
			{
				NewNoteTable->AddRow(RowName, NoteToAdd);
			}
		}
		FAssetRegistryModule::AssetCreated(NewNoteTable);
		NewNoteTable->MarkPackageDirty();
	}
	return NewNoteTable;
}

TArray<FPcMusicGameplayNotes> PcMusicConfig_Helpers::GenerateSliderSubEvents(const FPcImportedMusicData& SliderData, const TMap<int32, float>& TimingPointMap)
{
	TArray<FPcMusicGameplayNotes> SubEvents;
	float BaseBeatLength = 500.f;

	int32 BestTPTime = -1;
	for (const auto& Elem : TimingPointMap)
	{
		if (Elem.Key <= SliderData.TimestampMS && Elem.Key > BestTPTime)
		{
			BaseBeatLength = Elem.Value;
			BestTPTime = Elem.Key;
		}
	}
	
	const float SliderDuration = SliderData.SliderEndTimeMS - SliderData.TimestampMS;
	const float TickInterval = (BaseBeatLength > 0 && SliderData.SliderTickRate > 0) ? FMath::Max(20.f, BaseBeatLength / SliderData.SliderTickRate) : -1.f;

	if (SliderDuration > 0 && TickInterval > 0) {
		const float SinglePassDuration = SliderDuration / FMath::Max(1, SliderData.Repeats);
		for (int32 Pass = 0; Pass < SliderData.Repeats; ++Pass) {
			for (float TimeAlongPass = TickInterval; TimeAlongPass < SinglePassDuration; TimeAlongPass += TickInterval) {
				if (!FMath::IsNearlyEqual(TimeAlongPass, SinglePassDuration, 1.f))
				{
					FPcMusicGameplayNotes TickEvent;
					TickEvent.StartTimeMS = SliderData.TimestampMS + FMath::RoundToInt((Pass * SinglePassDuration) + TimeAlongPass);
					TickEvent.ApproachRate = SliderData.ApproachRate;
					SubEvents.Add(TickEvent);
				}
			}
		}
		for (int32 Repeat = 1; Repeat <= SliderData.Repeats; ++Repeat)
		{
			FPcMusicGameplayNotes TailEvent;
			TailEvent.StartTimeMS = SliderData.TimestampMS + FMath::RoundToInt(Repeat * SinglePassDuration);
			TailEvent.ApproachRate = SliderData.ApproachRate;
			SubEvents.Add(TailEvent);
		}
	}
	return SubEvents;
}


#undef LOCTEXT_NAMESPACE

#endif // WITH_EDITOR