// Includes have been updated
#include "Project_Circle/MusicSystem/MusicGameplaySystem/PcMusicDirectorSubsystem.h"
#include "Engine/DataTable.h"
#include "MusicActions/MusicActionInstance.h"
#include "Project_Circle/MusicSystem/MusicImportSystem/PcMusicConfigurationData.h"

// MODIFIED: InitializePlayback now requires the manager and stores it.
void UPcMusicDirectorSubsystem::InitializePlayback(UPcMusicConfigurationData* SongConfig, APcMusicGameplayManager* InMusicManager)
{
	ResetState();

	if (!SongConfig)
	{
		UE_LOG(LogTemp, Error, TEXT("MusicDirectorSubsystem: Provided SongConfiguration was null."));
		return;
	}
	// NEW: Validate and store the manager reference. It's crucial for spawning instances.
	if (!InMusicManager)
	{
		UE_LOG(LogTemp, Error, TEXT("MusicDirectorSubsystem: Provided PcMusicGameplayManager was null. Cannot proceed."));
		return;
	}
	MusicManager = InMusicManager;

	UDataTable* RhythmProfileData = SongConfig->GeneratedMusicEventsProfile;
	UDataTable* NoteEventData = SongConfig->GeneratedMusicNotesProfile;

	if (!RhythmProfileData || !NoteEventData)
	{
		UE_LOG(LogTemp, Error, TEXT("MusicDirectorSubsystem: A required DataTable is missing."));
		return;
	}

	// The rest of this function is unchanged, as the data loading is correct.
	TArray<FPcMusicGameplayEvents*> TempRhythmPtrs;
	RhythmProfileData->GetAllRows(TEXT("Loading Rhythm Profile"), TempRhythmPtrs);
	for (const FPcMusicGameplayEvents* Ptr : TempRhythmPtrs)
	{
		if (Ptr) RhythmProfileRows.Add(*Ptr);
	}
	TArray<FPcMusicGameplayNotes*> TempNotePtrs;
	NoteEventData->GetAllRows(TEXT("Loading Note Events"), TempNotePtrs);
	for (const FPcMusicGameplayNotes* Ptr : TempNotePtrs)
	{
		if (Ptr) NoteEventRows.Add(*Ptr);
	}
	if (RhythmProfileRows.Num() == 0 && NoteEventRows.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("MusicDirectorSubsystem: Both DataTables were empty."));
		return;
	}
	NoteEventRows.Sort([](const FPcMusicGameplayNotes& A, const FPcMusicGameplayNotes& B) {
		return A.StartTimeMS < B.StartTimeMS;
	});
	int32 LatestEventTime = 0;
	if (NoteEventRows.Num() > 0)
	{
		LatestEventTime = FMath::Max(LatestEventTime, NoteEventRows.Last().StartTimeMS);
	}
	AbsoluteSongEndTimeMS = LatestEventTime + 2000;
	bIsReadyForPlayback = true;
	UE_LOG(LogTemp, Log, TEXT("MusicDirectorSubsystem: Initialized with %d rhythm sections and %d note events. Ready."), RhythmProfileRows.Num(), NoteEventRows.Num());
}

// MODIFIED: UpdateMusicTime is now the central driver.
void UPcMusicDirectorSubsystem::UpdateMusicTime(float CurrentTimeSeconds)
{
	if (!bIsReadyForPlayback)
		return;

	const int32 CurrentTimeMs = FMath::RoundToInt(CurrentTimeSeconds * 1000.0f);
	if (CurrentTimeMs > LastProcessedMusicProgressMs)
	{
		// 1. Process rhythm and beat events
		UpdateRhythmSection(CurrentTimeMs);
		ProcessBeatTicks(CurrentTimeMs);
		
		// 2. Factory create instances for upcoming notes
		ProcessNoteSpawning(CurrentTimeMs);

		// 3. Heartbeat - broadcast the tick to all active instances
		OnMusicTick.Broadcast(CurrentTimeMs);
		
		// 4. Handle song end and progress broadcast
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
	MusicManager = nullptr;
	RhythmProfileRows.Empty();
	NoteEventRows.Empty();
	NextNoteToSpawnIndex = 0;
	LastProcessedMusicProgressMs = -1;
	AbsoluteSongEndTimeMS = -1;
	CurrentSectionIndex = 0;
	NextBeatTimestampMS = 0;
	CurrentBeatInSession = 0;
	CurrentBPM = 0.f;
	CurrentMeter = 4;
	CurrentBreakEndTimeMS = -1;
}

void UPcMusicDirectorSubsystem::ProcessNoteSpawning(int32 InCurrentTimeMS)
{
	if (IsInBreakPeriod())
		return;
	
	const int32 LookaheadBoundaryMS = InCurrentTimeMS + LookaheadTimeMS;

	// Iterate through the sorted list of notes.
	while (NextNoteToSpawnIndex < NoteEventRows.Num())
	{
		const FPcMusicGameplayNotes& NextNote = NoteEventRows[NextNoteToSpawnIndex];

		// Check if the note has entered our lookahead window.
		if (NextNote.StartTimeMS <= LookaheadBoundaryMS)
		{
			if (UMusicActionInstance* NewInstance = NewObject<UMusicActionInstance>(this))
			{
				NewInstance->Initialize(NextNote, MusicManager.Get(), this);
			}

			// Move to the next note in the list.
			NextNoteToSpawnIndex++;
		}
		else
		{
			// The notes are sorted by time, so if this one is outside the window, all subsequent ones will be too.
			break;
		}
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

		if (!FMath::IsNearlyEqual(CurrentBPM, CurrentSection.BPM))
		{
			CurrentBPM = CurrentSection.BPM;
			OnBPMChanged.Broadcast(CurrentBPM);
		}
		
		if (CurrentMeter != CurrentSection.Meter)
		{
			CurrentMeter = CurrentSection.Meter;
			OnMeterChanged.Broadcast(CurrentMeter);
		}

		if (CurrentSection.bIsBreakSection && CurrentBreakEndTimeMS == -1)
		{
			CurrentBreakEndTimeMS = CurrentSection.BreakEndTimeMS;
			OnBreakStart.Broadcast(CurrentSection.StartTimeMS, CurrentSection.BreakEndTimeMS);
		}
		
		CurrentBeatInSession = 0; 
		NextBeatTimestampMS = CurrentSection.AnchorTimestampMS;
	}

	// Check for the end of a break period
	if (CurrentBreakEndTimeMS != -1 && InCurrentTimeMS >= CurrentBreakEndTimeMS)
	{
		OnBreakEnd.Broadcast(0, CurrentBreakEndTimeMS);
		CurrentBreakEndTimeMS = -1;
	}
}

void UPcMusicDirectorSubsystem::ProcessBeatTicks(int32 InCurrentTimeMS)
{
	if (IsInBreakPeriod())
		return;
	
	if (RhythmProfileRows.Num() == 0 || CurrentSectionIndex >= RhythmProfileRows.Num())
		return;

	const FPcMusicGameplayEvents& CurrentSection = RhythmProfileRows[CurrentSectionIndex];
	const float BeatLength = CurrentSection.BeatLengthMS;
	const int32 Anchor = CurrentSection.AnchorTimestampMS;

	if (BeatLength <= 0 || InCurrentTimeMS < Anchor)
		return;

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