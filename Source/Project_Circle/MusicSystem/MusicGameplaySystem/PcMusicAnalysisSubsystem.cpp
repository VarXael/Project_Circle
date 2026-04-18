#include "PcMusicAnalysisSubsystem.h"
#include "Engine/DataTable.h"
#include "Project_Circle/MusicSystem/MusicImportSystem/PcMusicConfigurationData.h"

void UPcMusicAnalysisSubsystem::InitializePlayback(UPcMusicConfigurationData* SongConfig)
{
	ResetState();

	if (!SongConfig || !SongConfig->GeneratedRhythmProfile || !SongConfig->GeneratedNoteData)
	{
		UE_LOG(LogTemp, Error, TEXT("MusicAnalysisSubsystem: Missing configuration or generated DataTables."));
		return;
	}

	DefaultGameplayBPM = SongConfig->DefaultGameplayBPM > 0.f ? SongConfig->DefaultGameplayBPM : 110.f;

	SongConfig->GeneratedRhythmProfile->ForeachRow<FPcRhythmSectionProfile>("Load", [&](const FName&, const FPcRhythmSectionProfile& Row) { RhythmSections.Add(Row); });
	SongConfig->GeneratedNoteData->ForeachRow<FPcRuntimeEvent>("Load", [&](const FName&, const FPcRuntimeEvent& Row) { RuntimeEvents.Add(Row); });

	if (RhythmSections.IsEmpty() || RuntimeEvents.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("MusicAnalysisSubsystem: DataTables were empty."));
		return;
	}

	bIsReadyForPlayback = true;
}

void UPcMusicAnalysisSubsystem::ResetState()
{
	bIsReadyForPlayback = false;
	RhythmSections.Empty();
	RuntimeEvents.Empty();
	NextEventIndex = 0;
	LastProcessedMusicProgressMs = -1;
	CurrentSectionIndex = 0;
	CurrentBeatInSession = 0;
	NextBeatTimestampMS = 0;
	CurrentBPM = 0.f;
	CurrentGameplayBPM = 0.f;
	BeatSubdivision = 1.0f;
	CurrentPresetOverride = EPcMovementPresetOverride::Auto;
}

void UPcMusicAnalysisSubsystem::UpdateMusicTime(float CurrentTimeSeconds)
{
	if (!bIsReadyForPlayback) return;

	const int32 CurrentTimeMs = FMath::RoundToInt(CurrentTimeSeconds * 1000.0f);
	if (CurrentTimeMs > LastProcessedMusicProgressMs)
	{
		ProcessMusicEvents();
		LastProcessedMusicProgressMs = CurrentTimeMs;
		OnSongProgress.Broadcast(CurrentTimeMs);
	}
}

void UPcMusicAnalysisSubsystem::ProcessMusicEvents()
{
	const int32 CurrentTimeMs = LastProcessedMusicProgressMs;
	UpdateRhythmSection(CurrentTimeMs);
	ProcessBeatTicks(CurrentTimeMs);

	while (NextEventIndex < RuntimeEvents.Num() && RuntimeEvents[NextEventIndex].TimestampMS <= CurrentTimeMs)
	{
		const FPcRuntimeEvent& Event = RuntimeEvents[NextEventIndex];

		switch (Event.EventType)
		{
		case EPcRuntimeEventType::NoteHit:
			OnNoteHit.Broadcast(Event.TimestampMS, Event.Value1, Event.Value2);
			break;
		case EPcRuntimeEventType::MeterChange:
			OnMeterChanged.Broadcast(Event.Value1);
			break;
		case EPcRuntimeEventType::BreakStart:
			OnBreakStart.Broadcast(Event.TimestampMS, Event.Value1);
			break;
		case EPcRuntimeEventType::BreakEnd:
			OnBreakEnd.Broadcast(Event.Value1, Event.TimestampMS);
			break;
		case EPcRuntimeEventType::SongEnd:
			OnSongEnd.Broadcast(Event.TimestampMS / 1000.f);
			break;
		}

		NextEventIndex++;
	}
}

void UPcMusicAnalysisSubsystem::UpdateRhythmSection(int32 InCurrentTimeMS)
{
	if (RhythmSections.IsEmpty()) return;

	int32 NewSectionIndex = CurrentSectionIndex;
	while (NewSectionIndex < RhythmSections.Num() - 1 && InCurrentTimeMS >= RhythmSections[NewSectionIndex + 1].StartTimeMS)
	{
		NewSectionIndex++;
	}

	if (NewSectionIndex != CurrentSectionIndex || CurrentBPM == 0.f)
	{
		CurrentSectionIndex = NewSectionIndex;
		const FPcRhythmSectionProfile& Section = RhythmSections[CurrentSectionIndex];

		CurrentBPM = Section.BPM;
		OnBPMChanged.Broadcast(CurrentBPM);

		CurrentPresetOverride = Section.MovementPreset;
		float EffectiveBPM = Section.GameplayBPM > 0.f ? Section.GameplayBPM : Section.BPM;
		
		if (!FMath::IsNearlyEqual(CurrentGameplayBPM, EffectiveBPM))
		{
			CurrentGameplayBPM = EffectiveBPM;
			
			// We now calculate subdivision as a float so 0.5 (half-time) is respected!
			BeatSubdivision = FMath::Max(0.125f, CurrentGameplayBPM / DefaultGameplayBPM);
			OnGameplayBPMChanged.Broadcast(CurrentGameplayBPM);
		}

		CurrentBeatInSession = 0;
		NextBeatTimestampMS = Section.AnchorTimestampMS;
	}
}

void UPcMusicAnalysisSubsystem::ProcessBeatTicks(int32 InCurrentTimeMS)
{
	if (RhythmSections.IsEmpty()) return;

	const FPcRhythmSectionProfile& Section = RhythmSections[CurrentSectionIndex];
	if (Section.BeatLengthMS <= 0 || InCurrentTimeMS < Section.AnchorTimestampMS) return;

	if (NextBeatTimestampMS <= 0 || NextBeatTimestampMS < InCurrentTimeMS - FMath::RoundToInt(Section.BeatLengthMS * 4))
	{
		CurrentBeatInSession = FMath::FloorToInt((InCurrentTimeMS - Section.AnchorTimestampMS) / Section.BeatLengthMS) + 1;
	}

	NextBeatTimestampMS = Section.AnchorTimestampMS + FMath::RoundToInt(CurrentBeatInSession * Section.BeatLengthMS);

	while (InCurrentTimeMS >= NextBeatTimestampMS)
	{
		const float BeatTimeSeconds = NextBeatTimestampMS / 1000.0f;
		OnBeatTriggered.Broadcast(BeatTimeSeconds);
		OnGameplayBeatTriggered.Broadcast(BeatTimeSeconds);

		CurrentBeatInSession++;
		NextBeatTimestampMS = Section.AnchorTimestampMS + FMath::RoundToInt(CurrentBeatInSession * Section.BeatLengthMS);
	}
}