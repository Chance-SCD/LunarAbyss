#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "CrewCharacter.generated.h"

class UCameraComponent;
class UInputAction;
class UInputMappingContext;

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
	/** Zero-g push-off: max drift speed and acceleration from input, m/s and m/s^2. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|ZeroG") float ZeroGMaxSpeed = 1.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|ZeroG") float ZeroGPushAccel = 0.6f;
	/** DT_Slice_GravityTuning (TENTATIVE). Damage = FallDamagePerMetre x (Drop - FallSafeHeight) x g at impact; drop from the feet. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Health") float FallSafeHeight = 3.0f;      // m
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Health") float FallDamagePerMetre = 15.f;  // HP/m
	/** Falls that start while gravity < 1 (restore ramp) are capped at this % of MaxHealth. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Health") float RestoreFallDamageCapPct = 40.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Crew|Health") float MaxHealth = 140.f;
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

	bool bZeroG = false;
	bool bWalking = false;
	float FallApexZ = 0.f;
	bool bFallStartedLowG = false;
	void StartFall();
	bool bTrackingFall = false;
};
