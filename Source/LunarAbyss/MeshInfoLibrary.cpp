#include "MeshInfoLibrary.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"

TArray<FString> ULAMeshInfoLibrary::GetSocketNames(UStaticMesh* Mesh)
{
	TArray<FString> Out;
	if (Mesh) for (const UStaticMeshSocket* S : Mesh->Sockets) if (S) Out.Add(S->SocketName.ToString());
	return Out;
}
