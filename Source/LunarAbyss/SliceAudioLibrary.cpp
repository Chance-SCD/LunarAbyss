#include "SliceAudioLibrary.h"
#include "Sound/SoundCue.h"
#include "Sound/SoundWave.h"
#include "Sound/SoundNode.h"
#include "Sound/SoundNodeWavePlayer.h"
#include "Sound/SoundNodeRandom.h"
#if WITH_EDITOR
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#endif

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

bool ULASliceAudioLibrary::BuildMultiWaveCue(USoundCue* Cue, const TArray<USoundWave*>& Waves, bool bLooping)
{
#if WITH_EDITOR
	TArray<USoundWave*> Valid;
	for (USoundWave* W : Waves) if (W) Valid.Add(W);
	if (!Cue || Valid.Num() == 0) return false;
	Cue->Modify();
	// drop the old graph so stale nodes do not survive in AllNodes
	if (UEdGraph* G = Cue->GetGraph())
	{
		// keep the Root node, drop every other editor node (otherwise opening the cue recompiles the old graph back in)
		TArray<UEdGraphNode*> Old = G->Nodes;
		for (UEdGraphNode* N : Old)
			if (N && !N->GetClass()->GetName().Contains(TEXT("Root"))) G->RemoveNode(N);
	}
	Cue->FirstNode = nullptr;
	Cue->AllNodes.Reset();
	TArray<USoundNode*> Players;
	for (USoundWave* W : Valid)
	{
		USoundNodeWavePlayer* P = Cue->ConstructSoundNode<USoundNodeWavePlayer>();
		P->SetSoundWave(W);
		P->bLooping = bLooping;
		Players.Add(P);
	}
	if (Players.Num() == 1)
	{
		Cue->FirstNode = Players[0];
	}
	else
	{
		USoundNodeRandom* R = Cue->ConstructSoundNode<USoundNodeRandom>();
		R->bRandomizeWithoutReplacement = true;
		// Vega fix: ConstructSoundNode already made the editor graph node with its default input pins (Random = 2).
		// Writing ChildNodes directly desyncs pins (SoundCueGraph.cpp:61 assert), so grow via InsertChildNode, which adds pins + weights.
		while (R->ChildNodes.Num() < Players.Num()) R->InsertChildNode(R->ChildNodes.Num());
		while (R->ChildNodes.Num() > Players.Num()) R->RemoveChildNode(R->ChildNodes.Num() - 1);
		for (int32 i = 0; i < Players.Num(); ++i) { R->ChildNodes[i] = Players[i]; if (R->Weights.IsValidIndex(i)) R->Weights[i] = 1.0f; }
		Cue->FirstNode = R;
	}
	Cue->LinkGraphNodesFromSoundNodes();
	Cue->CacheAggregateValues();
	Cue->PostEditChange();
	Cue->MarkPackageDirty();
	return GetCueWavePaths(Cue).Num() == Valid.Num();
#else
	return false;
#endif
}

static void CollectWaves(const USoundNode* N, TArray<FString>& Out, int32 Depth)
{
	if (!N || Depth > 8) return;
	if (const USoundNodeWavePlayer* P = Cast<USoundNodeWavePlayer>(N))
		if (const USoundWave* W = P->GetSoundWave()) Out.AddUnique(W->GetPackage()->GetName());
	for (const USoundNode* C : N->ChildNodes) CollectWaves(C, Out, Depth + 1);
}

TArray<FString> ULASliceAudioLibrary::GetCueWavePaths(USoundCue* Cue)
{
	TArray<FString> Out;
	if (Cue) CollectWaves(Cue->FirstNode, Out, 0);
	return Out;
}

float ULASliceAudioLibrary::GetCueDuration(USoundCue* Cue)
{
	return Cue ? Cue->GetDuration() : 0.f;
}
