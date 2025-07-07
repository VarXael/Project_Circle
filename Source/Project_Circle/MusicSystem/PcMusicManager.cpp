// Fill out your copyright notice in the Description page of Project Settings.

#include "PcMusicManager.h"

#include "MetasoundOutput.h"
#include "MusicData.h"
#include "Components/AudioComponent.h"
#include "MetasoundOutputSubsystem.h"
#include "Kismet/GameplayStatics.h"

// Helper struct for the new analysis, local to this .cpp file
struct FSectionFeatures
{
    int32 StartTime = 0;
    int32 EndTime = 0;
    float BaseBeatLength = 500.f;
    float NotesPerSecond = 0.f;
    int32 DominantRhythmDelta = 500; // Note: Re-using this to store AvgAudioStrength temporarily
    bool bHasKiaiFlag = false;
    int32 AnchorTimestamp = 0;
};

APcMusicManager::APcMusicManager()
{
    PrimaryActorTick.bCanEverTick = false; // We don't need to tick.

    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootSceneComponent"));
    MusicAudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("MusicAudioComponent"));
    MusicAudioComponent->SetupAttachment(RootComponent);
    MusicAudioComponent->bAutoActivate = false;

    // Initialize Core Variables
    NextEventIndex = 0;
    CurrentMusicProgress = 0.0f;
    CurrentBPM = 0.0f;
    CurrentBeatLengthMs = 0.0f;
    bIsInHighIntensity = false;
    CurrentMeter = 4; // Default
    bInBreakPeriod = false;
    CurrentBreakEndTimeMS = 0;
    LastProcessedMusicProgressMs = -1;
    NextBeatTimestampMS = 0;

    // Initialize New Analysis System Variables
    CurrentRhythmSectionIndex = 0;
}

void APcMusicManager::BeginPlay()
{
    Super::BeginPlay();
}

void APcMusicManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (OnMetasoundOutputValueChanged.IsBound())
    {
        OnMetasoundOutputValueChanged.Unbind();
    }
    Super::EndPlay(EndPlayReason);
}

void APcMusicManager::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}

void APcMusicManager::StartMusicPlayback()
{
    if (MainMusicMetaSound && MusicAudioComponent && SongWaveAsset && DataTableMusicInfo)
    {
        if (LoadMusicDataFromTable() && LoadedMusicData.Num() > 0)
        {
            // --- This is the new, crucial step ---
            AnalyzeAndGenerateRhythmSections();
            // ------------------------------------

            // Reset Core Variables
            NextEventIndex = 0;
            CurrentMusicProgress = 0.0f;
            CurrentBPM = 0.0f;
            CurrentBeatLengthMs = 0.0f;
            bIsInHighIntensity = false;
            CurrentMeter = 4;
            bInBreakPeriod = false;
            CurrentBreakEndTimeMS = 0;
            LastProcessedMusicProgressMs = -1;
            NextBeatTimestampMS = 0;
            CurrentRhythmSectionIndex = 0;

            if (OnMetasoundOutputValueChanged.IsBound()) { OnMetasoundOutputValueChanged.Unbind(); }
            MusicAudioComponent->Stop();

            MusicAudioComponent->SetSound(MainMusicMetaSound);
            MusicAudioComponent->SetWaveParameter(FName("Song"), SongWaveAsset);
            MusicAudioComponent->Play();

            if (UWorld* World = GetWorld())
            {
                OnMetasoundOutputValueChanged.BindUFunction(this, FName("OnMetaSoundCurrentTimeChanged"));
                if (UMetaSoundOutputSubsystem* MetaSoundOutputSubsystem = World->GetSubsystem<UMetaSoundOutputSubsystem>())
                {
                    MetaSoundOutputSubsystem->WatchOutput(MusicAudioComponent, FName("CurrentTime"), OnMetasoundOutputValueChanged);
                }
            }
        }
    }
}

bool APcMusicManager::LoadMusicDataFromTable()
{
    if (!DataTableMusicInfo) return false;
    LoadedMusicData.Empty();
    DataTableMusicInfo->ForeachRow<FMusicData>(TEXT("Loading Music Data"),
        [this](const FName& Key, const FMusicData& Value)
        {
            LoadedMusicData.Add(Value);
        });
    return LoadedMusicData.Num() > 0;
}

