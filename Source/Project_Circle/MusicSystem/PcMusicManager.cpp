// Fill out your copyright notice in the Description page of Project Settings.


#include "PcMusicManager.h"

#include "MetasoundOutput.h"
#include "MusicData.h"
#include "Components/AudioComponent.h"

#include "CoreMinimal.h"
#include "MetasoundGeneratorHandle.h"
#include "MetasoundOutputSubsystem.h"
#include "MetasoundSource.h"
#include "GameFramework/Info.h"
#include "Engine/DataTable.h"

APcMusicManager::APcMusicManager()
{
	// Enable this actor to call Tick() every frame.
	PrimaryActorTick.bCanEverTick = true;
	
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootSceneComponent"));

	MusicAudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("MusicAudioComponent"));
	MusicAudioComponent->SetupAttachment(RootComponent);
	MusicAudioComponent->bAutoActivate = false;
}

// Called when the game starts or when spawned
void APcMusicManager::BeginPlay()
{
	Super::BeginPlay();
	NextEventIndex = 0;
	
}

// Called every frame
void APcMusicManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

bool APcMusicManager::LoadMusicDataFromTable()
{
	if (DataTableMusicInfo)
	{
		LoadedMusicData.Empty();

		DataTableMusicInfo->ForeachRow<FMusicData>(TEXT("Loading Music Data from DataTable"), 
			[this](const FName& Key, const FMusicData& Value)
			{
				LoadedMusicData.Add(Value);
			});

		LoadedMusicData.Sort([](const FMusicData& A, const FMusicData& B)
		{
			return A.TimestampMS < B.TimestampMS;
		});
		return true;
	}
	return false;
}

void APcMusicManager::OnMetaSoundCurrentTimeChanged(FName OutputName, const FMetaSoundOutput& Output)
{
	if (Output.IsValid() && Output.IsType<Metasound::FTime>())
	{
		Metasound::FTime RetrievedTimeStruct; 
        
		if (Output.Get<Metasound::FTime>(RetrievedTimeStruct))
		{
			CurrentMusicProgress = static_cast<float>(RetrievedTimeStruct.GetSeconds()); 
          
			CurrentMusicProgressMS = static_cast<int32>(CurrentMusicProgress * 1000.0f);
			MusicTick(CurrentMusicProgressMS);

			ProcessMusicEvents(CurrentMusicProgressMS);
		}
	}
}

void APcMusicManager::StartMusicPlayback()
{
	if (MainMusicMetaSound && MusicAudioComponent)
	{
		LoadMusicDataFromTable();
		if (LoadedMusicData.Num() > 0)
		{
			NextEventIndex = 0; 
			MusicAudioComponent->SetSound(MainMusicMetaSound);
			MusicAudioComponent->SetWaveParameter(FName("Song"), SongWaveAsset);
			MusicAudioComponent->Play();

			if (UWorld* World = GetWorld())
			{
				OnMetasoundOutputValueChanged.BindUFunction(this, FName("OnMetaSoundCurrentTimeChanged"));
				
				UMetaSoundOutputSubsystem* MetaSoundOutputSubsystem = World->GetSubsystem<UMetaSoundOutputSubsystem>();
				bool bWatchStarted = MetaSoundOutputSubsystem->WatchOutput(MusicAudioComponent,
					FName("CurrentTime"),
					OnMetasoundOutputValueChanged
				);
				
			}
			else
			{
				UE_LOG(LogTemp, Error, TEXT("APcMusicManager has no valid World. Cannot get UMetaSoundOutputSubsystem."));
			}
		}
	}
}

void APcMusicManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if(OnMetasoundOutputValueChanged.IsBound())
	{
		OnMetasoundOutputValueChanged.Unbind();
	}
    Super::EndPlay(EndPlayReason);
}

bool APcMusicManager::GetMusicDataAt(const TArray<FMusicData>& SortedArray, int32 SongProgress,
                                     FMusicData& OutMusicData)
{
	OutMusicData = FMusicData();

	if (SortedArray.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("GetMusicDataAt: Input array is empty."));
		return false;
	}

	auto It = std::lower_bound(SortedArray.GetData(), SortedArray.GetData() + SortedArray.Num(), SongProgress,
		[](const FMusicData& Element, int32 Value)
		{
			return Element.TimestampMS < Value;
		});

	if (It == SortedArray.GetData() + SortedArray.Num())
	{
		UE_LOG(LogTemp, Warning, TEXT("GetMusicDataAt: No FMusicData entry with TimestampMS >= %d found in the array."), SongProgress);
		return false;
	}
	else
	{
		OutMusicData = *It;
		return true;
	}
}

void APcMusicManager::ProcessMusicEvents(int32 InCurrentTimeMS)
{
	while (NextEventIndex < LoadedMusicData.Num())
	{
		const FMusicData& NextMusicEvent = LoadedMusicData[NextEventIndex];
		if (InCurrentTimeMS >= NextMusicEvent.TimestampMS)
		{
			OnMusicEventTriggered(NextMusicEvent); 
			NextEventIndex++; 
		}
		else
		{
			break; 
		}
	}
}

