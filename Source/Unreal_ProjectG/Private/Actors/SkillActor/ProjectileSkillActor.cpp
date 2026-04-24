// Fill out your copyright notice in the Description page of Project Settings.


#include "Actors/SkillActor/ProjectileSkillActor.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "TimerManager.h"
#include "Types/PGDataTableStruct.h"

AProjectileSkillActor::AProjectileSkillActor()
{
    ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
    PrimaryActorTick.bCanEverTick = false;
}

void AProjectileSkillActor::InitFromConfig(const FHeroSpawnableConfig& InConfig, int32 InAbilityLevel)
{
    Super::InitFromConfig(InConfig, InAbilityLevel);

    if (Config.Speed > 0.f)
    {
        ProjectileMovement->InitialSpeed = Config.Speed;
        ProjectileMovement->MaxSpeed = Config.Speed;
        ProjectileMovement->bRotationFollowsVelocity = true;
        ProjectileMovement->ProjectileGravityScale = 0.f;
        ProjectileMovement->bShouldBounce = false;
    }

    if (Config.Speed > KINDA_SMALL_NUMBER && Config.MaxRange > 0.f)
    {
        SetLifeSpan(Config.MaxRange / Config.Speed);
    }

    BaseActorScale = GetActorScale3D();
    ElapsedGrowthTime = 0.f;

    SetRuntimeMultipliers(1.f, 1.f);
}

void AProjectileSkillActor::BeginPlay()
{
    Super::BeginPlay();

    if (bEnableTimeGrowth)
    {
        CurrentGrowthMultiplier = 1.f;
        StartGrowthUpdateTimerIfNeeded();
    }
}

void AProjectileSkillActor::StartGrowthUpdateTimerIfNeeded()
{
    if (!GetWorld() || !bEnableTimeGrowth) return;

    UpdateGrowthByTime();

    GetWorldTimerManager().SetTimer(
        GrowthUpdateTimerHandle,
        this,
        &AProjectileSkillActor::UpdateGrowthByTime,
        GrowthUpdateInterval,
        true);
}

void AProjectileSkillActor::UpdateGrowthByTime()
{
    if (!bEnableTimeGrowth)
    {
        StopGrowthUpdateTimer();
        return;
    }

    // Owner/Instigator가 없으면 더 이상 spec rebuild 시도하지 않음
    if (!IsValid(GetOwner()) && !IsValid(GetInstigator()))
    {
        StopGrowthUpdateTimer();
        return;
    }

    ElapsedGrowthTime += GrowthUpdateInterval;

    const float Alpha = FMath::Clamp(ElapsedGrowthTime / GrowthDuration, 0.f, 1.f);
    const float NewMultiplier = FMath::Lerp(1.f, MaxGrowthMultiplier, Alpha);

    if (FMath::IsNearlyEqual(NewMultiplier, GetRuntimeScaleMultiplier()))
    {
        return;
    }

    // 단일 소스: SkillActor 런타임 배율 사용
    SetRuntimeMultipliers(NewMultiplier, NewMultiplier);
    RebuildEffectSpecsFromConfig();
    CurrentGrowthMultiplier = NewMultiplier;

    if(Alpha >= 1.f)
    {
        StopGrowthUpdateTimer();
    }
}

void AProjectileSkillActor::StopGrowthUpdateTimer()
{
    if (GetWorld())
    {
        GetWorldTimerManager().ClearTimer(GrowthUpdateTimerHandle);
    }
}

void AProjectileSkillActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    StopGrowthUpdateTimer();
    Super::EndPlay(EndPlayReason);
}

void AProjectileSkillActor::SpawnFollowUpActor()
{
    // 투사체는 후속 액털 스폰을 맞은 대상 위치에 스폰
    if (Config.NextSpawn.ActorClass)
    {
        if (!Config.NextSpawn.ActorClass) return;

        AActor* OwnerActor = GetOwner();
        if (!OwnerActor || !GetWorld()) return;

        FVector SpawnOffset = FVector::ZeroVector;
        if (const FSpawnOffsetRow* Row = Config.NextSpawn.SpawnOffsetRow.GetRow<FSpawnOffsetRow>(TEXT("SpawnOffset")))
        {
            SpawnOffset = Row->SpawnOffset;
        }

        FVector BaseSpawnLocation = GetActorLocation();

        // 만약 OverlappingTargets에 유효한 타겟이 있다면, [0]번 타겟 위치로 스폰 오프셋을 계산. 그렇지 않으면 현재 액터 위치 기준으로 계산
        if (!OverlappingTargets.IsEmpty())
        {
            BaseSpawnLocation = OverlappingTargets[0].IsNull() ? BaseSpawnLocation : OverlappingTargets[0]->GetActorLocation();
        }

        const FTransform SpawnTransform(GetActorRotation(), BaseSpawnLocation + SpawnOffset);

        ASkillActor* Spawned = GetWorld()->SpawnActorDeferred<ASkillActor>(
            Config.NextSpawn.ActorClass,
            SpawnTransform,
            OwnerActor,
            Cast<APawn>(OwnerActor),
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

        if (!Spawned) return;

        FHeroSpawnableConfig FollowUpConfig = MakeSpawnableConfigFromFollowUp(Config.NextSpawn);


        // 현재 배율을 후속 액터로 전달(후속이 Init에서 Spec 생성할 때 반영됨)
        Spawned->SetOwner(OwnerActor);
        Spawned->SetInstigator(Cast<APawn>(OwnerActor));
        PropagateRuntimeMultipliersTo(Spawned);
        Spawned->InitFromConfig(FollowUpConfig, CachedAbilityLevel);
        Spawned->FinishSpawning(SpawnTransform);
    }
}
