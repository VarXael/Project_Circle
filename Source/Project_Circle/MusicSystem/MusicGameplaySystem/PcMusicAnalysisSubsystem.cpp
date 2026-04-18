#include "PcMusicAnalysisSubsystem.h"
#include "Engine/DataTable.h"
#include "Project_Circle/MusicSystem/MusicImportSystem/PcMusicConfigurationData.h"

void UPcMusicAnalysisSubsystem::InitializePlayback(UPcMusicConfigurationData* SongConfig)
{
	ResetState();
	if (!SongConfig || !SongConfig->GeneratedRhythmProfile || !SongConfig->GeneratedNoteData) return;
	
	DefaultGameplayBPM = SongConfig->DefaultGameplayBPM > 0.f ? SongConfig->DefaultGameplayBPM : 110.f;
	
	PulseSlow = SongConfig->PulseSlow;
	PulseNormal = SongConfig->PulseNormal;
	PulseFast = SongConfig->PulseFast;
	PulseVeryFast = SongConfig->PulseVeryFast;

	SongConfig->GeneratedRhythmProfile->ForeachRow<FPcRhythmSectionProfile>("Load", [&](const FName&, const FPcRhythmSectionProfile& Row) { RhythmSections.Add(Row); });
	SongConfig->GeneratedNoteData->ForeachRow<FPcRuntimeEvent>("Load", [&](const FName&, const FPcRuntimeEvent& Row) { RuntimeEvents.Add(Row); });
	if (!RhythmSections.IsEmpty() && !RuntimeEvents.IsEmpty()) bIsReadyForPlayback = true;
}

void UPcMusicAnalysisSubsystem::ResetState()
{
	bIsReadyForPlayback = false; RhythmSections.Empty(); RuntimeEvents.Empty();
	NextEventIndex = 0; LastProcessedMusicProgressMs = -1; CurrentSectionIndex = 0;
	CurrentBeatInSession = 0; NextBeatTimestampMS = 0; NextGameplayBeatTimestampMS = 0;
	CurrentBPM = 0.f; CurrentGameplayBPM = 0.f; BeatSubdivision = 1.0f; CurrentPresetOverride = EPcMovementPresetOverride::Auto;
}

const FPcMovementPreset& UPcMusicAnalysisSubsystem::GetCurrentPulsePreset() const {
	if (CurrentPresetOverride == EPcMovementPresetOverride::Slow) return PulseSlow;
	if (CurrentPresetOverride == EPcMovementPresetOverride::Normal) return PulseNormal;
	if (CurrentPresetOverride == EPcMovementPresetOverride::Fast) return PulseFast;
	if (CurrentPresetOverride == EPcMovementPresetOverride::VeryFast) return PulseVeryFast;

	if (BeatSubdivision < 0.75f) return PulseSlow;
	if (BeatSubdivision < 1.5f) return PulseNormal;
	if (BeatSubdivision < 3.0f) return PulseFast;
	return PulseVeryFast;
}

FString UPcMusicAnalysisSubsystem::GetActivePresetName() const {
	if (CurrentPresetOverride == EPcMovementPresetOverride::Slow) return "FORCE SLOW";
	if (CurrentPresetOverride == EPcMovementPresetOverride::Normal) return "FORCE NORMAL";
	if (CurrentPresetOverride == EPcMovementPresetOverride::Fast) return "FORCE FAST";
	if (CurrentPresetOverride == EPcMovementPresetOverride::VeryFast) return "FORCE VERY FAST";
	
	if (BeatSubdivision < 0.75f) return "AUTO SLOW";
	if (BeatSubdivision < 1.5f) return "AUTO NORMAL";
	if (BeatSubdivision < 3.0f) return "AUTO FAST";
	return "AUTO VERY FAST";
}

void UPcMusicAnalysisSubsystem::UpdateMusicTime(float CurrentTimeSeconds)
{
	if (!bIsReadyForPlayback) return;
	const int32 CurrentTimeMs = FMath::RoundToInt(CurrentTimeSeconds * 1000.0f);
	if (CurrentTimeMs > LastProcessedMusicProgressMs) {
		ProcessMusicEvents(); LastProcessedMusicProgressMs = CurrentTimeMs; OnSongProgress.Broadcast(CurrentTimeMs);
	}
}

float UPcMusicAnalysisSubsystem::GetTimeUntilNextGameplayBeat() const
{
	if (!bIsReadyForPlayback || NextGameplayBeatTimestampMS <= 0 || LastProcessedMusicProgressMs < 0) return 0.f;
	float DiffMS = FMath::Max(0.f, (float)(NextGameplayBeatTimestampMS - LastProcessedMusicProgressMs));
	return DiffMS / 1000.f;
}

float UPcMusicAnalysisSubsystem::GetGameplayBeatIntervalMS() const
{
	if (RhythmSections.IsEmpty() || !bIsReadyForPlayback) return 0.f;
	const FPcRhythmSectionProfile& Section = RhythmSections[CurrentSectionIndex];
	return Section.BeatLengthMS * GetCurrentPulsePreset().BeatsPerJump;
}

TArray<FPcRuntimeEvent> UPcMusicAnalysisSubsystem::GetUpcomingNotes(float LookaheadWindowSec) const
{
	TArray<FPcRuntimeEvent> Upcoming;
	if (!bIsReadyForPlayback) return Upcoming;
	
	int32 MaxTimeMS = LastProcessedMusicProgressMs + FMath::RoundToInt(LookaheadWindowSec * 1000.f);
	
	for (int32 i = NextEventIndex; i < RuntimeEvents.Num(); ++i) {
		if (RuntimeEvents[i].TimestampMS > MaxTimeMS) break;
		if (RuntimeEvents[i].EventType == EPcRuntimeEventType::NoteHit) {
			Upcoming.Add(RuntimeEvents[i]);
		}
	}
	return Upcoming;
}

