// Fill out your copyright notice in the Description page of Project Settings.


#include "Bomb.h"
#include "MyNet.h"
#include "Kismet/GameplayStatics.h"


// Sets default values
ABomb::ABomb()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	SphereComp = CreateDefaultSubobject<USphereComponent>(TEXT("SphereComp"));
	SetRootComponent(SphereComp);

	SM = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SM"));
	SM->SetupAttachment(SphereComp);

	ProjectileMovementComp = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovementComp"));
	ProjectileMovementComp->bShouldBounce = true;

	// Since we need to replicate some functionality
	// for thi actor, we need to mark it as true
	SetReplicates(true);

}

// Called when the game starts or when spawned
void ABomb::BeginPlay()
{
	Super::BeginPlay();

	// register that funciton that will be calles in any bounce event
	ProjectileMovementComp->OnProjectileBounce.AddDynamic(this, &ABomb::OnProjectileBounce);
	
}

// Called every frame
void ABomb::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void ABomb::GetLifetimeReplicatedProps( TArray< FLifetimeProperty > & OutLifetimeProps ) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// Tell the engine that we will wish to replicate the bIsArmed variable
	DOREPLIFETIME(ABomb, bIsArmed);
}

void ABomb::ArmBomb()
{
	if (bIsArmed)
	{
		// Change the base color of static mesh to red
		UMaterialInstanceDynamic* DynamicMAT = SM->CreateAndSetMaterialInstanceDynamic(0);

		DynamicMAT->SetVectorParameterValue(TEXT("Color"), FLinearColor::Red);
	}
}

void ABomb::OnProjectileBounce(const FHitResult& ImpactResult, const FVector& ImpactVelocity)
{
	// If the bomb is not armed and we have authority
	// arm it and perform a delayed explosion
	if (!bIsArmed && Role == ROLE_Authority)
	{
		bIsArmed = true;
		ArmBomb();

		PerformDelayedExplosion(FuseTime);
	}
}

void ABomb::OnRep_IsArmed()
{
	// Will get called when the bomb is armed
	// from the authority client
	if (bIsArmed)
	{
		ArmBomb();
	}
}

void ABomb::PerformDelayedExplosion(float ExplosionDelay)
{
	FTimerHandle TimerHandle;
	FTimerDelegate TimerDel;
	TimerDel.BindUFunction(this, TEXT("Explode"));

	GetWorld()->GetTimerManager().SetTimer(TimerHandle, TimerDel, ExplosionDelay, false);
}

void ABomb::Explode()
{
	SimulateExplosionFX();

	// We won't use any specific damage in our case
	TSubclassOf<UDamageType> DmgType;
	// Do not ignore any actors
	TArray<AActor*> IgnoreActors;

	// This will eventually call the TakeDamage funciton that we have overriden in the Character class
	UGameplayStatics::ApplyRadialDamage(GetWorld(), ExplosionDamage, GetActorLocation(), ExplosionRadius, DmgType, IgnoreActors, this, GetInstigatorController());

	FTimerHandle TimerHandle;
	FTimerDelegate TimerDel;

	TimerDel.BindLambda([&]()
	{
		Destroy();
	});

	// Destroy actor after 0.3 seconds
	GetWorld()->GetTimerManager().SetTimer(TimerHandle, TimerDel, 0.3f, false);
}

void ABomb::SimulateExplosionFX_Implementation()
{
	if (ExplosionFX)
	{
		UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), ExplosionFX, GetTransform(), true);
	}
}