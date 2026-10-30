#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CorvetteHangar.generated.h"

class AFighterPawn;
class UStaticMeshComponent;
class UGravityEngineComponent;
class USoundBase;

UENUM(BlueprintType)
enum class EHangarDoorState : uint8 { Closed, Opening, Open, Closing };

/**
 * Gray-box Corvette 4C: kinematic hull + walkable interior + hangar launch + gravity engine.
 *
 * Frame of reference (see Docs/Slice_Notes.md): by default the corvette stays at the world origin
 * (bMoveInWorld = false) and its speed is a "virtual" ship-frame velocity. Crew walk on a static floor,
 * so there is no based-movement jitter or large-coordinate precision loss. Set bMoveInWorld = true to
 * actually translate the actor (relies on CharacterMovement based movement; for testing).
 */
UCLASS()
class LUNARABYSS_API ACorvetteHangar : public AActor
{
	GENERATED_BODY()

public:
	ACorvetteHangar();
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual FVector GetVelocity() const override { return bMoveInWorld ? CurrentVelocity * 100.f : FVector::ZeroVector; }

	/** Teleports the fighter to LaunchPoint and gives it the ship velocity plus LaunchSpeed out of the bay. */
	UFUNCTION(BlueprintCallable, Category = "Hangar")
	void LaunchFighter(AFighterPawn* Fighter);

	/** Re-reads the launch socket from the meshes and moves LaunchPoint to it. Returns true if a socket was found. */
	UFUNCTION(BlueprintCallable, Category = "Hangar")
	bool SnapLaunchPointToSocket();

	UFUNCTION(BlueprintPure, Category = "Hangar")
	AFighterPawn* GetParkedFighter() const { return ParkedFighter; }

	/** Ship-frame world position of an interior socket (falls back to hull). */
	bool GetSocketTransform(FName Socket, FTransform& Out) const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<UStaticMeshComponent> Hull;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<UStaticMeshComponent> Interior;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<USceneComponent> LaunchPoint;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<UGravityEngineComponent> GravityEngine;
	/** Stub dock point at hull socket DockCollar_Port (+X = outward, ship port side). Nothing docks yet. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<USceneComponent> DockPortPoint;

	// ---- Hangar doors (Forge 4C v6.3: attach at Mount_HangarDoor_L/R; push 2.9 m outboard, then swing 90 deg) ----
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<UStaticMeshComponent> HangarDoorL;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<UStaticMeshComponent> HangarDoorR;
	UPROPERTY(EditAnywhere, Category = "Meshes") TSoftObjectPtr<UStaticMesh> HangarDoorLAsset;
	UPROPERTY(EditAnywhere, Category = "Meshes") TSoftObjectPtr<UStaticMesh> HangarDoorRAsset;
	UFUNCTION(BlueprintCallable, Category = "Hangar") void OpenHangarDoors();
	UFUNCTION(BlueprintCallable, Category = "Hangar") void CloseHangarDoors();
	UFUNCTION(BlueprintCallable, Category = "Hangar") void ToggleHangarDoors();
	UFUNCTION(BlueprintPure, Category = "Hangar") EHangarDoorState GetHangarDoorState() const { return DoorState; }
	/** Request a launch: opens the doors first if needed, launches when fully open, closes CloseAfterLaunchDelay later. */
	UFUNCTION(BlueprintCallable, Category = "Hangar") void RequestLaunch(AFighterPawn* Fighter);

	/** Door timing from Echo-S/Forge notifies (30 fps): open PushStart f6, SwingStart f44, Settle f162; close SwingStart f6, SealStart f132, Sealed f172. */
	UPROPERTY(EditAnywhere, Category = "Hangar|Doors") float DoorOpenPushStart = 0.2f, DoorOpenSwingStart = 1.467f, DoorOpenSettle = 5.4f;
	UPROPERTY(EditAnywhere, Category = "Hangar|Doors") float DoorCloseSwingStart = 0.2f, DoorCloseSealStart = 4.4f, DoorCloseSealed = 5.733f;
	UPROPERTY(EditAnywhere, Category = "Hangar|Doors") float DoorPushDistance = 290.f;  // cm along ship +Y (Blender) = UE -Y
	UPROPERTY(EditAnywhere, Category = "Hangar|Doors") float DoorSwingDeg = 90.f;
	/** Horn lead-in before the doors move (SND_HGR_DOOR_001). */
	UPROPERTY(EditAnywhere, Category = "Hangar|Doors") float DoorHornLeadIn = 1.0f;
	UPROPERTY(EditAnywhere, Category = "Hangar|Doors") float CloseAfterLaunchDelay = 8.0f;
	UPROPERTY(EditAnywhere, Category = "Hangar|Doors") TSoftObjectPtr<USoundBase> DoorHornSound;
	UPROPERTY(EditAnywhere, Category = "Hangar|Doors") TSoftObjectPtr<USoundBase> DoorOpenSound;
	UPROPERTY(EditAnywhere, Category = "Hangar|Doors") TSoftObjectPtr<USoundBase> DoorCloseSound;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Docking") FName DockPortSocket = TEXT("DockCollar_Port");

	/** Fighter parking spot sockets tried in order (UE strips the SOCKET_ prefix on import; both forms are tried). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hangar")
	TArray<FName> LaunchSocketNames = { TEXT("Hangar_Spot_Fighter"), TEXT("Hangar") };
	/** Launch lane: fighter is placed at Threshold and pointed at Clear (Forge 4C v6.3 sockets). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hangar") FName LaunchThresholdSocket = TEXT("HangarLaunch_Threshold");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hangar") FName LaunchClearSocket = TEXT("HangarLaunch_Clear");
	/** Offset from the parking socket (cm). FighterC origin is ground centre (gear down), so 0 = on the deck. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hangar") FVector LaunchOffset = FVector(0.f, 0.f, 0.f);
	/** Extra push along LaunchPoint's X (out of the port aperture), m/s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hangar") float LaunchSpeed = 10.f;
	/** Seconds of Newtonian coast after launch before flight assist engages. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hangar") float LaunchCoastTime = 4.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hangar") bool bSpawnParkedFighter = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hangar") TSubclassOf<AFighterPawn> FighterClass;

	/** Corvette 4C flight (DT_Slice_ShipStats, TENTATIVE): m/s, m/s^2. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement") float MaxSpeed = 250.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement") float BoostSpeed = 320.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement") float Acceleration = 30.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement") bool bBoost = false;
	/** Commanded speed along the ship's forward axis, m/s (clamped to MaxSpeed). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement") float TargetSpeed = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement") bool bMoveInWorld = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movement") FVector CurrentVelocity = FVector::ZeroVector; // m/s

	UPROPERTY(EditAnywhere, Category = "Meshes") TSoftObjectPtr<UStaticMesh> HullMeshAsset;
	UPROPERTY(EditAnywhere, Category = "Meshes") TSoftObjectPtr<UStaticMesh> InteriorMeshAsset;

private:
	UPROPERTY() TObjectPtr<AFighterPawn> ParkedFighter;
	UPROPERTY() TObjectPtr<AFighterPawn> PendingLaunch;
	EHangarDoorState DoorState = EHangarDoorState::Closed;
	float DoorTime = 0.f;       // seconds since this cycle started (negative = horn lead-in)
	bool bDoorSoundPlayed = false;
	float CloseTimer = -1.f;
	FTransform DoorLRest, DoorRRest;
	void TickDoors(float Dt);
	void AttachDoors();
	void ApplyDoorPose(float Push01, float Swing01);
	void PlayDoorSound(const TSoftObjectPtr<USoundBase>& S);
};