void UPcMusicAnalysisSubsystem::ProcessMusicEvents()
{
	const int32 CurrentTimeMs = LastProcessedMusicProgressMs;
	UpdateRhythmSection(CurrentTimeMs); ProcessBeatTicks(CurrentTimeMs);
	while (NextEventIndex < RuntimeEvents.Num() && RuntimeEvents[NextEventIndex].TimestampMS <= CurrentTimeMs)
	{
		const FPcRuntimeEvent& Event = RuntimeEvents[NextEventIndex];
		switch (Event.EventType) {
		case EPcRuntimeEventType::NoteHit: OnNoteHit.Broadcast(Event.TimestampMS, Event.Value1, Event.Value2); break;
		case EPcRuntimeEventType::MeterChange: OnMeterChanged.Broadcast(Event.Value1); break;
		case EPcRuntimeEventType::BreakStart: OnBreakStart.Broadcast(Event.TimestampMS, Event.Value1); break;
		case EPcRuntimeEventType::BreakEnd: OnBreakEnd.Broadcast(Event.Value1, Event.TimestampMS); break;
		case EPcRuntimeEventType::SongEnd: OnSongEnd.Broadcast(Event.TimestampMS / 1000.f); break;
		}
		NextEventIndex++;
	}
}

void UPcMusicAnalysisSubsystem::UpdateRhythmSection(int32 InCurrentTimeMS)
{
	if (RhythmSections.IsEmpty()) return;
	int32 NewSectionIndex = CurrentSectionIndex;
	while (NewSectionIndex < RhythmSections.Num() - 1 && InCurrentTimeMS >= RhythmSections[NewSectionIndex + 1].StartTimeMS) NewSectionIndex++;

	if (NewSectionIndex != CurrentSectionIndex || CurrentBPM == 0.f) {
		CurrentSectionIndex = NewSectionIndex; const FPcRhythmSectionProfile& Section = RhythmSections[CurrentSectionIndex];
		
		CurrentBPM = Section.BPM; OnBPMChanged.Broadcast(CurrentBPM);
		CurrentPresetOverride = Section.MovementPreset;
		
		// The active subdivision establishes which Preset is used.
		BeatSubdivision = FMath::Max(0.125f, (Section.GameplayBPM > 0.f ? Section.GameplayBPM : Section.BPM) / DefaultGameplayBPM);
		
		// The Gameplay BPM is explicitly calculated by dividing the Raw BPM by the Preset's jump cost!
		float NewGameplayBPM = CurrentBPM / GetCurrentPulsePreset().BeatsPerJump;
		if (!FMath::IsNearlyEqual(CurrentGameplayBPM, NewGameplayBPM)) {
			CurrentGameplayBPM = NewGameplayBPM;
			OnGameplayBPMChanged.Broadcast(CurrentGameplayBPM);
		}
		
		CurrentBeatInSession = 0; 
		NextBeatTimestampMS = Section.AnchorTimestampMS;
		
		// Align the true Gameplay Beat to the start of the section.
		NextGameplayBeatTimestampMS = Section.AnchorTimestampMS;
	}
}

void UPcMusicAnalysisSubsystem::ProcessBeatTicks(int32 InCurrentTimeMS)
{
	if (RhythmSections.IsEmpty()) return;
	const FPcRhythmSectionProfile& Section = RhythmSections[CurrentSectionIndex];
	if (Section.BeatLengthMS <= 0 || InCurrentTimeMS < Section.AnchorTimestampMS) return;

	// --- 1. THE RAW BEAT LOOP (Under the hood) ---
	if (NextBeatTimestampMS <= 0 || NextBeatTimestampMS < InCurrentTimeMS - FMath::RoundToInt(Section.BeatLengthMS * 4)) {
		CurrentBeatInSession = FMath::FloorToInt((InCurrentTimeMS - Section.AnchorTimestampMS) / Section.BeatLengthMS) + 1;
	}
	NextBeatTimestampMS = Section.AnchorTimestampMS + FMath::RoundToInt(CurrentBeatInSession * Section.BeatLengthMS);

	while (InCurrentTimeMS >= NextBeatTimestampMS) {
		OnBeatTriggered.Broadcast(NextBeatTimestampMS / 1000.0f);
		CurrentBeatInSession++;
		NextBeatTimestampMS = Section.AnchorTimestampMS + FMath::RoundToInt(CurrentBeatInSession * Section.BeatLengthMS);
	}

	// --- 2. THE GAMEPLAY BEAT LOOP (The one the UI and Player cares about!) ---
	float GameplayBeatInterval = GetGameplayBeatIntervalMS();
	if (NextGameplayBeatTimestampMS <= 0) NextGameplayBeatTimestampMS = Section.AnchorTimestampMS;

	// Catch-up logic if lag occurs
	if (NextGameplayBeatTimestampMS < InCurrentTimeMS - FMath::RoundToInt(GameplayBeatInterval * 2)) {
		float TimeSinceAnchor = (float)(InCurrentTimeMS - Section.AnchorTimestampMS);
		int32 GameplayBeatsPassed = FMath::FloorToInt(TimeSinceAnchor / GameplayBeatInterval);
		NextGameplayBeatTimestampMS = Section.AnchorTimestampMS + FMath::RoundToInt((GameplayBeatsPassed + 1) * GameplayBeatInterval);
	}

	while (InCurrentTimeMS >= NextGameplayBeatTimestampMS) {
		OnGameplayBeatTriggered.Broadcast(NextGameplayBeatTimestampMS / 1000.0f);
		NextGameplayBeatTimestampMS += FMath::RoundToInt(GameplayBeatInterval);
	}
}