#include "MusicAnalysisSubsystem.h"
#include "AnalyzedSongData.h"
#include "Algo/Sort.h"

void UMusicAnalysisSubsystem::StartSongPlayback(USongConfigurationData* SongConfig)
{
	CurrentAnalyzedSong = UAnalyzedSongData::RunSongAnalysis(this, SongConfig);
	
	if (CurrentAnalyzedSong)
	{
		ResetPlaybackState();
		bIsReadyForPlayback = true;
	}
	else
	{
		bIsReadyForPlayback = false;
	}
}

void UMusicAnalysisSubsystem::ResetPlaybackState()
{
	bIsReadyForPlayback = false;
	NextEventIndex = 0;
	LastProcessedMusicProgressMs = -1;
	NextBeatTimestampMS = 0;
	CurrentRhythmSectionIndex = 0;
	CurrentBeatInSession = 0;
	CurrentBPM = 0.f;
	CurrentMeter = 4;
	CurrentBreakEndTimeMS = -1;
	NoteEventQueue.Empty();
}


void UMusicAnalysisSubsystem::UpdateMusicTime(float CurrentTimeSeconds)
{
	if (!bIsReadyForPlayback || !CurrentAnalyzedSong) return;

	const int32 CurrentTimeMs = FMath::RoundToInt(CurrentTimeSeconds * 1000.0f);
	if (CurrentTimeMs > LastProcessedMusicProgressMs)
	{
		ProcessMusicEvents();

		const int32 SongEndTimeMS = CurrentAnalyzedSong->GetAbsoluteSongEndTimeMS();
		if (SongEndTimeMS > 0 && LastProcessedMusicProgressMs < SongEndTimeMS && CurrentTimeMs >= SongEndTimeMS)
		{
			OnSongEnd.Broadcast(SongEndTimeMS / 1000.f);
			// To prevent re-firing, we can simply stop updating after song end.
			// A robust way is to set LastProcessedMusicProgressMs very high.
			LastProcessedMusicProgressMs = INT_MAX; 
		}
		else
		{
			LastProcessedMusicProgressMs = CurrentTimeMs;
		}
	}
}

