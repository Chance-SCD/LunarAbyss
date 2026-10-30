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

	/** v1.8.1 (Echo-S): replaces the cue graph with WavePlayer -> Output (1 wave) or N WavePlayers -> Random -> Output
	 *  (every wave wired, equal weights, no repeats until all played). Recompiles; editor only. Returns true if every wave is reachable. */
	UFUNCTION(BlueprintCallable, Category = "LunarAbyss|Audio")
	static bool BuildMultiWaveCue(USoundCue* Cue, const TArray<USoundWave*>& Waves, bool bLooping);

	/** Wave paths reachable from the cue's FirstNode (for the check step). */
	UFUNCTION(BlueprintCallable, Category = "LunarAbyss|Audio")
	static TArray<FString> GetCueWavePaths(USoundCue* Cue);

	/** Cue duration as the engine computes it (INDEFINITELY_LOOPING_DURATION = 10000 for loops). */
	UFUNCTION(BlueprintCallable, Category = "LunarAbyss|Audio")
	static float GetCueDuration(USoundCue* Cue);
};
