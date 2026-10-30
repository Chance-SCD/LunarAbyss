#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "InputActionValue.h"
#include "FighterPawn.generated.h"

class UStaticMeshComponent;
class USpringArmComponent;
class UCameraComponent;
class UInputAction;
class UInputMappingContext;

/** Gray-box fighter with a kinematic 6DOF flight model. Tunables are in m/s and deg/s. */
UCLASS()
class LUNARABYSS_API AFighterPawn : public APawn
{
	GENERATED_BODY()

public:
	AFighterPawn();

	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual FVector GetVelocity() const override { return LinearVelocity; }
	virtual void PostInitializeComponents() override;
	virtual void UnPossessed() override;

	/** Parked fighters don't integrate flight (they ride the hangar LaunchPoint). */
	void SetParked(bool bNewParked) { bParked = bNewParked; if (bParked) LinearVelocity = FVector::ZeroVector; }
	UFUNCTION(BlueprintPure, Category = "Flight") bool IsParked() const { return bParked; }
	/** Ignore collisions with Other for Seconds (used when launching out of the bay). */
	void IgnoreActorForSeconds(AActor* Other, float Seconds);

	/** World velocity in cm/s (Unreal units). */
	UFUNCTION(BlueprintCallable, Category = "Flight")
	void SetLinearVelocity(const FVector& NewVelocityCmPerSec) { LinearVelocity = NewVelocityCmPerSec; }

	UFUNCTION(BlueprintPure, Category = "Flight")
	bool IsFlightAssistOn() const { return bFlightAssist; }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Hull;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USpringArmComponent> SpringArm;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCameraComponent> ChaseCamera;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCameraComponent> CockpitCamera;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flight|Linear") float MaxSpeed = 450.f;      // m/s (DT_Slice_ShipStats FighterC, TENTATIVE)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flight|Linear") float BoostSpeed = 600.f;    // m/s
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flight|Linear") float ForwardAccel = 120.f;  // m/s^2 (DT_Slice_ShipStats FighterC)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flight|Linear") float StrafeAccel = 25.f;    // m/s^2 (lateral + vertical)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flight|Linear") float BoostAccelMultiplier = 1.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flight|Angular") float PitchRate = 80.f;      // deg/s
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flight|Angular") float YawRate = 70.f;       // deg/s
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flight|Angular") float RollRate = 150.f;     // deg/s
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flight|Angular") float MouseSensitivity = 0.15f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flight") bool bFlightAssist = true;
	/** Gray-box mesh; falls back to a scaled cube if not imported yet. */
	UPROPERTY(EditAnywhere, Category = "Mesh") TSoftObjectPtr<UStaticMesh> FighterMeshAsset;

private:
	void BuildInput();
	void OnThrottle(const FInputActionValue& V) { InThrottle = V.Get<float>(); }
	void OnStrafe(const FInputActionValue& V)   { InStrafe = V.Get<float>(); }
	void OnLift(const FInputActionValue& V)     { InLift = V.Get<float>(); }
	void OnPitch(const FInputActionValue& V)    { InPitch += V.Get<float>(); }
	void OnYaw(const FInputActionValue& V)      { InYaw += V.Get<float>(); }
	void OnRoll(const FInputActionValue& V)     { InRoll = V.Get<float>(); }
	void OnBoostStart(const FInputActionValue&) { bBoost = true; }
	void OnBoostStop(const FInputActionValue&)  { bBoost = false; }
	void OnToggleAssist(const FInputActionValue&) { bFlightAssist = !bFlightAssist; }
	void OnToggleCamera(const FInputActionValue&);

	UPROPERTY() TObjectPtr<UInputMappingContext> FlightContext;
	UPROPERTY() TObjectPtr<UInputAction> ThrottleAction;
	UPROPERTY() TObjectPtr<UInputAction> StrafeAction;
	UPROPERTY() TObjectPtr<UInputAction> LiftAction;
	UPROPERTY() TObjectPtr<UInputAction> PitchAction;
	UPROPERTY() TObjectPtr<UInputAction> YawAction;
	UPROPERTY() TObjectPtr<UInputAction> RollAction;
	UPROPERTY() TObjectPtr<UInputAction> BoostAction;
	UPROPERTY() TObjectPtr<UInputAction> AssistAction;
	UPROPERTY() TObjectPtr<UInputAction> CameraAction;

	FVector LinearVelocity = FVector::ZeroVector; // cm/s, world space
	float InThrottle = 0.f, InStrafe = 0.f, InLift = 0.f, InPitch = 0.f, InYaw = 0.f, InRoll = 0.f;
	bool bBoost = false;
	bool bCockpitView = false;
	bool bParked = false;
};
