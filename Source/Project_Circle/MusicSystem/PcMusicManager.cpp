// Fill out your copyright notice in the Description page of Project Settings.

#include "PcMusicManager.h"
#include "MusicAnalysisSubsystem.h"
#include "Components/AudioComponent.h"
#include "MetasoundOutput.h"
#include "MetasoundOutputSubsystem.h"
#include "Sound/SoundWave.h"

// THIS IS THE FIX: Include the full definition of UMetaSoundSource
#include "MetasoundSource.h" 

APcMusicManager::APcMusicManager()
{
	PrimaryActorTick.bCanEverTick = false; 
	
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootSceneComponent"));
	MusicAudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("MusicAudioComponent"));
	MusicAudioComponent->SetupAttachment(RootComponent);
	MusicAudioComponent->bAutoActivate = false;
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


void APcMusicManager::StartMusicPlayback(float DifficultyBias)
{
	if (!MainMusicMetaSound || !SongWaveAsset)
	{
		UE_LOG(LogTemp, Error, TEXT("PcMusicManager: Missing essential assets to start playback."));
		return;
	}

	UWorld* World = GetWorld();
	if (!World) return;
	
	if (UMusicAnalysisSubsystem* MusicSubsystem = World->GetSubsystem<UMusicAnalysisSubsystem>())
	{ 
		MusicSubsystem->StartSongAnalysis(PrimaryDataTableMusicInfo,DataTableMusicInfo, DifficultyBias);
	}
	
	MusicAudioComponent->Stop();
	if (OnMetasoundOutputValueChanged.IsBound())
	{
		OnMetasoundOutputValueChanged.Unbind();
	}

	MusicAudioComponent->SetSound(MainMusicMetaSound.Get()); 
	MusicAudioComponent->SetWaveParameter(FName("Song"), SongWaveAsset.Get());

	OnMetasoundOutputValueChanged.BindUFunction(this, FName("OnMetaSoundTimeChanged"));

	if (UMetaSoundOutputSubsystem* MetaSoundOutputSubsystem = World->GetSubsystem<UMetaSoundOutputSubsystem>())
	{
		MetaSoundOutputSubsystem->WatchOutput(MusicAudioComponent, FName("CurrentTime"), OnMetasoundOutputValueChanged);
	}
	
	MusicAudioComponent->Play();
}

void APcMusicManager::OnMetaSoundTimeChanged(FName OutputName, const FMetaSoundOutput& Output)
{
	if (Output.IsValid() && Output.IsType<Metasound::FTime>())
	{
		Metasound::FTime RetrievedTimeStruct;
		if (Output.Get<Metasound::FTime>(RetrievedTimeStruct))
		{
			CurrentSongProgressInSeconds = static_cast<float>(RetrievedTimeStruct.GetSeconds());
			CurrentSongProgressInMs = FMath::RoundToInt(CurrentSongProgressInSeconds * 1000.0f);

			if (UWorld* World = GetWorld())
			{
				if (UMusicAnalysisSubsystem* MusicSubsystem = World->GetSubsystem<UMusicAnalysisSubsystem>())
				{
					MusicSubsystem->UpdateMusicTime(CurrentSongProgressInSeconds);
				}
			}
		}
	}
}

void APcMusicManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}