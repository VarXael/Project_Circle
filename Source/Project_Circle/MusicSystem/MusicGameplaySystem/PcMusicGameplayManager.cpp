#include "PcMusicGameplayManager.h"
#include "PcMusicDirectorSubsystem.h"
#include "Components/AudioComponent.h"
#include "AbilitySystemComponent.h"      // NEW: Required include for ASC
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
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(false);
}

UAbilitySystemComponent* APcMusicGameplayManager::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
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
	if (!SongConfiguration)
	{
		UE_LOG(LogTemp, Error, TEXT("PcMusicManager: Missing essential assets. Check sound assets and the SongConfiguration."));
		return;
	}

	UWorld* World = GetWorld();
	if (!World) return;
	
	if (UPcMusicDirectorSubsystem* MusicSubsystem = World->GetSubsystem<UPcMusicDirectorSubsystem>())
	{ 
		// MODIFIED: The call to InitializePlayback now passes 'this' to provide the world context.
		MusicSubsystem->InitializePlayback(SongConfiguration, this);
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
				if (UPcMusicDirectorSubsystem* MusicSubsystem = World->GetSubsystem<UPcMusicDirectorSubsystem>())
				{
					MusicSubsystem->UpdateMusicTime(CurrentSongProgressInSeconds);
				}
			}
		}
	}
}

// Tick remains empty as per your original design.
void APcMusicGameplayManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}