// =========================================================================
// ==                 THE NEW, CORRECTED ANALYSIS FUNCTION                ==
// =========================================================================
void APcMusicManager::AnalyzeAndGenerateRhythmSections()
{
    RhythmSections.Empty();
    if (LoadedMusicData.Num() == 0) return;

    // --- Step 1: Pre-process and cache all relevant data types ---
    TArray<FMusicData> UninheritedTimingPoints;
    TArray<FMusicData> AllHitObjects;
    TArray<FMusicData> AllAudioBeats; // <-- NEW: We need this data now
    TArray<FMusicData> AllTimingPoints; // <-- NEW: For correct Kiai detection

    for (const FMusicData& Data : LoadedMusicData)
    {
        if (Data.EntryType == EGameplayEntryType::TimingPoint)
        {
            AllTimingPoints.Add(Data);
            if (Data.Uninherited == 1)
            {
                UninheritedTimingPoints.Add(Data);
            }
        }
        else if (Data.EntryType == EGameplayEntryType::HitObject)
        {
            AllHitObjects.Add(Data);
        }
        else if (Data.EntryType == EGameplayEntryType::AudioBeat)
        {
            AllAudioBeats.Add(Data);
        }
    }
    if (UninheritedTimingPoints.Num() == 0 || AllHitObjects.Num() == 0) return;

    // --- Step 2: Identify all structural boundaries (no change here, this is good) ---
    TSet<int32> BoundarySet;
    BoundarySet.Add(0); // Start of song is always a boundary
    for (const FMusicData& TP : UninheritedTimingPoints)
    {
        BoundarySet.Add(TP.TimestampMS);
    }
    bool bCurrentKiaiFlagState = false;
    for (const FMusicData& Data : LoadedMusicData)
    {
        if (Data.EntryType == EGameplayEntryType::TimingPoint)
        {
            if (((Data.Effects & 1) != 0) != bCurrentKiaiFlagState)
            {
                bCurrentKiaiFlagState = !bCurrentKiaiFlagState;
                BoundarySet.Add(Data.TimestampMS);
            }
        }
        else if (Data.EntryType == EGameplayEntryType::Break)
        {
            BoundarySet.Add(Data.TimestampMS);
            BoundarySet.Add(Data.BreakEndTimeMS);
        }
    }
    BoundarySet.Add(AllHitObjects.Last().TimestampMS + 5000); // End of song boundary
    TArray<int32> BoundaryTimestamps = BoundarySet.Array();
    BoundaryTimestamps.Sort();

    // --- Step 3: Multi-Pass Analysis (THE NEW LOGIC) ---
    
    // --- Pass A: Feature Extraction ---
    TArray<FSectionFeatures> FeatureList;
    float MaxNPS = 0.f;
    float MaxAvgStrength = 0.f;

    for (int32 i = 0; i < BoundaryTimestamps.Num() - 1; ++i)
    {
        FSectionFeatures Features;
        Features.StartTime = BoundaryTimestamps[i];
        Features.EndTime = BoundaryTimestamps[i + 1];
        const float SectionDurationSec = (Features.EndTime - Features.StartTime) / 1000.f;

        if (SectionDurationSec < 1.0f) continue; // Ignore very short sections to reduce noise

        // Correctly find the base beat length and anchor note for this section
        for (const FMusicData& TP : UninheritedTimingPoints)
        {
            if (TP.TimestampMS <= Features.StartTime) Features.BaseBeatLength = TP.BeatLength;
            else break;
        }
        bool bAnchorFound = false;
        int32 NoteCount = 0;
        for (const FMusicData& HitObject : AllHitObjects)
        {
            if (HitObject.TimestampMS >= Features.StartTime && HitObject.TimestampMS < Features.EndTime)
            {
                NoteCount++;
                if (!bAnchorFound)
                {
                    Features.AnchorTimestamp = HitObject.TimestampMS;
                    bAnchorFound = true;
                }
            }
        }
        
        if (NoteCount == 0) continue; // Skip sections with no notes (like breaks)

        // Correctly find the Kiai state for this section by checking ALL timing points
        bool lastKiaiState = false;
        for (const FMusicData& TP : AllTimingPoints)
        {
            if (TP.TimestampMS <= Features.StartTime) lastKiaiState = ((TP.Effects & 1) != 0);
            else break;
        }
        Features.bHasKiaiFlag = lastKiaiState;

        // Calculate NotesPerSecond
        Features.NotesPerSecond = NoteCount / SectionDurationSec;
        if (Features.NotesPerSecond > MaxNPS) MaxNPS = Features.NotesPerSecond;

        // NEW: Calculate Average Audio Strength for the section
        float TotalStrength = 0.f;
        int32 BeatCount = 0;
        for (const FMusicData& AudioBeat : AllAudioBeats)
        {
            if (AudioBeat.TimestampMS >= Features.StartTime && AudioBeat.TimestampMS < Features.EndTime)
            {
                TotalStrength += AudioBeat.AudioBeatStrength;
                BeatCount++;
            }
        }
        float AvgStrength = (BeatCount > 0) ? TotalStrength / BeatCount : 0.f;
        if (AvgStrength > MaxAvgStrength) MaxAvgStrength = AvgStrength;
        
        // We will normalize this later, for now just store it
        Features.DominantRhythmDelta = FMath::RoundToInt(AvgStrength); // Re-using this field to store avg strength

        FeatureList.Add(Features);
    }

    // --- Pass B: Contextual Evaluation and Final Rhythm Generation ---
    for (const FSectionFeatures& Features : FeatureList)
    {
        // Normalize the features to a 0-1 range to weigh them equally
        float NormalizedNPS = (MaxNPS > 0) ? Features.NotesPerSecond / MaxNPS : 0.f;
        float NormalizedStrength = (MaxAvgStrength > 0) ? static_cast<float>(Features.DominantRhythmDelta) / MaxAvgStrength : 0.f;
        
        // Calculate the final Intensity Score
        float IntensityScore = 0.f;
        IntensityScore += NormalizedNPS * 0.5f;      // Note density is 50% of the score
        IntensityScore += NormalizedStrength * 0.3f; // Audio energy is 30% of the score
        if (Features.bHasKiaiFlag) IntensityScore += 0.2f; // Kiai is a 20% bonus

        // --- THE NEW DECISION LOGIC ---
        // We no longer use a histogram. We use the mapper's timing as the ground truth.
        // We only decide if the gameplay should feel double-time based on our intensity score.
        bool bIsHighIntensity = IntensityScore > 0.6f; // Threshold for feeling "intense" (tweak this value!)
        
        float GameplayBeatLength = Features.BaseBeatLength; // Default to the mapper's intended beat
        if (bIsHighIntensity)
        {
            // If the section is intense, we use a double-time beat for gameplay.
            GameplayBeatLength = Features.BaseBeatLength / 2.0f;
        }

        FGameplayRhythmSection FinalSection;
        FinalSection.StartTimeMS = Features.StartTime;
        FinalSection.BeatLengthMS = GameplayBeatLength;
        FinalSection.BPM = 60000.0f / GameplayBeatLength;
        FinalSection.AnchorTimestampMS = Features.AnchorTimestamp > 0 ? Features.AnchorTimestamp : Features.StartTime;
        FinalSection.bIsHighIntensity = bIsHighIntensity;

        // Merge sections that are identical to the previous one to create clean, long sections
        if (RhythmSections.Num() == 0 || !FMath::IsNearlyEqual(FinalSection.BPM, RhythmSections.Last().BPM, 1.f) || FinalSection.bIsHighIntensity != RhythmSections.Last().bIsHighIntensity)
        {
            RhythmSections.Add(FinalSection);
            UE_LOG(LogTemp, Warning, TEXT("New Rhythm Section at %dms: %.2f BPM (BeatLength: %.2fms). High Intensity: %s (Score: %.2f)"), FinalSection.StartTimeMS, FinalSection.BPM, FinalSection.BeatLengthMS, bIsHighIntensity ? TEXT("Yes") : TEXT("No"), IntensityScore);
        }
    }
}
// =========================================================================
// ==              END OF THE NEW, CORRECTED ANALYSIS FUNCTION            ==
// =========================================================================


