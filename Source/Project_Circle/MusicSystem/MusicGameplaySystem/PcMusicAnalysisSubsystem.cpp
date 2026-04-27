#include "PcMusicAnalysisSubsystem.h"
#include "Engine/DataTable.h"
#include "Project_Circle/MusicSystem/MusicImportSystem/PcMusicConfigurationData.h"

// =============================================================================
//  INIT
// =============================================================================

void UPcMusicAnalysisSubsystem::InitializePlayback(UPcMusicConfigurationData* SongConfig)
{
	ResetState();
	if (!SongConfig || !SongConfig->GeneratedRhythmProfile || !SongConfig->GeneratedNoteData) return;

	TargetGameplayBPM = SongConfig->TargetGameplayBPM > 0.f ? SongConfig->TargetGameplayBPM : 75.f;
	PulseNormal       = SongConfig->PulseNormal;
	PulseEnhanced     = SongConfig->PulseEnhanced;

	SongConfig->GeneratedRhythmProfile->ForeachRow<FPcRhythmSectionProfile>("Load",
		[&](const FName&, const FPcRhythmSectionProfile& Row) { RhythmSections.Add(Row); });

	SongConfig->GeneratedNoteData->ForeachRow<FPcRuntimeEvent>("Load",
		[&](const FName&, const FPcRuntimeEvent& Row) { RuntimeEvents.Add(Row); });

	if (!RhythmSections.IsEmpty() && !RuntimeEvents.IsEmpty())
		bIsReadyForPlayback = true;
}

void UPcMusicAnalysisSubsystem::ResetState()
{
	bIsReadyForPlayback          = false;
	RhythmSections.Empty();
	RuntimeEvents.Empty();
	NextEventIndex               = 0;
	LastProcessedMusicProgressMs = -1;
	CurrentSectionIndex          = 0;
	CurrentBeatInSession         = 0;
	NextBeatTimestampMS          = 0;
	NextGameplayBeatTimestampMS  = 0;
	CurrentBPM                   = 0.f;
	CurrentGameplayBPM           = 0.f;
	bCurrentSectionIsEnhanced    = false;
}

// =============================================================================
//  BPM AUTO-SUBDIVISION
//
//  Tries raw BPM / 1, / 2, / 4 and picks whichever lands closest to
//  TargetGameplayBPM.  If the section has an authored GameplayBPM override
//  (> 0) that is used directly instead.
// =============================================================================

float UPcMusicAnalysisSubsystem::ComputeGameplayBPM(float RawBPM, float Target) const
{
	float Best     = RawBPM;
	float BestDiff = FMath::Abs(RawBPM - Target);

	float Candidate = RawBPM;
	while (Candidate > 20.f)
	{
		Candidate *= 0.5f;
		const float Diff = FMath::Abs(Candidate - Target);
		if (Diff < BestDiff) { BestDiff = Diff; Best = Candidate; }
	}
	return FMath::Max(Best, 1.f);
}

// =============================================================================
//  PRESET
// =============================================================================

const FPcMovementPreset& UPcMusicAnalysisSubsystem::GetCurrentPulsePreset() const
{
	return bCurrentSectionIsEnhanced ? PulseEnhanced : PulseNormal;
}

FString UPcMusicAnalysisSubsystem::GetActivePresetName() const
{
	return bCurrentSectionIsEnhanced ? TEXT("ENHANCED") : TEXT("NORMAL");
}

// =============================================================================
//  TIME UPDATES
// =============================================================================

void UPcMusicAnalysisSubsystem::UpdateMusicTime(float CurrentTimeSeconds)
{
	if (!bIsReadyForPlayback) return;
	const int32 CurrentTimeMs = FMath::RoundToInt(CurrentTimeSeconds * 1000.f);
	if (CurrentTimeMs > LastProcessedMusicProgressMs)
	{
		ProcessMusicEvents();
		LastProcessedMusicProgressMs = CurrentTimeMs;
		OnSongProgress.Broadcast(CurrentTimeMs);
	}
}

