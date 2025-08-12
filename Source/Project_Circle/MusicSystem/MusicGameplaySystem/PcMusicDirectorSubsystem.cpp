// Fill out your copyright notice in the Description page of Project Settings.

#include "Project_Circle/MusicSystem/MusicGameplaySystem/PcMusicDirectorSubsystem.h"
#include "Engine/DataTable.h"
#include "MusicActions/MusicActionInstance.h"
#include "MusicActions/MusicActionSet.h"
#include "Project_Circle/MusicSystem/MusicImportSystem/PcMusicConfigurationData.h"
#include "Project_Circle/MusicSystem/MusicGameplaySystem/PcMusicGameplayManager.h"

void UPcMusicDirectorSubsystem::InitializePlayback(UPcMusicConfigurationData* SongConfig, APcMusicGameplayManager* InMusicManager)
{
	ResetState();

	if (!SongConfig || !InMusicManager)
	{
		UE_LOG(LogTemp, Error, TEXT("MusicDirectorSubsystem: InitializePlayback failed due to null SongConfig or InMusicManager."));
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
	
	// --- THIS IS THE CORRECTED DATA LOADING SECTION, USING YOUR ORIGINAL LOGIC ---
	// Step 1: Get arrays of POINTERS to the data table rows.
	TArray<FPcMusicGameplayEvents*> TempRhythmPtrs;
	RhythmProfileData->GetAllRows(TEXT("Loading Rhythm Profile"), TempRhythmPtrs);
	
	TArray<FPcMusicGameplayNotes*> TempNotePtrs;
	NoteEventData->GetAllRows(TEXT("Loading Note Events"), TempNotePtrs);

	// Step 2: Loop through the pointers and add COPIES of the data to our member variables.
	for (const FPcMusicGameplayEvents* Ptr : TempRhythmPtrs)
	{
		if (Ptr)
		{
			RhythmProfileRows.Add(*Ptr);
		}
	}
	for (const FPcMusicGameplayNotes* Ptr : TempNotePtrs)
	{
		if (Ptr)
		{
			NoteEventRows.Add(*Ptr);
		}
	}
	// --- END OF CORRECTED SECTION ---

	if (RhythmProfileRows.Num() == 0 && NoteEventRows.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("MusicDirectorSubsystem: Both DataTables were empty. This might be intended, but playback will have no events."));
	}

	NoteEventRows.Sort([](const FPcMusicGameplayNotes& A, const FPcMusicGameplayNotes& B) {
		return A.StartTimeMS < B.StartTimeMS;
	});

	int32 LatestEventTime = 0;
	if (NoteEventRows.Num() > 0)
	{
		LatestEventTime = FMath::Max(LatestEventTime, NoteEventRows.Last().StartTimeMS);
	}
	//todo Make a pooling system!
	AbsoluteSongEndTimeMS = LatestEventTime + 2000; // 2-second buffer

	bIsReadyForPlayback = true;
	UE_LOG(LogTemp, Log, TEXT("MusicDirectorSubsystem: Initialized with %d rhythm sections and %d note events. Ready."), RhythmProfileRows.Num(), NoteEventRows.Num());
}

void UPcMusicDirectorSubsystem::UpdateMusicTime(float CurrentTimeSeconds)
{
	if (!bIsReadyForPlayback) return;

	const int32 CurrentTimeMs = FMath::RoundToInt(CurrentTimeSeconds * 1000.0f);
	if (CurrentTimeMs > LastProcessedMusicProgressMs)
	{
		UpdateRhythmSection(CurrentTimeMs);
		ProcessBeatTicks(CurrentTimeMs);
		ProcessNoteSpawning(CurrentTimeMs);
		
		if (AbsoluteSongEndTimeMS > 0 && CurrentTimeMs >= AbsoluteSongEndTimeMS)
		{
			OnSongEnd.Broadcast(AbsoluteSongEndTimeMS / 1000.f);
			AbsoluteSongEndTimeMS = -1;
		}
		
		LastProcessedMusicProgressMs = CurrentTimeMs;
		OnMusicTick.Broadcast(CurrentTimeMs);
	}
}

void UPcMusicDirectorSubsystem::ResetState()
{
	bIsReadyForPlayback = false;
	MusicManager = nullptr;
	ActiveNoteInstances.Empty();
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
	if (IsInBreakPeriod()) return;
	
	const int32 LookaheadBoundaryMS = InCurrentTimeMS + LookaheadTimeMS;

	while (NextNoteToSpawnIndex < NoteEventRows.Num())
	{
		const FPcMusicGameplayNotes& NextNote = NoteEventRows[NextNoteToSpawnIndex];

		if (NextNote.StartTimeMS <= LookaheadBoundaryMS)
		{
			const UMusicActionSet* ActionSet = NextNote.MusicGameplayEventDefinition;
			if (ActionSet && ActionSet->MusicActionInstanceClass)
			{
				UMusicActionInstance* NewInstance = NewObject<UMusicActionInstance>(this, ActionSet->MusicActionInstanceClass);
				NewInstance->Initialize(NextNote, MusicManager, this);
				ActiveNoteInstances.Add(NewInstance);
			}
			NextNoteToSpawnIndex++;
		}
		else
		{
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

	if (CurrentBreakEndTimeMS != -1 && InCurrentTimeMS >= CurrentBreakEndTimeMS)
	{
		OnBreakEnd.Broadcast(0, CurrentBreakEndTimeMS);
		CurrentBreakEndTimeMS = -1;
	}
}

void UPcMusicDirectorSubsystem::ProcessBeatTicks(int32 InCurrentTimeMS)
{
	if (IsInBreakPeriod()) return;
	
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