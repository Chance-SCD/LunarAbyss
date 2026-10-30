#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MeshInfoLibrary.generated.h"

class UStaticMesh;

/** QA-156: Python can't read UStaticMesh::Sockets (protected property), so expose the real socket list. */
UCLASS()
class LUNARABYSS_API ULAMeshInfoLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category = "LunarAbyss|Mesh")
	static TArray<FString> GetSocketNames(UStaticMesh* Mesh);
};
