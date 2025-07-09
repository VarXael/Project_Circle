#include "MusicAnalysisSubsystem.h"
#include "SongConfigurationData.h"
#include "Engine/DataTable.h"

void UMusicAnalysisSubsystem::InitializePlayback(USongConfigurationData* SongConfig)
{
	ResetState();

	if (!SongConfig)
	{
		UE_LOG(LogTemp, Error, TEXT("MusicAnalysisSubsystem: Provided SongConfiguration was null. Cannot initialize playback."));
		return;
	}

	// Get the generated data tables from the SongConfig asset
	UDataTable* RhythmProfileData = SongConfig->GeneratedRhythmProfile;
	UDataTable* NoteEventData = SongConfig->GeneratedNoteData;

	if (!RhythmProfileData || !NoteEventData)
	{
		UE_LOG(LogTemp, Error, TEXT("MusicAnalysisSubsystem: The provided SongConfiguration is missing its GeneratedRhythmProfile or GeneratedNoteData. Did you generate the assets?"));
		return;
	}

	TArray<FRhythmSectionProfile*> TempProfilePtrs;
	TArray<FMusicData*> TempEventPtrs;
	RhythmProfileData->GetAllRows(TEXT("Loading Rhythm Profile"), TempProfilePtrs);
	NoteEventData->GetAllRows(TEXT("Loading Note Events"), TempEventPtrs);

	RhythmProfileRows.Reserve(TempProfilePtrs.Num());
	for (const FRhythmSectionProfile* Ptr : TempProfilePtrs)
	{
		if (Ptr)
		{
			RhythmProfileRows.Add(*Ptr);
		}
	}

	RuntimeEventRows.Reserve(TempEventPtrs.Num());
	for (const FMusicData* Ptr : TempEventPtrs)
	{
		if (Ptr)
		{
			RuntimeEventRows.Add(*Ptr);
		}
	}

	if (RhythmProfileRows.Num() == 0 || RuntimeEventRows.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("MusicAnalysisSubsystem: One or both of the generated DataTables were empty."));
		return;
	}

	for (const FMusicData& EventRow : RuntimeEventRows)
	{
		if (EventRow.EntryType == EGameplayEntryType::TimingPoint && EventRow.Uninherited == 1)
		{
			MasterBeatLengths.Add(EventRow.TimestampMS, EventRow.BeatLength);
		}
		
		AbsoluteSongEndTimeMS = FMath::Max(AbsoluteSongEndTimeMS, EventRow.TimestampMS);
		AbsoluteSongEndTimeMS = FMath::Max(AbsoluteSongEndTimeMS, EventRow.BreakEndTimeMS);
		AbsoluteSongEndTimeMS = FMath::Max(AbsoluteSongEndTimeMS, EventRow.SliderEndTimeMS);
	}
	AbsoluteSongEndTimeMS += 2000;

	bIsReadyForPlayback = true;
	UE_LOG(LogTemp, Log, TEXT("MusicAnalysisSubsystem: Initialized with %d rhythm sections and %d timeline events. Ready."), RhythmProfileRows.Num(), RuntimeEventRows.Num());
}

void UMusicAnalysisSubsystem::ResetState()
{
	bIsReadyForPlayback = false;
	
	// FIX: Added back the lines to clear the main data arrays. This is critical.
	RhythmProfileRows.Empty();
	RuntimeEventRows.Empty();
	
	MasterBeatLengths.Empty();
	NoteEventQueue.Empty();
	NextEventIndex = 0;
	LastProcessedMusicProgressMs = -1;
	AbsoluteSongEndTimeMS = -1;
	NextBeatTimestampMS = 0;
	CurrentSectionIndex = 0;
	CurrentBeatInSession = 0;
	CurrentBPM = 0.f;
	CurrentMeter = 4;
	CurrentBreakEndTimeMS = -1;
}

void UMusicAnalysisSubsystem::UpdateMusicTime(float CurrentTimeSeconds)
{
	if (!bIsReadyForPlayback) return;
	
	const int32 CurrentTimeMs = FMath::RoundToInt(CurrentTimeSeconds * 1000.0f);
	if (CurrentTimeMs > LastProcessedMusicProgressMs)
	{
		ProcessMusicEvents();
		if (AbsoluteSongEndTimeMS > 0 && LastProcessedMusicProgressMs < AbsoluteSongEndTimeMS && CurrentTimeMs >= AbsoluteSongEndTimeMS)
		{
			OnSongEnd.Broadcast(AbsoluteSongEndTimeMS / 1000.f);
			AbsoluteSongEndTimeMS = -1;
		}
		LastProcessedMusicProgressMs = CurrentTimeMs;
	}
}

