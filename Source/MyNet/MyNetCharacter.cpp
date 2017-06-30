// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MyNetCharacter.h"
#include "Kismet/HeadMountedDisplayFunctionLibrary.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"

//////////////////////////////////////////////////////////////////////////
// AMyNetCharacter

AMyNetCharacter::AMyNetCharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	// set our turn rates for input
	BaseTurnRate = 45.f;
	BaseLookUpRate = 45.f;

	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true; // Character moves in the direction of input...	
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 540.0f, 0.0f); // ...at this rotation rate
	GetCharacterMovement()->JumpZVelocity = 600.f;
	GetCharacterMovement()->AirControl = 0.2f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 300.0f; // The camera follows at this distance behind the character	
	CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	FollowCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named MyCharacter (to avoid direct content references in C++)


	// ---------------- Network Logic
	// -----------------------------
	CharText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("CharText"));
	CharText->SetRelativeLocation(FVector(0, 0, 100));
	CharText->SetupAttachment(RootComponent);

	bReplicates = true;
}

//////////////////////////////////////////////////////////////////////////
// Input

void AMyNetCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	// Set up gameplay key bindings
	check(PlayerInputComponent);
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);

	PlayerInputComponent->BindAxis("MoveForward", this, &AMyNetCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &AMyNetCharacter::MoveRight);

	// We have 2 versions of the rotation bindings to handle different kinds of devices differently
	// "turn" handles devices that provide an absolute delta, such as a mouse.
	// "turnrate" is for devices that we choose to treat as a rate of change, such as an analog joystick
	PlayerInputComponent->BindAxis("Turn", this, &APawn::AddControllerYawInput);
	PlayerInputComponent->BindAxis("TurnRate", this, &AMyNetCharacter::TurnAtRate);
	PlayerInputComponent->BindAxis("LookUp", this, &APawn::AddControllerPitchInput);
	PlayerInputComponent->BindAxis("LookUpRate", this, &AMyNetCharacter::LookUpAtRate);

	// handle touch devices
	PlayerInputComponent->BindTouch(IE_Pressed, this, &AMyNetCharacter::TouchStarted);
	PlayerInputComponent->BindTouch(IE_Released, this, &AMyNetCharacter::TouchStopped);

	// VR headset functionality
	PlayerInputComponent->BindAction("ResetVR", IE_Pressed, this, &AMyNetCharacter::OnResetVR);

	// ---------------- Network bombing
	// -----------------------------
	PlayerInputComponent->BindAction("ThrowBomb", IE_Pressed, this, &AMyNetCharacter::AttempToSpawnBomb);
}


void AMyNetCharacter::OnResetVR()
{
	UHeadMountedDisplayFunctionLibrary::ResetOrientationAndPosition();
}

void AMyNetCharacter::TouchStarted(ETouchIndex::Type FingerIndex, FVector Location)
{
		Jump();
}

void AMyNetCharacter::TouchStopped(ETouchIndex::Type FingerIndex, FVector Location)
{
		StopJumping();
}

void AMyNetCharacter::TurnAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerYawInput(Rate * BaseTurnRate * GetWorld()->GetDeltaSeconds());
}

void AMyNetCharacter::LookUpAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerPitchInput(Rate * BaseLookUpRate * GetWorld()->GetDeltaSeconds());
}

void AMyNetCharacter::MoveForward(float Value)
{
	if ((Controller != NULL) && (Value != 0.0f))
	{
		// find out which way is forward
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		AddMovementInput(Direction, Value);
	}
}

void AMyNetCharacter::MoveRight(float Value)
{
	if ( (Controller != NULL) && (Value != 0.0f) )
	{
		// find out which way is right
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);
	
		// get right vector 
		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
		// add movement in that direction
		AddMovementInput(Direction, Value);
	}
}

// ---------------- Network Logic
// -----------------------------

void AMyNetCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	//Tell the engine to call the OnRep_Health and OnRep_BombCount each time
	//a variable changes
	DOREPLIFETIME(AMyNetCharacter, Health);
	DOREPLIFETIME(AMyNetCharacter, BombCount);
}


void AMyNetCharacter::OnRep_Health()
{
	UpdateCharText();
}

void AMyNetCharacter::OnRep_BombCount()
{
	UpdateCharText();
}

void AMyNetCharacter::InitHealth()
{
	Health = MaxHealth;
	UpdateCharText();
}

void AMyNetCharacter::InitBombCount()
{
	BombCount = MaxBombCount;
	UpdateCharText();
}

void AMyNetCharacter::UpdateCharText()
{
	// Create string that will display the health and bomb values;
	FString NextText = FString("Health: ") + FString::SanitizeFloat(Health) +
		FString(" BombCount: ") + FString::FromInt(BombCount);


	// Set the created string to the render comp
	CharText->SetText(FText::FromString(NextText));
}

void AMyNetCharacter::BeginPlay()
{
	Super::BeginPlay();

	InitHealth();
	InitBombCount();
}

// ---------------- Network bombing
// -----------------------------
float AMyNetCharacter::TakeDamage(float Damage, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	Super::TakeDamage(Damage, DamageEvent, EventInstigator, DamageCauser);


	// Decrease the character's hp
	Health -= Damage;
	if (Health <= 0)
	{
		InitHealth();
	}

	// Call the update text on the local client
	// OnRep_health will be changed in every other client so the cahracter's text
	// will be contain a text with the right values
	UpdateCharText();

	return Health;
}

void AMyNetCharacter::ServerTakeDamage_Implementation(float Damage, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	TakeDamage(Damage, DamageEvent, EventInstigator, DamageCauser);
}


bool AMyNetCharacter::ServerTakeDamage_Validate(float Damage, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	// Assume that everithing is  ok withour any futher checks and return true
	return true;
}

void AMyNetCharacter::AttempToSpawnBomb()
{
	// If we don't have authority, ameaning that we're not the server
	// tell the server to spawn the bomb.
	// IF we're the server, just spawn the bomb = we trust ourselfs.
	if (Role < ROLE_Authority)
	{
		ServerSpawnBomb();
	}
	else
	{
		SpawnBomb();
	}
}

void AMyNetCharacter::ServerSpawnBomb_Implementation()
{
	SpawnBomb();
}

bool AMyNetCharacter::ServerSpawnBomb_Validate()
{
	return true;
}

void AMyNetCharacter::SpawnBomb()
{
	// Decrease the bomb count and update the text in teh local client
	// OnRep_BombCount will be called in every other client
	BombCount--;
	UpdateCharText();

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Instigator = this;
	SpawnParameters.Owner = GetController();

	// Spawn the bomb
	GetWorld()->SpawnActor<ABomb>(BombActorBP, GetActorLocation() + GetActorForwardVector() * 200, GetActorRotation(), SpawnParameters);
}