void UMusicAnalysisSubsystem::GenerateSliderSubEvents(const FMusicData& SliderData)
{
	if (!(SliderData.HitObjectType & 2) || !CurrentAnalyzedSong) return;
	
	float BaseBeatLength = 500.f;
	const auto& MasterTPs = CurrentAnalyzedSong->GetMasterUninheritedTimingPoints();

	// Find the correct beat length for this slider
	if (MasterTPs.Num() > 0)
	{
		int32 BestTPTime = -1;
		for(const auto& Elem : MasterTPs)
		{
			if (Elem.Key <= SliderData.TimestampMS && Elem.Key > BestTPTime)
			{
				BaseBeatLength = Elem.Value.BeatLength;
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

void UMusicAnalysisSubsystem::ProcessMusicEvents()
{
	if (!CurrentAnalyzedSong) return;

	int32 CurrentTimeMs = LastProcessedMusicProgressMs;
	UpdateRhythmSection(CurrentTimeMs);
	ProcessBeatTicks(CurrentTimeMs);

	const auto& EventTimeline = CurrentAnalyzedSong->GetRuntimeEventTimeline();

	while (true) {
		const FMusicData* NextMajorEvent = (NextEventIndex < EventTimeline.Num()) ? &EventTimeline[NextEventIndex] : nullptr;
		FQueuedNoteEvent* NextSubEvent = (NoteEventQueue.Num() > 0) ? &NoteEventQueue.Last() : nullptr;
		int32 NextMajorEventTime = NextMajorEvent ? NextMajorEvent->TimestampMS : INT_MAX;
		int32 NextSubEventTime = NextSubEvent ? NextSubEvent->TimestampMS : INT_MAX;
		
		if (FMath::Min(NextMajorEventTime, NextSubEventTime) > CurrentTimeMs) break;

		if (NextMajorEventTime <= NextSubEventTime) {
			if (NextMajorEvent->EntryType == EGameplayEntryType::HitObject) {
				OnNoteHit.Broadcast(NextMajorEvent->TimestampMS, NextMajorEvent->HitObjectType, NextMajorEvent->HitSound);
				if (NextMajorEvent->HitObjectType & 2) GenerateSliderSubEvents(*NextMajorEvent);
			} else if (NextMajorEvent->EntryType == EGameplayEntryType::TimingPoint) {
				if(NextMajorEvent->Uninherited == 1 && NextMajorEvent->Meter != CurrentMeter) {
					CurrentMeter = NextMajorEvent->Meter; OnMeterChanged.Broadcast(CurrentMeter);
				}
			} else if (NextMajorEvent->EntryType == EGameplayEntryType::Break) {
				CurrentBreakEndTimeMS = NextMajorEvent->BreakEndTimeMS;
				OnBreakStart.Broadcast(NextMajorEvent->TimestampMS, NextMajorEvent->BreakEndTimeMS);
			}
			NextEventIndex++;
		} else {
			OnNoteHit.Broadcast(NextSubEvent->TimestampMS, NextSubEvent->NoteType, NextSubEvent->OriginalHitSound);
			NoteEventQueue.Pop();
		}
	}
	
	if (CurrentBreakEndTimeMS > 0 && LastProcessedMusicProgressMs < CurrentBreakEndTimeMS && CurrentTimeMs >= CurrentBreakEndTimeMS) {
		OnBreakEnd.Broadcast(0, CurrentBreakEndTimeMS);
	}
}

void UMusicAnalysisSubsystem::UpdateRhythmSection(int32 InCurrentTimeMS)
{
	if (!CurrentAnalyzedSong) return;
	const auto& RhythmSections = CurrentAnalyzedSong->GetRhythmSections();
	if(RhythmSections.Num() == 0) return;
	
	int32 NewRhythmSectionIndex = CurrentRhythmSectionIndex;
	while (NewRhythmSectionIndex < RhythmSections.Num() - 1 && InCurrentTimeMS >= RhythmSections[NewRhythmSectionIndex + 1].StartTimeMS)
	{
		NewRhythmSectionIndex++;
	}

	if (NewRhythmSectionIndex != CurrentRhythmSectionIndex || CurrentBPM == 0.f) {
		CurrentRhythmSectionIndex = NewRhythmSectionIndex;
		const FGameplayRhythmSection& CurrentSection = RhythmSections[CurrentRhythmSectionIndex];
		if(!FMath::IsNearlyEqual(CurrentBPM, CurrentSection.BPM)) {
			CurrentBPM = CurrentSection.BPM;
			OnBPMChanged.Broadcast(CurrentBPM);
		}
		CurrentBeatInSession = 0; 
		NextBeatTimestampMS = CurrentSection.AnchorTimestampMS;
	}
}

void UMusicAnalysisSubsystem::ProcessBeatTicks(int32 InCurrentTimeMS)
{
	if (InCurrentTimeMS < CurrentBreakEndTimeMS || !CurrentAnalyzedSong) return;
	const auto& RhythmSections = CurrentAnalyzedSong->GetRhythmSections();
	if (RhythmSections.Num() == 0) return;

	const FGameplayRhythmSection& CurrentSection = RhythmSections[CurrentRhythmSectionIndex];
	const float BeatLength = CurrentSection.BeatLengthMS;
	const int32 Anchor = CurrentSection.AnchorTimestampMS;

	if (BeatLength <= 0 || InCurrentTimeMS < Anchor) return;

	if (NextBeatTimestampMS < InCurrentTimeMS - FMath::RoundToInt(BeatLength * 4)) {
		const float BeatsPassed = (InCurrentTimeMS - Anchor) / BeatLength;
		CurrentBeatInSession = FMath::FloorToInt(BeatsPassed) + 1;
	}

	NextBeatTimestampMS = Anchor + FMath::RoundToInt(CurrentBeatInSession * BeatLength);

	while (InCurrentTimeMS >= NextBeatTimestampMS) {
		OnBeatTriggered.Broadcast(NextBeatTimestampMS / 1000.0f);
		CurrentBeatInSession++;
		NextBeatTimestampMS = Anchor + FMath::RoundToInt(CurrentBeatInSession * BeatLength);
	}
}