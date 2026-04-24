// Fill out your copyright notice in the Description page of Project Settings.


#include "Actors/SkillActor/BuffAuraActor.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Components/DecalComponent.h"
#include "Components/SphereComponent.h"
#include "PGFunctionLibrary.h"

ABuffAuraActor::ABuffAuraActor()
{
    AuraDecalComponent = CreateDefaultSubobject<UDecalComponent>(TEXT("AuraDecalComponent"));
    AuraDecalComponent->SetupAttachment(SceneRoot);
    AuraDecalComponent->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));
    AuraDecalComponent->DecalSize = FVector(200.f, 100.f, 100.f);
}

void ABuffAuraActor::InitFromConfig(const FHeroSpawnableConfig& InConfig, int32 InAbilityLevel)
{
    Super::InitFromConfig(InConfig, InAbilityLevel);

    Config.HitsPerLifeSpan = 0.f;

    if (AuraDecalComponent)
    {
        const float Radius = Config.Radius > 0.f ? Config.Radius : CollisionComponent->GetScaledSphereRadius();
        AuraDecalComponent->DecalSize = FVector(200.f, Radius, Radius);
    }
}

void ABuffAuraActor::BeginPlay()
{
    Super::BeginPlay();

    CollisionComponent->OnComponentBeginOverlap.RemoveAll(this);
    CollisionComponent->OnComponentEndOverlap.RemoveAll(this);
    CollisionComponent->OnComponentBeginOverlap.AddDynamic(this, &ABuffAuraActor::OnAuraBeginOverlap);
    CollisionComponent->OnComponentEndOverlap.AddDynamic(this, &ABuffAuraActor::OnAuraEndOverlap);
}

void ABuffAuraActor::OnActivatedFromPool_Implementation()
{
    Super::OnActivatedFromPool_Implementation();

    GetWorldTimerManager().ClearTimer(TickEffectTimerHandle);
    ActiveAuraEffectsByTarget.Empty();

    CollisionComponent->OnComponentBeginOverlap.RemoveAll(this);
    CollisionComponent->OnComponentEndOverlap.RemoveAll(this);
    CollisionComponent->OnComponentBeginOverlap.AddDynamic(this, &ABuffAuraActor::OnAuraBeginOverlap);
    CollisionComponent->OnComponentEndOverlap.AddDynamic(this, &ABuffAuraActor::OnAuraEndOverlap);

    if (AActor* OwnerActor = GetOwner())
    {
        if (USceneComponent* OwnerRoot = OwnerActor->GetRootComponent())
        {
            AttachToComponent(OwnerRoot, FAttachmentTransformRules::KeepWorldTransform);
        }
    }

    if (AuraDecalComponent)
    {
        AuraDecalComponent->SetVisibility(AuraDecalComponent->GetDecalMaterial() != nullptr, true);
    }

    TArray<AActor*> InitialOverlaps;
    CollisionComponent->GetOverlappingActors(InitialOverlaps);
    for (AActor* OverlappedActor : InitialOverlaps)
    {
        if (IsValidAuraTarget(OverlappedActor))
        {
            ApplyAuraToTarget(OverlappedActor);
        }
    }
}

void ABuffAuraActor::OnReturnedToPool_Implementation()
{
    RemoveAllAuraEffects();
    DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

    Super::OnReturnedToPool_Implementation();
}

void ABuffAuraActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    RemoveAllAuraEffects();
    Super::EndPlay(EndPlayReason);
}

void ABuffAuraActor::OnAuraBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    if (!IsValidAuraTarget(OtherActor))
    {
        return;
    }

    ApplyAuraToTarget(OtherActor);
}

void ABuffAuraActor::OnAuraEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
    if (!OtherActor)
    {
        return;
    }

    RemoveAuraFromTarget(OtherActor);
}

bool ABuffAuraActor::IsValidAuraTarget(AActor* TargetActor) const
{
    if (!TargetActor)
    {
        return false;
    }

    if (ActiveAuraEffectsByTarget.Contains(TargetActor))
    {
        return false;
    }

    AActor* OwnerActor = GetOwner();
    if (!OwnerActor)
    {
        return false;
    }

    switch (Config.TargetPolicy)
    {
    case ESkillTargetPolicy::Enemy:
        return UPGFunctionLibrary::IsTargetCharacterHostile(OwnerActor, TargetActor);

    case ESkillTargetPolicy::Ally:
        return !UPGFunctionLibrary::IsTargetCharacterHostile(OwnerActor, TargetActor);

    case ESkillTargetPolicy::AllyWithOutSelf:
        return !UPGFunctionLibrary::IsTargetCharacterHostile(OwnerActor, TargetActor) && TargetActor != OwnerActor;

    case ESkillTargetPolicy::Self:
        return TargetActor == OwnerActor;

    default:
        return false;
    }
}

void ABuffAuraActor::ApplyAuraToTarget(AActor* TargetActor)
{
    if (!TargetActor || EffectSpecHandles.IsEmpty())
    {
        return;
    }

    UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(TargetActor);
    UAbilitySystemComponent* SourceASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
    if (!TargetASC || !SourceASC)
    {
        return;
    }

    TArray<FActiveGameplayEffectHandle> AppliedHandles;
    for (const FGameplayEffectSpecHandle& SpecHandle : EffectSpecHandles)
    {
        if (!SpecHandle.IsValid())
        {
            continue;
        }

        const FActiveGameplayEffectHandle AppliedHandle = SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
        if (AppliedHandle.IsValid())
        {
            AppliedHandles.Add(AppliedHandle);
        }
    }

    if (!AppliedHandles.IsEmpty())
    {
        ActiveAuraEffectsByTarget.Add(TargetActor, AppliedHandles);
    }
}

void ABuffAuraActor::RemoveAuraFromTarget(AActor* TargetActor)
{
    if (!TargetActor)
    {
        return;
    }

    TArray<FActiveGameplayEffectHandle>* FoundHandles = ActiveAuraEffectsByTarget.Find(TargetActor);
    if (!FoundHandles)
    {
        return;
    }

    if (UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(TargetActor))
    {
        for (const FActiveGameplayEffectHandle& Handle : *FoundHandles)
        {
            if (Handle.IsValid())
            {
                TargetASC->RemoveActiveGameplayEffect(Handle);
            }
        }
    }

    ActiveAuraEffectsByTarget.Remove(TargetActor);
}

void ABuffAuraActor::RemoveAllAuraEffects()
{
    for (auto& Pair : ActiveAuraEffectsByTarget)
    {
        if (!Pair.Key.IsValid())
        {
            continue;
        }

        if (UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Pair.Key.Get()))
        {
            for (const FActiveGameplayEffectHandle& Handle : Pair.Value)
            {
                if (Handle.IsValid())
                {
                    TargetASC->RemoveActiveGameplayEffect(Handle);
                }
            }
        }
    }

    ActiveAuraEffectsByTarget.Empty();
}

