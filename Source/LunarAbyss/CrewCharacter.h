#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "Engine/DataTable.h"
#include "CrewCharacter.generated.h"

class UCameraComponent;
class UInputAction;
class UInputMappingContext;
class ALadderVolume;

/** First-person on-foot crew. Speeds in m/s: walk 1.25, jog 3.0, crouch 1.0. Zero-g handled by UGravityEngineComponent. */
UCLASS()
class LUNARABYSS_API ACrewCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ACrewCharacter();
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void Landed(const FHitResult& Hit) override;
	virtual void OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode) override;

	void EnterZeroG();
	void ExitZeroG(float StartGravityScale);
	void SetShipGravityScale(float Scale);
	UFUNCTION(BlueprintPure, Category = "Crew") bool IsInZeroG() const { return bZeroG; }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components") TObjectPtr<UCameraComponent> FirstPersonCamera;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Speed") float WalkSpeed = 1.25f;   // m/s
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Speed") float JogSpeed = 3.0f;     // m/s
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Speed") float CrouchSpeed = 1.0f;  // m/s
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Speed") float SprintSpeed = 5.5f;  // m/s, Left Shift, forward only
	/** Zero-g push-off: max drift speed and acceleration from input, m/s and m/s^2. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|ZeroG") float ZeroGMaxSpeed = 1.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|ZeroG") float ZeroGPushAccel = 0.6f;
	/** Ship_Combat_Stats_Spec v0.4.5 (TENTATIVE): from impact speed v (m/s), Damage = FallDamagePerMetre x (v^2 / (2 x 9.81) - FallSafeHeight). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Health") float FallSafeHeight = 3.0f;      // m
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Health") float FallDamagePerMetre = 15.f;  // HP/m
	/** Only falls that start during a gravity restore (engine online, ramp < 1) are capped at this % of MaxHealth. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Health") float RestoreFallDamageCapPct = 40.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Health") float MaxHealth = 140.f;
	/** Jump tuning (DT_Slice_MoveTuning row "Default" overrides these at BeginPlay if the table exists). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Jump") float BaseJumpHeightM = 0.6f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Jump") float JumpGravityExponent = 0.25f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Jump") float IndoorJumpHeightCapM = 1.4f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Jump") float MagBootBelowG = 0.1f;
	UPROPERTY(EditAnywhere, Category = "Crew|Jump") TSoftObjectPtr<UDataTable> MoveTuningTable;
	/** Indoors flag (set by the ship's gravity engine while inside its interior bounds; a volume can set it too). */
	UFUNCTION(BlueprintCallable, Category = "Crew") void SetIndoors(bool b) { bIndoors = b; }
	UFUNCTION(BlueprintPure, Category = "Crew") bool IsIndoors() const { return bIndoors; }
	/** Stub: true when gravity < MagBootBelowG (no gameplay effect yet). */
	UFUNCTION(BlueprintPure, Category = "Crew") bool IsMagBootMode() const { return bMagBootMode; }
	virtual void BeginPlay() override;

	// ---- Ladder (QA-136) ----
	void LadderOverlapBegin(ALadderVolume* L);
	void LadderOverlapEnd(ALadderVolume* L);
	UFUNCTION(BlueprintPure, Category = "Crew") bool IsOnLadder() const { return Ladder != nullptr; }
	/** Test hook: ladder axis input (+1 climb, 0 release). */
	void SetLadderInput(float V) { LadderInput = V; }
	/** Set by the gravity engine while it is ramping back in (restore). */
	void SetGravityRestoring(bool b) { bGravityRestoring = b; }
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Crew|Health") float Health = 140.f;

	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

private:
	void BuildInput();
	void OnMove(const FInputActionValue& V);
	void OnLook(const FInputActionValue& V);
	void OnUpDown(const FInputActionValue& V);
	void OnJump(const FInputActionValue&);
	void OnCrouchToggle(const FInputActionValue&);
	void OnWalkToggle(const FInputActionValue&);
	void UpdateSpeeds();

	UPROPERTY() TObjectPtr<UInputMappingContext> FootContext;
	UPROPERTY() TObjectPtr<UInputAction> MoveAction;
	UPROPERTY() TObjectPtr<UInputAction> LookAction;
	UPROPERTY() TObjectPtr<UInputAction> UpDownAction;
	UPROPERTY() TObjectPtr<UInputAction> JumpAction;
	UPROPERTY() TObjectPtr<UInputAction> CrouchAction;
	UPROPERTY() TObjectPtr<UInputAction> WalkAction;
	UPROPERTY() TObjectPtr<UInputAction> SprintAction;
	void OnSprintStart(const FInputActionValue&) { bSprintHeld = true; }
	void OnSprintStop(const FInputActionValue&) { bSprintHeld = false; bSprinting = false; UpdateSpeeds(); }
	bool bSprintHeld = false;
	bool bSprinting = false;

	bool bZeroG = false;
	bool bWalking = false;
	float FallApexZ = 0.f;
	bool bFallStartedLowG = false;
	bool bGravityRestoring = false;
	bool bIndoors = false;
	UPROPERTY() TObjectPtr<ALadderVolume> Ladder;
	UPROPERTY() TObjectPtr<ALadderVolume> LadderCooldown; // just exited this one; re-grab needs up input or leaving it
	float LadderInput = 0.f;
	float ShipGravity = 1.f;
	float LadderZeroGVz = 0.f;
	void EnterLadder(ALadderVolume* L);
	void ExitLadder(bool bTop);
	void TickLadder(float Dt);
	bool bMagBootMode = false;
	void StartFall();
	bool bTrackingFall = false;
};
