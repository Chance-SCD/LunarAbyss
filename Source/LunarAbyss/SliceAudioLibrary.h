#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SliceAudioLibrary.generated.h"

class USoundCue;
class USoundWave;

/** Editor helpers for the gray-box sound cues (used by Scripts/fix_sound_cues.py). */
UCLASS()
class LUNARABYSS_API ULASliceAudioLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** "FirstNode=<class>(wave=<path>, loop=<0/1>) ... | Duration=<s>" for a cue's node tree. */
	UFUNCTION(BlueprintCallable, Category = "LunarAbyss|Audio")
	static FString DescribeCue(USoundCue* Cue);

	/** Replaces the cue graph with a single WavePlayer (looping as given) and recompiles. Editor only. */
	UFUNCTION(BlueprintCallable, Category = "LunarAbyss|Audio")
	static bool BuildWaveCue(USoundCue* Cue, USoundWave* Wave, bool bLooping);
};
