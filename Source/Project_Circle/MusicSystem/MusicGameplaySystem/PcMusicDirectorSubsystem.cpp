#include "Project_Circle/MusicSystem/MusicGameplaySystem/PcMusicDirectorSubsystem.h"
#include "Engine/DataTable.h"
#include "Project_Circle/MusicEventsSystem/MusicAction.h"
#include "Project_Circle/MusicEventsSystem/MusicActionSequence.h"
#include "Project_Circle/MusicEventsSystem/MusicProxy.h"
#include "Project_Circle/MusicSystem/MusicImportSystem/PcMusicConfigurationData.h"

void UPcMusicDirectorSubsystem::InitializePlayback(UPcMusicConfigurationData* SongConfig)
{
	ResetState();

	if (!SongConfig)
	{
		UE_LOG(LogTemp, Error, TEXT("MusicGameplaySubsystem: Provided SongConfiguration was null."));
		return;
	}

	UDataTable* RhythmProfileData = SongConfig->GeneratedMusicEventsProfile;
	UDataTable* NoteEventData = SongConfig->GeneratedMusicNotesProfile;

	if (!RhythmProfileData || !NoteEventData)
	{
		UE_LOG(LogTemp, Error, TEXT("MusicGameplaySubsystem: A required DataTable is missing."));
		return;
	}

	// Load Rhythm Profile (which now contains Meter and Break info)
	TArray<FPcMusicGameplayEvents*> TempRhythmPtrs;
	RhythmProfileData->GetAllRows(TEXT("Loading Rhythm Profile"), TempRhythmPtrs);
	for (const FPcMusicGameplayEvents* Ptr : TempRhythmPtrs)
	{
		if (Ptr)
		{
			RhythmProfileRows.Add(*Ptr);
		}
	}

	// Load Note Events
	TArray<FPcMusicGameplayNotes*> TempNotePtrs;
	NoteEventData->GetAllRows(TEXT("Loading Note Events"), TempNotePtrs);
	for (const FPcMusicGameplayNotes* Ptr : TempNotePtrs)
	{
		if (Ptr) NoteEventRows.Add(*Ptr);
	}
	
	if (RhythmProfileRows.Num() == 0 && NoteEventRows.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("MusicGameplaySubsystem: Both DataTables were empty."));
		return;
	}

	NoteEventRows.Sort([](const FPcMusicGameplayNotes& A, const FPcMusicGameplayNotes& B) {
		return A.StartTimeMS < B.StartTimeMS;
	});

	int32 LatestEventTime = 0;
	for(const auto& Event : RhythmProfileRows)
	{
		LatestEventTime = FMath::Max(LatestEventTime, Event.StartTimeMS);
		LatestEventTime = FMath::Max(LatestEventTime, Event.BreakEndTimeMS);
	}
	if (NoteEventRows.Num() > 0)
	{
		LatestEventTime = FMath::Max(LatestEventTime, NoteEventRows.Last().StartTimeMS);
	}
	AbsoluteSongEndTimeMS = LatestEventTime + 2000; // 2-second buffer

	bIsReadyForPlayback = true;
	UE_LOG(LogTemp, Log, TEXT("MusicGameplaySubsystem: Initialized with %d rhythm sections and %d note events. Ready."), RhythmProfileRows.Num(), NoteEventRows.Num());
}

void UPcMusicDirectorSubsystem::UpdateMusicTime(float CurrentTimeSeconds)
{
	if (!bIsReadyForPlayback) return;

	const int32 CurrentTimeMs = FMath::RoundToInt(CurrentTimeSeconds * 1000.0f);
	if (CurrentTimeMs > LastProcessedMusicProgressMs)
	{
		ProcessMusicEvents();
		
		if (AbsoluteSongEndTimeMS > 0 && CurrentTimeMs >= AbsoluteSongEndTimeMS)
		{
			OnSongEnd.Broadcast(AbsoluteSongEndTimeMS / 1000.f);
			AbsoluteSongEndTimeMS = -1;
		}
		
		LastProcessedMusicProgressMs = CurrentTimeMs;
		OnSongProgress.Broadcast(CurrentTimeMs);
	}
}

void UPcMusicDirectorSubsystem::ResetState()
{
	bIsReadyForPlayback = false;
	RhythmProfileRows.Empty();
	NoteEventRows.Empty();
	NextNoteIndex = 0;
	LastProcessedMusicProgressMs = -1;
	AbsoluteSongEndTimeMS = -1;
	CurrentSectionIndex = 0;
	NextBeatTimestampMS = 0;
	CurrentBeatInSession = 0;
	CurrentBPM = 0.f;
	
	// Reset the restored state variables
	CurrentMeter = 4;
	CurrentBreakEndTimeMS = -1;
}

