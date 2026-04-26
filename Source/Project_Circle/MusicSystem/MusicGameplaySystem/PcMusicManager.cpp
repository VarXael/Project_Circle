#include "PcMusicManager.h"
#include "PcMusicAnalysisSubsystem.h"
#include "Components/AudioComponent.h"
#include "MetasoundOutputSubsystem.h"
#include "MetasoundSource.h"
#include "Project_Circle/MusicSystem/MusicImportSystem/PcMusicConfigurationData.h"

APcMusicManager::APcMusicManager()
{
	PrimaryActorTick.bCanEverTick = false; 
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootSceneComponent"));
	MusicAudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("MusicAudioComponent"));
	MusicAudioComponent->SetupAttachment(RootComponent);
}

void APcMusicManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (OnMetasoundOutputValueChanged.IsBound()) OnMetasoundOutputValueChanged.Unbind();
	Super::EndPlay(EndPlayReason);
}

void APcMusicManager::StartMusicPlayback()
{
	if (!MainMusicMetaSound || !SongConfiguration || !SongConfiguration->SongWaveAsset) return;

	if (UPcMusicAnalysisSubsystem* Subsystem = GetWorld()->GetSubsystem<UPcMusicAnalysisSubsystem>())
		Subsystem->InitializePlayback(SongConfiguration);
	
	MusicAudioComponent->Stop();
	if (OnMetasoundOutputValueChanged.IsBound()) OnMetasoundOutputValueChanged.Unbind();

	MusicAudioComponent->SetSound(MainMusicMetaSound); 
	MusicAudioComponent->SetWaveParameter(FName("Song"), SongConfiguration->SongWaveAsset);

	OnMetasoundOutputValueChanged.BindUFunction(this, FName("OnMetaSoundTimeChanged"));

	if (UMetaSoundOutputSubsystem* MetaOutput = GetWorld()->GetSubsystem<UMetaSoundOutputSubsystem>())
		MetaOutput->WatchOutput(MusicAudioComponent, FName("CurrentTime"), OnMetasoundOutputValueChanged);
	
	MusicAudioComponent->Play();
}

void APcMusicManager::OnMetaSoundTimeChanged(FName OutputName, const FMetaSoundOutput& Output)
{
	if (Output.IsValid() && Output.IsType<Metasound::FTime>())
	{
		Metasound::FTime RetrievedTimeStruct;
		if (Output.Get<Metasound::FTime>(RetrievedTimeStruct))
		{
			if (UPcMusicAnalysisSubsystem* Subsystem = GetWorld()->GetSubsystem<UPcMusicAnalysisSubsystem>())
				Subsystem->UpdateMusicTime(static_cast<float>(RetrievedTimeStruct.GetSeconds()));
		}
	}
}