void UMusicAnalysisSubsystem::ProcessMusicEvents()
{
	const int32 CurrentTimeMs = LastProcessedMusicProgressMs;
	UpdateRhythmSection(CurrentTimeMs);
	ProcessBeatTicks(CurrentTimeMs);

	while (true)
	{
		// FIX: Added the '&' to get the address of the struct from the array.
		const FMusicData* NextMajorEvent = (NextEventIndex < RuntimeEventRows.Num()) ? &RuntimeEventRows[NextEventIndex] : nullptr;
		FQueuedNoteEvent* NextSubEvent = (NoteEventQueue.Num() > 0) ? &NoteEventQueue.Last() : nullptr;
		
		const int32 NextMajorEventTime = NextMajorEvent ? NextMajorEvent->TimestampMS : INT_MAX;
		const int32 NextSubEventTime = NextSubEvent ? NextSubEvent->TimestampMS : INT_MAX;
		
		if (FMath::Min(NextMajorEventTime, NextSubEventTime) > CurrentTimeMs)
		{
			break;
		}

		if (NextMajorEventTime <= NextSubEventTime)
		{
			if (NextMajorEvent)
			{
				if (NextMajorEvent->EntryType == EGameplayEntryType::HitObject)
				{
					OnNoteHit.Broadcast(NextMajorEvent->TimestampMS, NextMajorEvent->HitObjectType, NextMajorEvent->HitSound);
					if (NextMajorEvent->HitObjectType & 2)
					{
						GenerateSliderSubEvents(*NextMajorEvent);
					}
				}
				else if (NextMajorEvent->EntryType == EGameplayEntryType::TimingPoint)
				{
					if (NextMajorEvent->Uninherited == 1 && NextMajorEvent->Meter != CurrentMeter)
					{
						CurrentMeter = NextMajorEvent->Meter;
						OnMeterChanged.Broadcast(CurrentMeter);
					}
				}
				else if (NextMajorEvent->EntryType == EGameplayEntryType::Break)
				{
					CurrentBreakEndTimeMS = NextMajorEvent->BreakEndTimeMS;
					OnBreakStart.Broadcast(NextMajorEvent->TimestampMS, NextMajorEvent->BreakEndTimeMS);
				}
			}
			NextEventIndex++;
		}
		else
		{
			if (NextSubEvent)
			{
				OnNoteHit.Broadcast(NextSubEvent->TimestampMS, NextSubEvent->NoteType, NextSubEvent->OriginalHitSound);
				NoteEventQueue.Pop();
			}
		}
	}
	
	if (CurrentBreakEndTimeMS > 0 && LastProcessedMusicProgressMs < CurrentBreakEndTimeMS && CurrentTimeMs >= CurrentBreakEndTimeMS)
	{
		OnBreakEnd.Broadcast(0, CurrentBreakEndTimeMS);
		CurrentBreakEndTimeMS = -1;
	}
}

void UMusicAnalysisSubsystem::UpdateRhythmSection(int32 InCurrentTimeMS)
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
		const FRhythmSectionProfile* CurrentSection = &RhythmProfileRows[CurrentSectionIndex];
		
		if (CurrentSection)
		{
			if (!FMath::IsNearlyEqual(CurrentBPM, CurrentSection->BPM))
			{
				CurrentBPM = CurrentSection->BPM;
				OnBPMChanged.Broadcast(CurrentBPM);
			}
			CurrentBeatInSession = 0; 
			NextBeatTimestampMS = CurrentSection->AnchorTimestampMS;
		}
	}
}

void UMusicAnalysisSubsystem::ProcessBeatTicks(int32 InCurrentTimeMS)
{
	if (InCurrentTimeMS < CurrentBreakEndTimeMS || RhythmProfileRows.Num() == 0 || CurrentSectionIndex >= RhythmProfileRows.Num()) return;

	const FRhythmSectionProfile* CurrentSection = &RhythmProfileRows[CurrentSectionIndex];
	
	if (!CurrentSection) return;

	const float BeatLength = CurrentSection->BeatLengthMS;
	const int32 Anchor = CurrentSection->AnchorTimestampMS;

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

void UMusicAnalysisSubsystem::GenerateSliderSubEvents(const FMusicData& SliderData)
{
	if (!(SliderData.HitObjectType & 2)) return;
	
	float BaseBeatLength = 500.f;
	if (MasterBeatLengths.Num() > 0)
	{
		int32 BestTPTime = -1;
		for(const auto& Elem : MasterBeatLengths)
		{
			if (Elem.Key <= SliderData.TimestampMS && Elem.Key > BestTPTime)
			{
				BaseBeatLength = Elem.Value;
				BestTPTime = Elem.Key;
			}
		}
	}
	
	const float SliderDuration = SliderData.SliderEndTimeMS - SliderData.TimestampMS;
	const float TickInterval = (BaseBeatLength > 0 && SliderData.SliderTickRate > 0) ? FMath::Max(20.f, BaseBeatLength / SliderData.SliderTickRate) : -1.f;

	if (SliderDuration > 0 && TickInterval > 0) {
		const float SinglePassDuration = SliderDuration / FMath::Max(1, SliderData.Repeats);
		for (int32 Pass = 0; Pass < SliderData.Repeats; ++Pass) {
			for (float TimeAlongPass = TickInterval; TimeAlongPass < SinglePassDuration; TimeAlongPass += TickInterval) {
				if (!FMath::IsNearlyEqual(TimeAlongPass, SinglePassDuration, 1.f)) {
					FQueuedNoteEvent TickEvent;
					TickEvent.TimestampMS = SliderData.TimestampMS + FMath::RoundToInt((Pass * SinglePassDuration) + TimeAlongPass);
					TickEvent.NoteType = EQueuedNoteType::SliderTick;
					TickEvent.OriginalHitSound = SliderData.HitSound;
					NoteEventQueue.Add(TickEvent);
				}
			}
		}
		for (int32 Repeat = 1; Repeat <= SliderData.Repeats; ++Repeat) {
			FQueuedNoteEvent TailEvent;
			TailEvent.TimestampMS = SliderData.TimestampMS + FMath::RoundToInt(Repeat * SinglePassDuration);
			TailEvent.NoteType = EQueuedNoteType::SliderTail;
			TailEvent.OriginalHitSound = SliderData.HitSound;
			NoteEventQueue.Add(TailEvent);
		}
	}
	NoteEventQueue.Sort([](const FQueuedNoteEvent& A, const FQueuedNoteEvent& B) { return A.TimestampMS > B.TimestampMS; });
}