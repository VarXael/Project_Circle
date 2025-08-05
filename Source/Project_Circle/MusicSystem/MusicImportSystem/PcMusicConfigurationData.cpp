#include "PcMusicConfigurationData.h"

#if WITH_EDITOR

#include "PcMusicAnalyzer.h"
#include "PcMusicAnalysisTypes.h"
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
#include "Sound/SoundWave.h"

#define LOCTEXT_NAMESPACE "PcMusicConfigurationData"

namespace PcMusicConfig_Helpers
{
	TArray<FPcMusicGameplayNotes> GenerateSliderSubEvents(const FPcImportedMusicData& SliderData,
	                                                      const TMap<int32, float>& TimingPointMap,
	                                                      UMusicActionSet* NoteBehaviour);

	template <typename T>
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
			UObject* NewAsset = AssetToolsModule.Get().CreateAsset(AssetName, SavePath, UDataTable::StaticClass(),
			                                                       DataTableFactory);
			DataTable = Cast<UDataTable>(NewAsset);
		}
		if (DataTable)
		{
			DataTable->EmptyTable();
		}
		return DataTable;
	}

	// Function to convert Approach Rate (AR) to approach time in milliseconds
	float CalculateApproachTimeMS(float ApproachRate)
	{
		if (ApproachRate < 5.0f)
		{
			// AR < 5: 1800ms at AR0, scales linearly to 1200ms at AR5
			return 1800.0f - 120.0f * ApproachRate;
		}
		else if (FMath::IsNearlyEqual(ApproachRate, 5.0f))
		{
			// AR = 5: 1200ms
			return 1200.0f;
		}
		else // ApproachRate > 5.0f
		{
			// AR > 5: 1200ms at AR5, scales linearly to 450ms at AR10
			return 1200.0f - 150.0f * (ApproachRate - 5.0f);
		}
	}
}

void UPcMusicConfigurationData::GenerateRhythmAssets()
{
	if (!this->ImportedMusicDataProfile)
	{
		return;
	}

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

	FNotificationInfo Info(LOCTEXT("RhythmDataAssetsGenerated",
	                               "Successfully generated data assets. Please SAVE THIS Configuration Asset!"));
	Info.ExpireDuration = 8.0f;
	FSlateNotificationManager::Get().AddNotification(Info);

	if (GEditor && GeneratedAssetsToSync.Num() > 0)
	{
		GEditor->SyncBrowserToObjects(GeneratedAssetsToSync);
	}
}

UDataTable* UPcMusicConfigurationData::GenerateRhythmProfileTable(
	const TArray<FPcMusicGameplayEvents>& SectionsToExport)
{
	if (SectionsToExport.Num() == 0 || !ImportedMusicDataProfile)
	{
		return nullptr;
	}

	const FString DefaultSavePath = FPackageName::GetLongPackagePath(this->GetPathName());
	const FString RhythmDefaultSaveName = FString::Printf(TEXT("DT_%s_Events"), *SongName.ToString());

	UDataTable* NewRhythmTable = PcMusicConfig_Helpers::CreateOrFindDataTable<FPcMusicGameplayEvents>(
		RhythmDefaultSaveName, DefaultSavePath);
	if (!NewRhythmTable)
	{
		return nullptr;
	}

	// Clear any old data from the table to ensure we start fresh.
	NewRhythmTable->EmptyTable();

	TArray<FPcImportedMusicData*> SourceRows;
	ImportedMusicDataProfile->GetAllRows(TEXT(""), SourceRows);

	// Pre-filter the source rows for efficiency, so we don't loop through everything every time.
	TArray<FPcImportedMusicData*> TimingPoints;
	TArray<FPcImportedMusicData*> BreakPoints;
	for (FPcImportedMusicData* Row : SourceRows)
	{
		if (Row)
		{
			if (Row->EntryType == EPcGameplayEntryType::TimingPoint && Row->Uninherited == 1)
			{
				TimingPoints.Add(Row);
			}
			else if (Row->EntryType == EPcGameplayEntryType::Break)
			{
				BreakPoints.Add(Row);
			}
		}
	}

	TArray<FPcMusicGameplayEvents> EnrichedSections = SectionsToExport;

	int32 RowIndex = 0;
	for (FPcMusicGameplayEvents& Section : EnrichedSections)
	{
		// --- Step 1: Enrich the data with Meter and Break info ---
		int32 BestTimingPointTime = -1;
		for (const FPcImportedMusicData* TP : TimingPoints)
		{
			if (TP->TimestampMS <= Section.StartTimeMS && TP->TimestampMS > BestTimingPointTime)
			{
				Section.Meter = TP->Meter;
				BestTimingPointTime = TP->TimestampMS;
			}
		}

		for (const FPcImportedMusicData* BP : BreakPoints)
		{
			if (BP->TimestampMS == Section.StartTimeMS)
			{
				Section.bIsBreakSection = true;
				Section.BreakEndTimeMS = BP->BreakEndTimeMS;
				break;
			}
		}

		// --- Step 2: Assign the default behavior ---
		if (DefaultMusicEventBehaviour && Section.MusicGameplayEventDefinition == nullptr)
		{
			Section.MusicGameplayEventDefinition = DefaultMusicEventBehaviour;
		}

		// --- Step 3: Add the fully processed row to the table ---
		const FName RowName = FName(*FString::Printf(TEXT("Row_%d"), RowIndex));
		NewRhythmTable->AddRow(RowName, Section);
		RowIndex++;
	}

	// Mark the asset as created and dirty so the editor knows to save our changes.
	FAssetRegistryModule::AssetCreated(NewRhythmTable);
	NewRhythmTable->MarkPackageDirty();

	return NewRhythmTable;
}

