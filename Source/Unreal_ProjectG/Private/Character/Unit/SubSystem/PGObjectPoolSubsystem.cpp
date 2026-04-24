// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/Unit/SubSystem/PGObjectPoolSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogPGPooling, Log, All);

namespace PGObjectPoolSubsystemInternal
{
    static const FName ReturnedToPoolFuncName(TEXT("OnReturnedToPool"));

    static void SetActorToPooledState(AActor* Actor)
    {
        if (!IsValid(Actor))
        {
            return;
        }

        if (UFunction* ReturnedFunc = Actor->FindFunction(ReturnedToPoolFuncName))
        {
            Actor->ProcessEvent(ReturnedFunc, nullptr);
        }

        // 함수 구현 유무와 관계없이, 풀 상태는 최종적으로 비활성화를 강제한다.
        Actor->SetActorHiddenInGame(true);
        Actor->SetActorEnableCollision(false);
        Actor->SetActorTickEnabled(false);
    }
}

AActor* UPGObjectPoolSubsystem::TryGetPooledActor(TSubclassOf<AActor> ActorClass, const FTransform& SpawnTransform)
{
    if (!ActorClass)
    {
        UE_LOG(LogPGPooling, Warning, TEXT("[TryGetPooledActor] Invalid ActorClass."));
        return nullptr;
    }

    TArray<AActor*>& Pool = ActorPool.FindOrAdd(ActorClass).Actors;
    const int32 InitialPoolSize = Pool.Num();
    int32 InvalidDiscardedCount = 0;

    while (Pool.Num() > 0)
    {
        AActor* PooledActor = Pool.Pop();
        if (!IsValid(PooledActor))
        {
            ++InvalidDiscardedCount;
            continue;
        }

        PooledActor->SetActorTransform(SpawnTransform);
        UE_LOG(
            LogPGPooling,
            Log,
            TEXT("[TryGetPooledActor] HIT Class=%s Actor=%s Initial=%d Remaining=%d InvalidDiscarded=%d SpawnLocation=%s"),
            *GetNameSafe(ActorClass.Get()),
            *GetNameSafe(PooledActor),
            InitialPoolSize,
            Pool.Num(),
            InvalidDiscardedCount,
            *SpawnTransform.GetLocation().ToCompactString());
        return PooledActor;
    }

    UE_LOG(
        LogPGPooling,
        Log,
        TEXT("[TryGetPooledActor] MISS Class=%s Initial=%d InvalidDiscarded=%d"),
        *GetNameSafe(ActorClass.Get()),
        InitialPoolSize,
        InvalidDiscardedCount);

    return nullptr;
}

AActor* UPGObjectPoolSubsystem::GetActorFromPool(TSubclassOf<AActor> ActorClass, const FTransform& SpawnTransform)
{
    if (!ActorClass)
    {
        UE_LOG(LogPGPooling, Warning, TEXT("[GetActorFromPool] Invalid ActorClass."));
        return nullptr;
    }

    if (AActor* PooledActor = TryGetPooledActor(ActorClass, SpawnTransform))
    {
        return PooledActor;
    }

    // 풀이 비어있다면 새로 스폰 (최초 실행 시)
    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    AActor* SpawnedActor = GetWorld()->SpawnActor<AActor>(ActorClass, SpawnTransform, SpawnParams);

    if (SpawnedActor)
    {
        UE_LOG(
            LogPGPooling,
            Log,
            TEXT("[GetActorFromPool] SPAWN_NEW Class=%s Actor=%s SpawnLocation=%s"),
            *GetNameSafe(ActorClass.Get()),
            *GetNameSafe(SpawnedActor),
            *SpawnTransform.GetLocation().ToCompactString());
    }
    else
    {
        UE_LOG(
            LogPGPooling,
            Warning,
            TEXT("[GetActorFromPool] SPAWN_FAILED Class=%s SpawnLocation=%s"),
            *GetNameSafe(ActorClass.Get()),
            *SpawnTransform.GetLocation().ToCompactString());
    }

    return SpawnedActor;
}

void UPGObjectPoolSubsystem::ReturnActorToPool(AActor* ActorToReturn)
{
    if (!IsValid(ActorToReturn))
    {
        UE_LOG(LogPGPooling, Warning, TEXT("[ReturnActorToPool] Invalid Actor."));
        return;
    }

    PGObjectPoolSubsystemInternal::SetActorToPooledState(ActorToReturn);
    TArray<AActor*>& Pool = ActorPool.FindOrAdd(ActorToReturn->GetClass()).Actors;
    const int32 BeforeNum = Pool.Num();
    Pool.AddUnique(ActorToReturn);
    const bool bDuplicate = (Pool.Num() == BeforeNum);

    UE_LOG(
        LogPGPooling,
        Log,
        TEXT("[ReturnActorToPool] %s Class=%s Actor=%s Before=%d After=%d"),
        bDuplicate ? TEXT("DUPLICATE_IGNORED") : TEXT("RETURNED"),
        *GetNameSafe(ActorToReturn->GetClass()),
        *GetNameSafe(ActorToReturn),
        BeforeNum,
        Pool.Num());
}

void UPGObjectPoolSubsystem::PrewarmPool(TSubclassOf<AActor> ActorClass, int32 PoolSize)
{
    if (!ActorClass || PoolSize <= 0)
    {
        UE_LOG(
            LogPGPooling,
            Warning,
            TEXT("[PrewarmPool] Skip. Class=%s RequestedSize=%d"),
            *GetNameSafe(ActorClass.Get()),
            PoolSize);
        return;
    }

    TArray<AActor*>& Pool = ActorPool.FindOrAdd(ActorClass).Actors;
    const int32 BeforeCleanupSize = Pool.Num();
    Pool.RemoveAll([](const AActor* Actor)
    {
        return !IsValid(Actor);
    });
    const int32 RemovedInvalidCount = BeforeCleanupSize - Pool.Num();

    const int32 AdditionalCount = FMath::Max(0, PoolSize - Pool.Num());
    UE_LOG(
        LogPGPooling,
        Log,
        TEXT("[PrewarmPool] Class=%s Requested=%d Existing=%d RemovedInvalid=%d ToSpawn=%d"),
        *GetNameSafe(ActorClass.Get()),
        PoolSize,
        Pool.Num(),
        RemovedInvalidCount,
        AdditionalCount);

    int32 SpawnedCount = 0;

    for (int32 i = 0; i < AdditionalCount; ++i)
    {
        FActorSpawnParameters SpawnParams;
        SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

        // 플레이어 눈에 안 띄는 맵 저 밑바닥에 스폰
        FTransform SpawnTransform(FRotator::ZeroRotator, FVector(0.f, 0.f, -10000.f));
        AActor* NewActor = GetWorld()->SpawnActor<AActor>(ActorClass, SpawnTransform, SpawnParams);

        if (NewActor)
        {
            PGObjectPoolSubsystemInternal::SetActorToPooledState(NewActor);
            Pool.AddUnique(NewActor);
            ++SpawnedCount;
        }
        else
        {
            UE_LOG(
                LogPGPooling,
                Warning,
                TEXT("[PrewarmPool] Spawn failed for Class=%s at index=%d"),
                *GetNameSafe(ActorClass.Get()),
                i);
        }
    }

    UE_LOG(
        LogPGPooling,
        Log,
        TEXT("[PrewarmPool] Done Class=%s Spawned=%d FinalSize=%d"),
        *GetNameSafe(ActorClass.Get()),
        SpawnedCount,
        Pool.Num());
}