float UPcMusicAnalysisSubsystem::GetTimeUntilNextGameplayBeat() const
{
	if (!bIsReadyForPlayback || NextGameplayBeatTimestampMS <= 0 || LastProcessedMusicProgressMs < 0) return 0.f;
	return FMath::Max(0.f, (float)(NextGameplayBeatTimestampMS - LastProcessedMusicProgressMs)) / 1000.f;
}

// Gameplay beat interval = 60000 / currentGameplayBPM  (milliseconds)
float UPcMusicAnalysisSubsystem::GetGameplayBeatIntervalMS() const
{
	if (!bIsReadyForPlayback || CurrentGameplayBPM <= 0.f) return 0.f;
	return 60000.f / CurrentGameplayBPM;
}

TArray<FPcRuntimeEvent> UPcMusicAnalysisSubsystem::GetUpcomingNotes(float LookaheadWindowSec) const
{
	TArray<FPcRuntimeEvent> Upcoming;
	if (!bIsReadyForPlayback) return Upcoming;
	const int32 MaxTimeMS = LastProcessedMusicProgressMs + FMath::RoundToInt(LookaheadWindowSec * 1000.f);
	for (int32 i = NextEventIndex; i < RuntimeEvents.Num(); ++i)
	{
		if (RuntimeEvents[i].TimestampMS > MaxTimeMS) break;
		if (RuntimeEvents[i].EventType == EPcRuntimeEventType::NoteHit)
			Upcoming.Add(RuntimeEvents[i]);
	}
	return Upcoming;
}

// =============================================================================
//  INTERNAL PROCESSING
// =============================================================================

void UPcMusicAnalysisSubsystem::ProcessMusicEvents()
{
	const int32 CurrentTimeMs = LastProcessedMusicProgressMs;
	UpdateRhythmSection(CurrentTimeMs);
	ProcessBeatTicks(CurrentTimeMs);

	while (NextEventIndex < RuntimeEvents.Num()
		&& RuntimeEvents[NextEventIndex].TimestampMS <= CurrentTimeMs)
	{
		const FPcRuntimeEvent& Event = RuntimeEvents[NextEventIndex];
		switch (Event.EventType)
		{
		case EPcRuntimeEventType::NoteHit:    OnNoteHit.Broadcast(Event.TimestampMS, Event.Value1, Event.Value2); break;
		case EPcRuntimeEventType::MeterChange: OnMeterChanged.Broadcast(Event.Value1); break;
		case EPcRuntimeEventType::BreakStart:  OnBreakStart.Broadcast(Event.TimestampMS, Event.Value1); break;
		case EPcRuntimeEventType::BreakEnd:    OnBreakEnd.Broadcast(Event.Value1, Event.TimestampMS); break;
		case EPcRuntimeEventType::SongEnd:     OnSongEnd.Broadcast(Event.TimestampMS / 1000.f); break;
		}
		NextEventIndex++;
	}
}

