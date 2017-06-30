// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Components/TextRenderComponent.h"
#include "Net/UnrealNetwork.h"
#include "Bomb.h"
#include "MyNetCharacter.generated.h"

UCLASS(config=Game)
class AMyNetCharacter : public ACharacter
{
	GENERATED_BODY()

	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	class USpringArmComponent* CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
	class UCameraComponent* FollowCamera;
public:
	AMyNetCharacter();

	/** Base turn rate, in deg/sec. Other scaling may affect final turn rate. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Camera)
	float BaseTurnRate;

	/** Base look up/down rate, in deg/sec. Other scaling may affect final rate. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=Camera)
	float BaseLookUpRate;

protected:

	/** Resets HMD orientation in VR. */
	void OnResetVR();

	/** Called for forwards/backward input */
	void MoveForward(float Value);

	/** Called for side to side input */
	void MoveRight(float Value);

	/** 
	 * Called via input to turn at a given rate. 
	 * @param Rate	This is a normalized rate, i.e. 1.0 means 100% of desired turn rate
	 */
	void TurnAtRate(float Rate);

	/**
	 * Called via input to turn look up/down at a given rate. 
	 * @param Rate	This is a normalized rate, i.e. 1.0 means 100% of desired turn rate
	 */
	void LookUpAtRate(float Rate);

	/** Handler for when a touch input begins. */
	void TouchStarted(ETouchIndex::Type FingerIndex, FVector Location);

	/** Handler for when a touch input stops. */
	void TouchStopped(ETouchIndex::Type FingerIndex, FVector Location);

protected:
	// APawn interface
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	// End of APawn interface

public:
	/** Returns CameraBoom subobject **/
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
	/** Returns FollowCamera subobject **/
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }

// ---------------- Network Logic
// -----------------------------
protected:

	/** The health of the character */
	UPROPERTY(VisibleAnywhere, Transient, ReplicatedUsing = OnRep_Health, Category = Stats)
	float Health;

	/** The max health of the character */
	UPROPERTY(EditAnywhere, Category = Stats)
	float MaxHealth = 100.f;

	/** The number of bombs that the character carries */
	UPROPERTY(VisibleAnywhere, Transient, ReplicatedUsing = OnRep_BombCount, Category = Stats)
	int32 BombCount;

	/** The max number of bombs that a character can have */
	UPROPERTY(EditAnywhere, Category = Stats)
	int32 MaxBombCount = 3;

	/** Text render component - used instead of UMG, to keep the tutorial short */
	UPROPERTY(VisibleAnywhere)
	UTextRenderComponent* CharText;

private:
	/** Called when the Health viriable gets updated */
	UFUNCTION()
	void OnRep_Health();

	/** Called when the BombCount variable gets updated */
	UFUNCTION()
	void OnRep_BombCount();

	/** Initialize Health */
	UFUNCTION()
	void InitHealth();

	/** Initialize bomb count */
	UFUNCTION()
	void InitBombCount();

	/** Updates the cahracter's textt to match with the updated status */
	void UpdateCharText();

public:

	/** Marks the properties we wish to replicate */
	virtual void GetLifetimeReplicatedProps( TArray< FLifetimeProperty > & OutLifetimeProps ) const;

	virtual void BeginPlay() override;

// ---------------- Network bombing
// -----------------------------
private:
	/**
	* TakeDamage Server version. Call this instead of TakeDamage when you're a client
	* You don't have to generate an implementation. It will automaticly call the ServerTakeDamage_Implementation function
	*/
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerTakeDamage(float Damage, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser);

	/** Contains the actual implementation of the ServerTakeDamage function */
	void ServerTakeDamage_Implementation(float Damage, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser);

	/** Validates the client. If the result is false teh client will be disconected */
	bool ServerTakeDamage_Validate(float Damage, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser);

	// Bomb related functions
	
	/** Will try to spawn a bomb */
	void AttempToSpawnBomb();

	/** Returns true if we can throw a bomb */
	bool HasBombs() { return BombCount > 0; }

	/**
	* Spawns a bomb. Call this function when you'ar authorized to.
	* In case you're not authorized, use the ServerSpawnBomb function
	*/
	void SpawnBomb();

	/**
	* SpawnBomb Server version. Call this insted of SpawnBomb when you're a client.
	* You don't have to generate an implementation for this. It will automaticly call to ServerSpawnBomb_Implementation function
	*/
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerSpawnBomb();

	/** Contains tha actual implementain of the ServerSpawnBomb function */
	void ServerSpawnBomb_Implementation();

	/** validates the client. If the result is false the client will be disconected */
	bool ServerSpawnBomb_Validate();

public:
	/** Applies damage to the character */
	virtual float TakeDamage(float Damage, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser);

public:
	/** Bomb Blueprint */
	UPROPERTY(EditAnywhere)
	TSubclassOf<ABomb> BombActorBP;

};