AMusicProxy* UPcMusicDirectorSubsystem::SpawnMusicProxy(TSubclassOf<AMusicProxy> ProxyClass, const FTransform& SpawnTransform)
{
	// --- Safety Checks ---
	if (!ProxyClass)
	{
		UE_LOG(LogTemp, Error, TEXT("SpawnProxy failed: ProxyClass was not specified."));
		return nullptr;
	}

	// --- Spawn the Actor ---
	AMusicProxy* SpawnedProxy = GetWorld()->SpawnActor<AMusicProxy>(ProxyClass, SpawnTransform);

	if (SpawnedProxy)
	{
		UE_LOG(LogTemp, Log, TEXT("Successfully spawned proxy: %s"), *SpawnedProxy->GetName());
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("SpawnActor failed for an unknown reason."));
	}

	return SpawnedProxy;
}

void UPcMusicDirectorSubsystem::ProcessMusicEvents()
{
	const int32 CurrentTimeMs = LastProcessedMusicProgressMs;
	
	UpdateRhythmSection(CurrentTimeMs);
	ProcessBeatTicks(CurrentTimeMs);

	// Process Note Hits
	while (NextNoteIndex < NoteEventRows.Num())
	{
		const FPcMusicGameplayNotes& NextNote = NoteEventRows[NextNoteIndex];
		if (NextNote.StartTimeMS <= CurrentTimeMs)
		{
			OnNoteHit.Broadcast(NextNote.StartTimeMS);
			//todo temporary, just to test for now.
			if(NextNote.MusicGameplayEventDefinition)
			{
				for(UMusicAction* Action : NextNote.MusicGameplayEventDefinition->MusicActions)
					Action->OnExecute(this);
			}
			NextNoteIndex++;
		}
		else
		{
			break; // Notes are sorted
		}
	}

	// Check for the end of a break period
	if (CurrentBreakEndTimeMS != -1 && CurrentTimeMs >= CurrentBreakEndTimeMS)
	{
		OnBreakEnd.Broadcast(0, CurrentBreakEndTimeMS); // StartTime isn't relevant for the end event
		CurrentBreakEndTimeMS = -1; // We are no longer in a break
	}
}

void UPcMusicDirectorSubsystem::UpdateRhythmSection(int32 InCurrentTimeMS)
{
	if (RhythmProfileRows.Num() == 0) return;
	
	int32 NewSectionIndex = CurrentSectionIndex;
	while (NewSectionIndex < RhythmProfileRows.Num() - 1 && InCurrentTimeMS >= RhythmProfileRows[NewSectionIndex + 1].StartTimeMS)
	{
		NewSectionIndex++;
	}

	if (NewSectionIndex != CurrentSectionIndex || CurrentBPM == 0.f)
	{
		CurrentSectionIndex = NewSectionIndex;
		const FPcMusicGameplayEvents& CurrentSection = RhythmProfileRows[CurrentSectionIndex];

		// Update BPM
		if (!FMath::IsNearlyEqual(CurrentBPM, CurrentSection.BPM))
		{
			CurrentBPM = CurrentSection.BPM;
			OnBPMChanged.Broadcast(CurrentBPM);
		}
		
		// --- LOGIC FOR METER CHANGES ---
		if (CurrentMeter != CurrentSection.Meter)
		{
			CurrentMeter = CurrentSection.Meter;
			OnMeterChanged.Broadcast(CurrentMeter);
		}

		// --- LOGIC FOR BREAKS ---
		// Check if this section is a break and we're not already in one
		if (CurrentSection.bIsBreakSection && CurrentBreakEndTimeMS == -1)
		{
			CurrentBreakEndTimeMS = CurrentSection.BreakEndTimeMS;
			OnBreakStart.Broadcast(CurrentSection.StartTimeMS, CurrentSection.BreakEndTimeMS);
		}
		
		// Reset beat tracking for the new section
		CurrentBeatInSession = 0; 
		NextBeatTimestampMS = CurrentSection.AnchorTimestampMS;
	}
}

void UPcMusicDirectorSubsystem::ProcessBeatTicks(int32 InCurrentTimeMS)
{
	// --- GUARD CLAUSE FOR BREAKS ---
	// Don't process any beat ticks if we are currently in a break period.
	if (CurrentBreakEndTimeMS != -1) return;
	
	if (RhythmProfileRows.Num() == 0 || CurrentSectionIndex >= RhythmProfileRows.Num()) return;

	const FPcMusicGameplayEvents& CurrentSection = RhythmProfileRows[CurrentSectionIndex];
	const float BeatLength = CurrentSection.BeatLengthMS;
	const int32 Anchor = CurrentSection.AnchorTimestampMS;

	if (BeatLength <= 0 || InCurrentTimeMS < Anchor) return;

	if (NextBeatTimestampMS <= 0 || NextBeatTimestampMS < InCurrentTimeMS - FMath::RoundToInt(BeatLength * 4))
	{
		const float BeatsPassed = (InCurrentTimeMS - Anchor) / BeatLength;
		CurrentBeatInSession = FMath::FloorToInt(BeatsPassed) + 1;
	}

	NextBeatTimestampMS = Anchor + FMath::RoundToInt(CurrentBeatInSession * BeatLength);

	while (InCurrentTimeMS >= NextBeatTimestampMS)
	{
		OnBeatTriggered.Broadcast(NextBeatTimestampMS / 1000.0f);
		CurrentBeatInSession++;
		NextBeatTimestampMS = Anchor + FMath::RoundToInt(CurrentBeatInSession * BeatLength);
	}
}