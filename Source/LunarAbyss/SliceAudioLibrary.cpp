#include "SliceAudioLibrary.h"
#include "Sound/SoundCue.h"
#include "Sound/SoundWave.h"
#include "Sound/SoundNode.h"
#include "Sound/SoundNodeWavePlayer.h"

static void DescribeNode(const USoundNode* N, FString& Out, int32 Depth)
{
	if (!N) { Out += TEXT("null"); return; }
	Out += N->GetClass()->GetName();
	if (const USoundNodeWavePlayer* P = Cast<USoundNodeWavePlayer>(N))
	{
		const USoundWave* W = P->GetSoundWave();
		Out += FString::Printf(TEXT("(wave=%s, loop=%d)"), W ? *W->GetPathName() : TEXT("NONE"), P->bLooping ? 1 : 0);
	}
	if (Depth < 8 && N->ChildNodes.Num() > 0)
	{
		Out += TEXT("[");
		for (int32 i = 0; i < N->ChildNodes.Num(); ++i)
		{
			if (i) Out += TEXT(", ");
			DescribeNode(N->ChildNodes[i], Out, Depth + 1);
		}
		Out += TEXT("]");
	}
}

FString ULASliceAudioLibrary::DescribeCue(USoundCue* Cue)
{
	if (!Cue) return TEXT("null cue");
	FString Out = TEXT("FirstNode=");
	DescribeNode(Cue->FirstNode, Out, 0);
	Out += FString::Printf(TEXT(" | Duration=%.3f"), Cue->GetDuration());
	return Out;
}

bool ULASliceAudioLibrary::BuildWaveCue(USoundCue* Cue, USoundWave* Wave, bool bLooping)
{
#if WITH_EDITOR
	if (!Cue || !Wave) return false;
	Cue->Modify();
	USoundNodeWavePlayer* Player = Cue->ConstructSoundNode<USoundNodeWavePlayer>();
	if (!Player) return false;
	Player->SetSoundWave(Wave);
	Player->bLooping = bLooping;
	Cue->FirstNode = Player;
	Cue->LinkGraphNodesFromSoundNodes();
	Cue->CacheAggregateValues();
	Cue->PostEditChange();
	Cue->MarkPackageDirty();
	return Cue->FirstNode == Player;
#else
	return false;
#endif
}
