#include "Project_Circle/MusicSystem/MusicGameplaySystem/PcMusicAnalysisSubsystem.h"
#include "Engine/DataTable.h"
#include "Project_Circle/MusicSystem/MusicImportSystem/PcMusicConfigurationData.h"

void UPcMusicAnalysisSubsystem::InitializePlayback(UPcMusicConfigurationData* SongConfig)
{
	ResetState();

	if (!SongConfig)
	{
		UE_LOG(LogTemp, Error, TEXT("MusicAnalysisSubsystem: SongConfiguration was null."));
		return;
	}

	UDataTable* RhythmProfileData = SongConfig->GeneratedRhythmProfile;
	UDataTable* NoteEventData     = SongConfig->GeneratedNoteData;

	if (!RhythmProfileData || !NoteEventData)
	{
		UE_LOG(LogTemp, Error, TEXT("MusicAnalysisSubsystem: Missing GeneratedRhythmProfile or GeneratedNoteData."));
		return;
	}

	DefaultGameplayBPM = SongConfig->DefaultGameplayBPM > 0.f ? SongConfig->DefaultGameplayBPM : 110.f;

	TArray<FPcRhythmSectionProfile*> TempProfilePtrs;
	TArray<FPcImportedMusicData*>    TempEventPtrs;
	RhythmProfileData->GetAllRows(TEXT("Loading Rhythm Profile"), TempProfilePtrs);
	NoteEventData->GetAllRows(TEXT("Loading Note Events"),        TempEventPtrs);

	RhythmProfileRows.Reserve(TempProfilePtrs.Num());
	for (const FPcRhythmSectionProfile* Ptr : TempProfilePtrs)
		if (Ptr) RhythmProfileRows.Add(*Ptr);

	RuntimeEventRows.Reserve(TempEventPtrs.Num());
	for (const FPcImportedMusicData* Ptr : TempEventPtrs)
		if (Ptr) RuntimeEventRows.Add(*Ptr);

	if (RhythmProfileRows.Num() == 0 || RuntimeEventRows.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("MusicAnalysisSubsystem: One or both DataTables were empty."));
		return;
	}

	for (const FPcImportedMusicData& EventRow : RuntimeEventRows)
	{
		if (EventRow.EntryType == EPcGameplayEntryType::TimingPoint && EventRow.Uninherited == 1)
			MasterBeatLengths.Add(EventRow.TimestampMS, EventRow.BeatLength);

		AbsoluteSongEndTimeMS = FMath::Max(AbsoluteSongEndTimeMS, EventRow.TimestampMS);
		AbsoluteSongEndTimeMS = FMath::Max(AbsoluteSongEndTimeMS, EventRow.BreakEndTimeMS);
		AbsoluteSongEndTimeMS = FMath::Max(AbsoluteSongEndTimeMS, EventRow.SliderEndTimeMS);
	}
	AbsoluteSongEndTimeMS += 2000;

	bIsReadyForPlayback = true;
	UE_LOG(LogTemp, Log, TEXT("MusicAnalysisSubsystem: Initialized. %d rhythm sections, %d events. DefaultGameplayBPM=%.1f"),
		RhythmProfileRows.Num(), RuntimeEventRows.Num(), DefaultGameplayBPM);
}

void UPcMusicAnalysisSubsystem::ResetState()
{
	bIsReadyForPlayback          = false;
	RhythmProfileRows.Empty();
	RuntimeEventRows.Empty();
	MasterBeatLengths.Empty();
	NoteEventQueue.Empty();
	NextEventIndex               = 0;
	LastProcessedMusicProgressMs = -1;
	AbsoluteSongEndTimeMS        = -1;
	NextBeatTimestampMS          = 0;
	CurrentSectionIndex          = 0;
	CurrentBeatInSession         = 0;
	CurrentBPM                   = 0.f;
	CurrentMeter                 = 4;
	CurrentBreakEndTimeMS        = -1;
	CurrentGameplayBPM           = 0.f;
	BeatSubdivision              = 1;
	RawBeatCounter               = 0;
	DefaultGameplayBPM           = 110.f;
}

void UPcMusicAnalysisSubsystem::UpdateMusicTime(float CurrentTimeSeconds)
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
		OnSongProgress.Broadcast(CurrentTimeMs);
	}
}