void UPcMusicAnalysisSubsystem::UpdateRhythmSection(int32 InCurrentTimeMS)
{
	if (RhythmSections.IsEmpty()) return;

	// Advance section index
	int32 NewSectionIndex = CurrentSectionIndex;
	while (NewSectionIndex < RhythmSections.Num() - 1
		&& InCurrentTimeMS >= RhythmSections[NewSectionIndex + 1].StartTimeMS)
	{
		NewSectionIndex++;
	}

	if (NewSectionIndex != CurrentSectionIndex || CurrentBPM == 0.f)
	{
		CurrentSectionIndex = NewSectionIndex;
		const FPcRhythmSectionProfile& Section = RhythmSections[CurrentSectionIndex];

		CurrentBPM = Section.BPM;
		OnBPMChanged.Broadcast(CurrentBPM);

		// ── Preset ────────────────────────────────────────────────────────────
		// Auto: Normal unless the section is explicitly tagged Enhanced.
		// The data table drives this per-section.  No subdivision math involved.
		const bool bNewEnhanced = (Section.MovementPreset == EPcMovementPresetOverride::Enhanced);
		if (bNewEnhanced != bCurrentSectionIsEnhanced)
		{
			bCurrentSectionIsEnhanced = bNewEnhanced;
			OnPresetChanged.Broadcast(bCurrentSectionIsEnhanced);
		}

		// ── Gameplay BPM ──────────────────────────────────────────────────────
		// ── Gameplay BPM ──────────────────────────────────────────────────────────────
		// Enhanced sections target TargetGameplayBPM * 2 (drop doubles the tempo).
		// Normal sections target TargetGameplayBPM.
		// Per-section override: set GameplayBPM > 0 in the data table to bypass auto.
		const float EffectiveTarget = bNewEnhanced ? TargetGameplayBPM * 2.f : TargetGameplayBPM;
		const float NewGameplayBPM = (Section.GameplayBPM > 0.f)
			? Section.GameplayBPM
			: ComputeGameplayBPM(Section.BPM, EffectiveTarget);

		if (!FMath::IsNearlyEqual(CurrentGameplayBPM, NewGameplayBPM))
		{
			CurrentGameplayBPM = NewGameplayBPM;
			OnGameplayBPMChanged.Broadcast(CurrentGameplayBPM);
		}

		CurrentBeatInSession        = 0;
		NextBeatTimestampMS         = Section.AnchorTimestampMS;
		NextGameplayBeatTimestampMS = Section.AnchorTimestampMS;
	}
}

void UPcMusicAnalysisSubsystem::ProcessBeatTicks(int32 InCurrentTimeMS)
{
	if (RhythmSections.IsEmpty()) return;
	const FPcRhythmSectionProfile& Section = RhythmSections[CurrentSectionIndex];
	if (Section.BeatLengthMS <= 0 || InCurrentTimeMS < Section.AnchorTimestampMS) return;

	// ── 1. Raw beat loop (internal, not player-facing) ────────────────────────
	if (NextBeatTimestampMS <= 0
		|| NextBeatTimestampMS < InCurrentTimeMS - FMath::RoundToInt(Section.BeatLengthMS * 4))
	{
		CurrentBeatInSession = FMath::FloorToInt(
			(InCurrentTimeMS - Section.AnchorTimestampMS) / Section.BeatLengthMS) + 1;
	}
	NextBeatTimestampMS = Section.AnchorTimestampMS
		+ FMath::RoundToInt(CurrentBeatInSession * Section.BeatLengthMS);

	while (InCurrentTimeMS >= NextBeatTimestampMS)
	{
		OnBeatTriggered.Broadcast(NextBeatTimestampMS / 1000.f);
		CurrentBeatInSession++;
		NextBeatTimestampMS = Section.AnchorTimestampMS
			+ FMath::RoundToInt(CurrentBeatInSession * Section.BeatLengthMS);
	}

	// ── 2. Gameplay beat loop (what the player plays to) ──────────────────────
	const float GameplayBeatInterval = GetGameplayBeatIntervalMS();
	if (GameplayBeatInterval <= 0.f) return;

	if (NextGameplayBeatTimestampMS <= 0)
		NextGameplayBeatTimestampMS = Section.AnchorTimestampMS;

	// Catch-up if lag occurs
	if (NextGameplayBeatTimestampMS < InCurrentTimeMS - FMath::RoundToInt(GameplayBeatInterval * 2))
	{
		const float TimeSinceAnchor   = (float)(InCurrentTimeMS - Section.AnchorTimestampMS);
		const int32 BeatsPassed       = FMath::FloorToInt(TimeSinceAnchor / GameplayBeatInterval);
		NextGameplayBeatTimestampMS   = Section.AnchorTimestampMS
			+ FMath::RoundToInt((BeatsPassed + 1) * GameplayBeatInterval);
	}

	while (InCurrentTimeMS >= NextGameplayBeatTimestampMS)
	{
		OnGameplayBeatTriggered.Broadcast(NextGameplayBeatTimestampMS / 1000.f);
		NextGameplayBeatTimestampMS += FMath::RoundToInt(GameplayBeatInterval);
	}
}