void APcMusicManager::OnMetaSoundCurrentTimeChanged(FName OutputName, const FMetaSoundOutput& Output)
{
    if (Output.IsValid() && Output.IsType<Metasound::FTime>())
    {
        Metasound::FTime RetrievedTimeStruct;
        if (Output.Get<Metasound::FTime>(RetrievedTimeStruct))
        {
            CurrentMusicProgress = static_cast<float>(RetrievedTimeStruct.GetSeconds());
            int32 CurrentMusicProgressMs = FMath::RoundToInt(CurrentMusicProgress * 1000.0f);

            if (CurrentMusicProgressMs > LastProcessedMusicProgressMs)
            {
                ProcessMusicEvents(CurrentMusicProgressMs);
                LastProcessedMusicProgressMs = CurrentMusicProgressMs;
            }
        }
    }
}

void APcMusicManager::ProcessMusicEvents(int32 InCurrentTimeMS)
{
    if (LoadedMusicData.Num() == 0) return;

    UpdateRhythmSection(InCurrentTimeMS);
    ProcessBeatTicks(InCurrentTimeMS);
    CheckForBreakEnd(InCurrentTimeMS);

    while (NextEventIndex < LoadedMusicData.Num())
    {
        const FMusicData& NextEvent = LoadedMusicData[NextEventIndex];
        if (InCurrentTimeMS < NextEvent.TimestampMS) break;

        if (NextEvent.EntryType == EGameplayEntryType::HitObject)
        {
            OnHitObjectTriggered(NextEvent.TimestampMS, NextEvent.HitObjectType);
        }
        else if (NextEvent.EntryType == EGameplayEntryType::TimingPoint)
        {
            UpdateMeter(NextEvent);
        }
        else if (NextEvent.EntryType == EGameplayEntryType::Break)
        {
            ProcessBreakStartEvent(NextEvent);
        }
        NextEventIndex++;
    }
}