void UPcMusicAnalysisSubsystem::UpdateGameplayBPM(const FPcRhythmSectionProfile& Section)
{
	// Trust the analyzer's BPM directly (unless you manually override it in the DataTable)
	const float EffectiveGameplayBPM = Section.GameplayBPM > 0.f ? Section.GameplayBPM : Section.BPM;

	if (!FMath::IsNearlyEqual(CurrentGameplayBPM, EffectiveGameplayBPM))
	{
		CurrentGameplayBPM = EffectiveGameplayBPM;
		
		// Calculate how many times faster this section is than the song's base tempo.
		// e.g. 440 / 110 = 4x. Or 880 / 440 = 2x.
		BeatSubdivision = FMath::Max(1, FMath::RoundToInt(CurrentGameplayBPM / DefaultGameplayBPM));
		
		RawBeatCounter = 0;

		OnGameplayBPMChanged.Broadcast(CurrentGameplayBPM);

		UE_LOG(LogTemp, Log, TEXT("MusicAnalysisSubsystem: EffectiveBPM=%.1f  SpeedMultiplier=%d"),
			EffectiveGameplayBPM, BeatSubdivision);
	}
}

void UPcMusicAnalysisSubsystem::UpdateRhythmSection(int32 InCurrentTimeMS)
{
	if (RhythmProfileRows.Num() == 0) return;

	int32 NewSectionIndex = CurrentSectionIndex;
	while (NewSectionIndex < RhythmProfileRows.Num() - 1 &&
		   InCurrentTimeMS >= RhythmProfileRows[NewSectionIndex + 1].StartTimeMS)
	{
		NewSectionIndex++;
	}

	if (NewSectionIndex != CurrentSectionIndex || CurrentBPM == 0.f)
	{
		CurrentSectionIndex = NewSectionIndex;
		const FPcRhythmSectionProfile& Section = RhythmProfileRows[CurrentSectionIndex];

		if (!FMath::IsNearlyEqual(CurrentBPM, Section.BPM))
		{
			CurrentBPM = Section.BPM;
			OnBPMChanged.Broadcast(CurrentBPM);
		}

		UpdateGameplayBPM(Section);

		CurrentBeatInSession = 0;
		NextBeatTimestampMS  = Section.AnchorTimestampMS;
	}
}

void UPcMusicAnalysisSubsystem::ProcessBeatTicks(int32 InCurrentTimeMS)
{
	if (InCurrentTimeMS < CurrentBreakEndTimeMS || RhythmProfileRows.Num() == 0 ||
		CurrentSectionIndex >= RhythmProfileRows.Num()) return;

	const FPcRhythmSectionProfile& Section = RhythmProfileRows[CurrentSectionIndex];
	const float BeatLength = Section.BeatLengthMS;
	const int32 Anchor     = Section.AnchorTimestampMS;

	if (BeatLength <= 0 || InCurrentTimeMS < Anchor) return;

	if (NextBeatTimestampMS <= 0 || NextBeatTimestampMS < InCurrentTimeMS - FMath::RoundToInt(BeatLength * 4))
	{
		const float BeatsPassed = (InCurrentTimeMS - Anchor) / BeatLength;
		CurrentBeatInSession    = FMath::FloorToInt(BeatsPassed) + 1;
	}

	NextBeatTimestampMS = Anchor + FMath::RoundToInt(CurrentBeatInSession * BeatLength);

	while (InCurrentTimeMS >= NextBeatTimestampMS)
	{
		const float BeatTimeSeconds = NextBeatTimestampMS / 1000.0f;

		OnBeatTriggered.Broadcast(BeatTimeSeconds);

		// Every music pulse triggers a gameplay jump pulse.
		OnGameplayBeatTriggered.Broadcast(BeatTimeSeconds);

		CurrentBeatInSession++;
		NextBeatTimestampMS = Anchor + FMath::RoundToInt(CurrentBeatInSession * BeatLength);
	}
}