UDataTable* UPcMusicConfigurationData::GenerateNotesTable()
{
	if (!this->ImportedMusicDataProfile) return nullptr;

	const FString DefaultSavePath = FPackageName::GetLongPackagePath(this->GetPathName());
	const FString NoteDefaultSaveName = FString::Printf(TEXT("DT_%s_Notes"), *SongName.ToString());

	UDataTable* NewNoteTable = PcMusicConfig_Helpers::CreateOrFindDataTable<FPcMusicGameplayNotes>(
		NoteDefaultSaveName, DefaultSavePath);
	if (!NewNoteTable)
	{
		return nullptr;
	}

	NewNoteTable->EmptyTable();

	TArray<FPcImportedMusicData*> SourceRows;
	this->ImportedMusicDataProfile->GetAllRows(TEXT(""), SourceRows);

	// --- FIX #1: Create a strong, GC-proof reference to the Data Asset ---
	TObjectPtr<UMusicActionSet> NoteBehaviour = DefaultMusicNoteBehaviour;
	if (!NoteBehaviour)
	{
		UE_LOG(LogTemp, Warning, TEXT("GenerateNotesTable: DefaultMusicNoteBehaviour is not set in the configuration asset. Notes will have no behavior."));
	}

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
			// Use the GC-proof local variable for assignment
			NoteEvent.MusicGameplayEventDefinition = NoteBehaviour;
			AllGameplayNotes.Add(NoteEvent);

			if (SourceRow->HitObjectType & 2) // Check for slider
			{
				// --- FIX #2: Pass the behavior into the helper function ---
				AllGameplayNotes.Append(PcMusicConfig_Helpers::GenerateSliderSubEvents(*SourceRow, TimingPointMap, NoteBehaviour));
			}
		}
	}

	AllGameplayNotes.Sort([](const FPcMusicGameplayNotes& A, const FPcMusicGameplayNotes& B)
	{
		return A.StartTimeMS < B.StartTimeMS;
	});

	int32 RowIndex = 0;
	// --- FIX #3: The flawed and redundant final loop has been removed. ---
	for (const FPcMusicGameplayNotes& NoteToAdd : AllGameplayNotes)
	{
		// --- Add the fully processed row to the table ---
		const FName RowName = FName(*FString::Printf(TEXT("Row_%d"), RowIndex));
		NewNoteTable->AddRow(RowName, NoteToAdd);
		RowIndex++;
	}

	FAssetRegistryModule::AssetCreated(NewNoteTable);
	NewNoteTable->MarkPackageDirty();

	return NewNoteTable;
}


TArray<FPcMusicGameplayNotes> PcMusicConfig_Helpers::GenerateSliderSubEvents(
	const FPcImportedMusicData& SliderData, const TMap<int32, float>& TimingPointMap, UMusicActionSet* NoteBehaviour)
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
	const float TickInterval = (BaseBeatLength > 0 && SliderData.SliderTickRate > 0)
		                           ? FMath::Max(20.f, BaseBeatLength / SliderData.SliderTickRate)
		                           : -1.f;

	if (SliderDuration > 0 && TickInterval > 0)
	{
		const float SinglePassDuration = SliderDuration / FMath::Max(1, SliderData.Repeats);
		for (int32 Pass = 0; Pass < SliderData.Repeats; ++Pass)
		{
			for (float TimeAlongPass = TickInterval; TimeAlongPass < SinglePassDuration; TimeAlongPass += TickInterval)
			{
				if (!FMath::IsNearlyEqual(TimeAlongPass, SinglePassDuration, 1.f))
				{
					FPcMusicGameplayNotes TickEvent;
					TickEvent.StartTimeMS = SliderData.TimestampMS + FMath::RoundToInt(
						(Pass * SinglePassDuration) + TimeAlongPass);
					TickEvent.ApproachRate = SliderData.ApproachRate;
					// --- ASSIGN BEHAVIOR HERE ---
					TickEvent.MusicGameplayEventDefinition = NoteBehaviour;
					SubEvents.Add(TickEvent);
				}
			}
		}
		for (int32 Repeat = 1; Repeat <= SliderData.Repeats; ++Repeat)
		{
			FPcMusicGameplayNotes TailEvent;
			TailEvent.StartTimeMS = SliderData.TimestampMS + FMath::RoundToInt(Repeat * SinglePassDuration);
			TailEvent.ApproachRate = SliderData.ApproachRate;
			// --- AND ASSIGN BEHAVIOR HERE ---
			TailEvent.MusicGameplayEventDefinition = NoteBehaviour;
			SubEvents.Add(TailEvent);
		}
	}
	return SubEvents;
}

#undef LOCTEXT_NAMESPACE

#endif