void APcMusicManager::UpdateRhythmSection(int32 InCurrentTimeMS)
{
    if (RhythmSections.Num() == 0) return;

    int32 NewRhythmSectionIndex = CurrentRhythmSectionIndex;
    while (NewRhythmSectionIndex < RhythmSections.Num() - 1 &&
           InCurrentTimeMS >= RhythmSections[NewRhythmSectionIndex + 1].StartTimeMS)
    {
        NewRhythmSectionIndex++;
    }

    if (NewRhythmSectionIndex != CurrentRhythmSectionIndex || CurrentBPM == 0)
    {
        CurrentRhythmSectionIndex = NewRhythmSectionIndex;
        const FGameplayRhythmSection& CurrentSection = RhythmSections[CurrentRhythmSectionIndex];

        CurrentBPM = CurrentSection.BPM;
        CurrentBeatLengthMs = CurrentSection.BeatLengthMS;
        OnBPMChanged(CurrentBPM);
        
        if (bIsInHighIntensity != CurrentSection.bIsHighIntensity)
        {
            bIsInHighIntensity = CurrentSection.bIsHighIntensity;
            OnIntensityChanged(bIsInHighIntensity);
        }
        
        NextBeatTimestampMS = CurrentSection.AnchorTimestampMS;
    }
}

void APcMusicManager::ProcessBeatTicks(int32 InCurrentTimeMS)
{
    if (CurrentBeatLengthMs <= 0 || bInBreakPeriod) return;
    
    // If the next beat is way in the past, it means we've just come out of a break or a long pause.
    // We need to resync it to the current time to avoid a "storm" of catch-up beats.
    if (NextBeatTimestampMS < InCurrentTimeMS - FMath::RoundToInt(CurrentBeatLengthMs * 2))
    {
        // Resync to the beat grid instead of just the current time
        const FGameplayRhythmSection& CurrentSection = RhythmSections[CurrentRhythmSectionIndex];
        const float BeatsSinceAnchor = (InCurrentTimeMS - CurrentSection.AnchorTimestampMS) / CurrentSection.BeatLengthMS;
        const int32 CurrentBeatNumber = FMath::FloorToInt(BeatsSinceAnchor);
        NextBeatTimestampMS = CurrentSection.AnchorTimestampMS + FMath::RoundToInt((CurrentBeatNumber + 1) * CurrentSection.BeatLengthMS);
    }
    
    // Fire any beats that have occurred since the last frame.
    while (InCurrentTimeMS >= NextBeatTimestampMS)
    {
        OnBeatTriggered.Broadcast(NextBeatTimestampMS / 1000.0f);
        K2_OnBeatTriggered(NextBeatTimestampMS / 1000.0f);

        // Schedule the next beat
        NextBeatTimestampMS += FMath::RoundToInt(CurrentBeatLengthMs);
    }
}

void APcMusicManager::CheckForBreakEnd(int32 InCurrentTimeMS)
{
    if (bInBreakPeriod && InCurrentTimeMS >= CurrentBreakEndTimeMS)
    {
        bInBreakPeriod = false;
        OnBreakEnd(CurrentBreakEndTimeMS);
        // Let UpdateRhythmSection and ProcessBeatTicks handle the resync naturally
    }
}

void APcMusicManager::ProcessBreakStartEvent(const FMusicData& BreakData)
{
    bInBreakPeriod = true;
    CurrentBreakEndTimeMS = BreakData.BreakEndTimeMS;
    OnBreakStart(BreakData.TimestampMS, BreakData.BreakEndTimeMS);
}

void APcMusicManager::UpdateMeter(const FMusicData& TimingPointData)
{
    if (TimingPointData.Uninherited == 1 && TimingPointData.Meter != CurrentMeter)
    {
        CurrentMeter = TimingPointData.Meter;
        OnMeterChanged(CurrentMeter);
    }
}