void UPcMusicAnalysisSubsystem::ProcessMusicEvents()
{
	const int32 CurrentTimeMs = LastProcessedMusicProgressMs;
	UpdateRhythmSection(CurrentTimeMs);
	ProcessBeatTicks(CurrentTimeMs);

	while (true)
	{
		const FPcImportedMusicData* NextMajorEvent = (NextEventIndex < RuntimeEventRows.Num()) ? &RuntimeEventRows[NextEventIndex] : nullptr;
		FPcQueuedNoteEvent*         NextSubEvent   = (NoteEventQueue.Num() > 0) ? &NoteEventQueue.Last() : nullptr;

		const int32 NextMajorEventTime = NextMajorEvent ? NextMajorEvent->TimestampMS : INT_MAX;
		const int32 NextSubEventTime   = NextSubEvent   ? NextSubEvent->TimestampMS   : INT_MAX;

		if (FMath::Min(NextMajorEventTime, NextSubEventTime) > CurrentTimeMs) break;

		if (NextMajorEventTime <= NextSubEventTime)
		{
			if (NextMajorEvent)
			{
				if (NextMajorEvent->EntryType == EPcGameplayEntryType::HitObject)
				{
					OnNoteHit.Broadcast(NextMajorEvent->TimestampMS, NextMajorEvent->HitObjectType, NextMajorEvent->HitSound);
					if (NextMajorEvent->HitObjectType & 2)
						GenerateSliderSubEvents(*NextMajorEvent);
				}
				else if (NextMajorEvent->EntryType == EPcGameplayEntryType::TimingPoint)
				{
					if (NextMajorEvent->Uninherited == 1 && NextMajorEvent->Meter != CurrentMeter)
					{
						CurrentMeter = NextMajorEvent->Meter;
						OnMeterChanged.Broadcast(CurrentMeter);
					}
				}
				else if (NextMajorEvent->EntryType == EPcGameplayEntryType::Break)
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

void UPcMusicAnalysisSubsystem::GenerateSliderSubEvents(const FPcImportedMusicData& SliderData)
{
	if (!(SliderData.HitObjectType & 2)) return;

	float BaseBeatLength = 500.f;
	if (MasterBeatLengths.Num() > 0)
	{
		int32 BestTPTime = -1;
		for (const auto& Elem : MasterBeatLengths)
		{
			if (Elem.Key <= SliderData.TimestampMS && Elem.Key > BestTPTime)
			{
				BaseBeatLength = Elem.Value;
				BestTPTime     = Elem.Key;
			}
		}
	}

	const float SliderDuration = SliderData.SliderEndTimeMS - SliderData.TimestampMS;
	const float TickInterval   = (BaseBeatLength > 0 && SliderData.SliderTickRate > 0)
		? FMath::Max(20.f, BaseBeatLength / SliderData.SliderTickRate) : -1.f;

	if (SliderDuration > 0 && TickInterval > 0)
	{
		const float SinglePassDuration = SliderDuration / FMath::Max(1, SliderData.Repeats);
		for (int32 Pass = 0; Pass < SliderData.Repeats; ++Pass)
		{
			for (float T = TickInterval; T < SinglePassDuration; T += TickInterval)
			{
				if (!FMath::IsNearlyEqual(T, SinglePassDuration, 1.f))
				{
					FPcQueuedNoteEvent Tick;
					Tick.TimestampMS      = SliderData.TimestampMS + FMath::RoundToInt((Pass * SinglePassDuration) + T);
					Tick.NoteType         = EQueuedNoteType::SliderTick;
					Tick.OriginalHitSound = SliderData.HitSound;
					NoteEventQueue.Add(Tick);
				}
			}
		}
		for (int32 Repeat = 1; Repeat <= SliderData.Repeats; ++Repeat)
		{
			FPcQueuedNoteEvent Tail;
			Tail.TimestampMS      = SliderData.TimestampMS + FMath::RoundToInt(Repeat * SinglePassDuration);
			Tail.NoteType         = EQueuedNoteType::SliderTail;
			Tail.OriginalHitSound = SliderData.HitSound;
			NoteEventQueue.Add(Tail);
		}
	}

	// Sort descending so NoteEventQueue.Last() always gives the earliest timestamp.
	// ProcessMusicEvents pops from the back, so earliest must live at the end.
	NoteEventQueue.Sort([](const FPcQueuedNoteEvent& A, const FPcQueuedNoteEvent& B)
	{
		return A.TimestampMS > B.TimestampMS;
	});
}