
#include "PcMusicGameplayManager.h"
#include "PcMusicGameplaySubsystem.h"
#include "Components/AudioComponent.h"
#include "MetasoundOutput.h"
#include "MetasoundOutputSubsystem.h"
#include "Sound/SoundWave.h"
#include "MetasoundSource.h" 
#include "Project_Circle/MusicSystem/MusicImportSystem/PcMusicConfigurationData.h"

APcMusicGameplayManager::APcMusicGameplayManager()
{
	PrimaryActorTick.bCanEverTick = false; 
	
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootSceneComponent"));
	MusicAudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("MusicAudioComponent"));
	MusicAudioComponent->SetupAttachment(RootComponent);
	MusicAudioComponent->bAutoActivate = false;
}

void APcMusicGameplayManager::BeginPlay()
{
	Super::BeginPlay();
}

void APcMusicGameplayManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (OnMetasoundOutputValueChanged.IsBound())
    {
	    OnMetasoundOutputValueChanged.Unbind();
    }
	Super::EndPlay(EndPlayReason);
}


void APcMusicGameplayManager::StartMusicPlayback()
{
	// Updated validation to check for the single SongConfiguration asset
	if (!SongConfiguration)
	{
		UE_LOG(LogTemp, Error, TEXT("PcMusicManager: Missing essential assets. Check sound assets and the SongConfiguration."));
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
		return;
	
	if (UPcMusicGameplaySubsystem* MusicSubsystem = World->GetSubsystem<UPcMusicGameplaySubsystem>())
	{ 
		// Call the updated InitializePlayback function with the SongConfiguration asset
		MusicSubsystem->InitializePlayback(SongConfiguration);
	}
	
	MusicAudioComponent->Stop();
	if (OnMetasoundOutputValueChanged.IsBound())
	{
		OnMetasoundOutputValueChanged.Unbind();
	}

	MusicAudioComponent->SetSound(SongConfiguration->MainMusicMetaSound); 
	MusicAudioComponent->SetWaveParameter(FName("Song"), SongConfiguration->SongWaveAsset);

	OnMetasoundOutputValueChanged.BindUFunction(this, FName("OnMetaSoundTimeChanged"));

	if (UMetaSoundOutputSubsystem* MetaSoundOutputSubsystem = World->GetSubsystem<UMetaSoundOutputSubsystem>())
	{
		MetaSoundOutputSubsystem->WatchOutput(MusicAudioComponent, FName("CurrentTime"), OnMetasoundOutputValueChanged);
	}
	
	MusicAudioComponent->Play();
}

// No changes needed to OnMetaSoundTimeChanged or Tick
void APcMusicGameplayManager::OnMetaSoundTimeChanged(FName OutputName, const FMetaSoundOutput& Output)
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
				if (UPcMusicGameplaySubsystem* MusicSubsystem = World->GetSubsystem<UPcMusicGameplaySubsystem>())
				{
					MusicSubsystem->UpdateMusicTime(CurrentSongProgressInSeconds);
				}
			}
		}
	}
}

void APcMusicGameplayManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}