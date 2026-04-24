#include "AbilitySystem/AbilityTasks/SkillAbilityTask_SpawnActor.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "Abilities/Tasks/AbilityTask_WaitTargetData.h"
#include "AbilitySystem/Abilities/PGGameplayAbility.h"
#include "AbilitySystem/PGCharacterAttributeSet.h"
#include "AbilitySystem/TargetActor/GATargetActor_AOEGroundTrace.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Actors/SkillActor/SkillActor.h"
#include "Components/Combat/HeroCombatComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "PGFunctionLibrary.h"
#include "PGGameplayTags.h"
#include "Types/PGDataTableStruct.h"
#include "Character/Unit/SubSystem/PGObjectPoolSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogSkillSpawnTask, Log, All);

namespace SkillTaskConstants
{
    constexpr float SearchRadius = 1000.f;
}

USkillAbilityTask_SpawnActor* USkillAbilityTask_SpawnActor::Create(
    UGameplayAbility* OwningAbility,
    const FSkillActionRow& ActionRow,
    bool bIsAutoMode,
    const FGameplayAbilityTargetDataHandle& InTriggerTargetData,
    bool bInEmitRuntimeEvent)
{
    USkillAbilityTask_SpawnActor* Task = NewAbilityTask<USkillAbilityTask_SpawnActor>(OwningAbility);
    Task->CachedActionRow = ActionRow;
    Task->bAutoMode = bIsAutoMode;
    Task->TriggerTargetData = InTriggerTargetData;
    Task->bEmitRuntimeEvent = bInEmitRuntimeEvent;
    return Task;
}

void USkillAbilityTask_SpawnActor::Activate()
{
    const FHeroSpawnableConfig& Config = CachedActionRow.SpawnableConfig;
    AActor* AvatarActor = Ability->GetAvatarActorFromActorInfo();
    bWaitMontageFinish = (Config.Montage != nullptr);
    bActorSpawned = false;
    bTaskResolved = false;

    if (!AvatarActor || !Config.ActorClass)
    {
        OnCancelled.Broadcast({});
        EndTask();
        return;
    }

    switch (Config.SpawnLocationPolicy)
    {
    case ESpawnLocation::AtCaster:
        CachedSpawnLocation = AvatarActor->GetActorLocation();
        CachedSpawnRotation = AvatarActor->GetActorRotation();
        PlayMontageOrSpawn();
        break;

    case ESpawnLocation::AtTargetPoint:
        if (TryResolveSpawnFromTriggerData(CachedSpawnLocation, CachedSpawnRotation))
        {
            PlayMontageOrSpawn();
            return;
        }

        if (bAutoMode)
        {
            CachedSpawnLocation = ResolveAutoTargetLocation();
            CachedSpawnRotation = AvatarActor->GetActorRotation();
            PlayMontageOrSpawn();
        }
        else
        {
            StartWaitTargetData();
        }
        break;

    default:
        OnCancelled.Broadcast({});
        EndTask();
        break;
    }
}

void USkillAbilityTask_SpawnActor::StartWaitTargetData()
{
    const FHeroSpawnableConfig& Config = CachedActionRow.SpawnableConfig;
    AActor* AvatarActor = Ability->GetAvatarActorFromActorInfo();

    UAbilityTask_WaitTargetData* TargetTask = UAbilityTask_WaitTargetData::WaitTargetData(
        Ability,
        NAME_None,
        EGameplayTargetingConfirmation::UserConfirmed,
        AGATargetActor_AOEGroundTrace::StaticClass());

    TargetTask->ValidData.AddDynamic(this, &USkillAbilityTask_SpawnActor::OnTargetDataReady);
    TargetTask->Cancelled.AddDynamic(this, &USkillAbilityTask_SpawnActor::OnTargetDataCancelled);

    // GroundTrace 타겟 액터를 미리 스폰하여 위치 피드백 제공
    AGameplayAbilityTargetActor* SpawnedTargetActor = nullptr;
    if (TargetTask->BeginSpawningActor(Ability, AGATargetActor_AOEGroundTrace::StaticClass(), SpawnedTargetActor))
    {
        if (AGATargetActor_AOEGroundTrace* GroundTrace = Cast<AGATargetActor_AOEGroundTrace>(SpawnedTargetActor))
        {
            GroundTrace->SetOwner(AvatarActor);
            GroundTrace->SetInstigator(Cast<APawn>(AvatarActor));
            GroundTrace->PreviewRadius = Config.Radius;
            GroundTrace->AOETraceDecalMaterial = Config.IndicatorDecalMaterial;
            GroundTrace->OwnerActor = AvatarActor;
            GroundTrace->TargetPolicy = Config.TargetPolicy;
        }

        TargetTask->FinishSpawningActor(Ability, SpawnedTargetActor);
    }

    TargetTask->ReadyForActivation();
}

void USkillAbilityTask_SpawnActor::OnTargetDataReady(const FGameplayAbilityTargetDataHandle& TargetDataHandle)
{
    if (!TargetDataHandle.IsValid(0))
    {
        OnCancelled.Broadcast({});
        EndTask();
        return;
    }

    //==========================================================================================================
    // 스폰 프로젝타일의 타깃 액터를 위한 연출 타이밍 함수와 변수들. 현재는 이 경우의 수 하나 뿐이라 이렇게 구현했지만, 추후에 다른 연출 타이밍이 추가된다면 더 범용적으로 바꿀 필요가 있을듯
    // 타깃 데이터가 준비된 시점에 연출을 위한 이벤트 발송
    const FHeroSpawnableConfig& Config = CachedActionRow.SpawnableConfig;
    if (!bAutoMode && Config.SpawnLocationPolicy == ESpawnLocation::AtTargetPoint)
    {
        EmitRuntimeEvent(PGGameplayTags::Event_Trigger_OnTargetDataReady, TargetDataHandle);
    }
    //==========================================================================================================

    AActor* AvatarActor = Ability->GetAvatarActorFromActorInfo();
    CachedSpawnLocation = AvatarActor ? AvatarActor->GetActorLocation() : FVector::ZeroVector;
    CachedSpawnRotation = AvatarActor ? AvatarActor->GetActorRotation() : FRotator::ZeroRotator;

    if (const FGameplayAbilityTargetData* Data = TargetDataHandle.Get(0))
    {
        if (const FHitResult* Hit = Data->GetHitResult())
        {
            CachedSpawnLocation = Hit->Location;
        }
    }

    // 스폰 몽타주 재생 전에 캐릭터 방향을 타겟 방향으로 조정
    if(AvatarActor)
    {
        FRotator LookAtRotation = (CachedSpawnLocation - AvatarActor->GetActorLocation()).Rotation();
        LookAtRotation.Pitch = 0.f; // 수평 회전만 적용
        AvatarActor->SetActorRotation(LookAtRotation);
    }

    PlayMontageOrSpawn();
}

void USkillAbilityTask_SpawnActor::OnTargetDataCancelled(const FGameplayAbilityTargetDataHandle& TargetDataHandle)
{
    OnCancelled.Broadcast({});
    EndTask();
}

bool USkillAbilityTask_SpawnActor::TryResolveSpawnFromTriggerData(FVector& OutLocation, FRotator& OutRotation) const
{
    AActor* AvatarActor = Ability ? Ability->GetAvatarActorFromActorInfo() : nullptr;
    if (!AvatarActor || TriggerTargetData.Num() <= 0)
    {
        return false;
    }

    OutRotation = AvatarActor->GetActorRotation();

    for (int32 Index = 0; Index < TriggerTargetData.Num(); ++Index)
    {
        const FGameplayAbilityTargetData* Data = TriggerTargetData.Get(Index);
        if (!Data)
        {
            continue;
        }

        if (const FHitResult* Hit = Data->GetHitResult())
        {
            OutLocation = Hit->Location;
            return true;
        }

        const TArray<TWeakObjectPtr<AActor>> Actors = Data->GetActors();
        for (const TWeakObjectPtr<AActor>& WeakActor : Actors)
        {
            if (AActor* Actor = WeakActor.Get())
            {
                OutLocation = Actor->GetActorLocation();
                return true;
            }
        }
    }

    return false;
}

FVector USkillAbilityTask_SpawnActor::ResolveAutoTargetLocation() const
{
    const FHeroSpawnableConfig& Config = CachedActionRow.SpawnableConfig;
    AActor* AvatarActor = Ability->GetAvatarActorFromActorInfo();
    UHeroCombatComponent* CombatComp = UPGFunctionLibrary::NativeGetCombatComponentFromActor(AvatarActor);

    switch (Config.TargetPolicy)
    {
    case ESkillTargetPolicy::Enemy:
        if (CombatComp && CombatComp->CurrentTarget.IsValid())
        {
            return CombatComp->CurrentTarget->GetActorLocation();
        }
        break;

    case ESkillTargetPolicy::Ally:
    {
        TArray<FHitResult> HitResults;
        UKismetSystemLibrary::SphereTraceMultiForObjects(
            AvatarActor->GetWorld(),
            AvatarActor->GetActorLocation(),
            AvatarActor->GetActorLocation(),
            SkillTaskConstants::SearchRadius,
            { UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_Pawn) },
            false,
            { AvatarActor },
            EDrawDebugTrace::None,
            HitResults,
            true);

        AActor* LowestAlly = nullptr;
        float LowestHealth = FLT_MAX;

        for (const FHitResult& Hit : HitResults)
        {
            AActor* HitActor = Hit.GetActor();
            if (!HitActor)
            {
                continue;
            }

            UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitActor);
            if (!ASC)
            {
                continue;
            }

            const float Health = ASC->GetNumericAttribute(UPGCharacterAttributeSet::GetHealthAttribute());
            if (Health < LowestHealth)
            {
                LowestHealth = Health;
                LowestAlly = HitActor;
            }
        }

        if (LowestAlly)
        {
            return LowestAlly->GetActorLocation();
        }

        break;
    }

    case ESkillTargetPolicy::Self:
        return AvatarActor->GetActorLocation();

    default:
        break;
    }

    return AvatarActor->GetActorLocation();
}

void USkillAbilityTask_SpawnActor::PlayMontageOrSpawn()
{
    const FHeroSpawnableConfig& Config = CachedActionRow.SpawnableConfig;

    if (bWaitMontageFinish)
    {
        UAbilityTask_WaitGameplayEvent* SpawnEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
            Ability,
            PGGameplayTags::Shared_Event_ActorSpawn);
        SpawnEventTask->EventReceived.AddDynamic(this, &USkillAbilityTask_SpawnActor::OnSpawnEventReceived);
        SpawnEventTask->ReadyForActivation();

        UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(Ability, NAME_None, Config.Montage);
        MontageTask->OnCompleted.AddDynamic(this, &USkillAbilityTask_SpawnActor::OnMontageFinished);
        MontageTask->OnBlendOut.AddDynamic(this, &USkillAbilityTask_SpawnActor::OnMontageFinished);
        MontageTask->OnCancelled.AddDynamic(this, &USkillAbilityTask_SpawnActor::OnMontageCancelled);
        MontageTask->OnInterrupted.AddDynamic(this, &USkillAbilityTask_SpawnActor::OnMontageCancelled);
        MontageTask->ReadyForActivation();
    }
    else
    {
        SpawnActorAtLocation(CachedSpawnLocation, CachedSpawnRotation);
        bTaskResolved = true;
        OnCompleted.Broadcast({});
        EndTask();
    }
}

void USkillAbilityTask_SpawnActor::OnMontageFinished()
{
    if (bTaskResolved)
    {
        return;
    }

    if (!bActorSpawned)
    {
        SpawnActorAtLocation(CachedSpawnLocation, CachedSpawnRotation);
    }

    bTaskResolved = true;
    OnCompleted.Broadcast({});
    EndTask();
}

void USkillAbilityTask_SpawnActor::OnMontageCancelled()
{
    if (bTaskResolved)
    {
        return;
    }

    bTaskResolved = true;
    OnCancelled.Broadcast({});
    EndTask();
}

void USkillAbilityTask_SpawnActor::OnSpawnEventReceived(FGameplayEventData Payload)
{
    if (bActorSpawned)
    {
        return;
    }

    SpawnActorAtLocation(CachedSpawnLocation, CachedSpawnRotation);

    if (!bWaitMontageFinish)
    {
        if (bTaskResolved)
        {
            return;
        }
        bTaskResolved = true;
        OnCompleted.Broadcast({});
        EndTask();
    }
}

void USkillAbilityTask_SpawnActor::SpawnActorAtLocation(const FVector& Location, const FRotator& Rotation)
{
    const FHeroSpawnableConfig& Config = CachedActionRow.SpawnableConfig;
    AActor* AvatarActor = Ability->GetAvatarActorFromActorInfo();

    FVector FinalLocation = Location;
    FRotator FinalRotation = Rotation;

    // At Caster 정책이라면 이 시점에서 스폰될 위치를 결정
    if (Config.SpawnLocationPolicy == ESpawnLocation::AtCaster && AvatarActor)
    {
        FinalLocation = AvatarActor->GetActorLocation();
        FinalRotation = AvatarActor->GetActorRotation();
    }

    FVector SpawnOffset = FVector::ZeroVector;
    if (const FSpawnOffsetRow* Row = Config.SpawnOffsetRow.GetRow<FSpawnOffsetRow>(TEXT("SpawnOffset")))
    {
        SpawnOffset = Row->SpawnOffset;
    }

    // X/Y 오프셋은 캐릭터의 로컬 좌표계를 따름, Z 오프셋은 항상 월드 좌표계를 따름
    FVector RotatedOffset = SpawnOffset;
    if (AvatarActor)
    {
        const FRotator YawOnlyRotation(0.0f, AvatarActor->GetActorRotation().Yaw, 0.0f);
        const FVector XYOffset = FVector(SpawnOffset.X, SpawnOffset.Y, 0.0f);
        const FVector RotatedXYOffset = YawOnlyRotation.RotateVector(XYOffset);
        RotatedOffset = FVector(RotatedXYOffset.X, RotatedXYOffset.Y, SpawnOffset.Z);
    }

    const FTransform SpawnTransform(FinalRotation, FinalLocation + RotatedOffset);

    UPGObjectPoolSubsystem* PoolSubsystem = GetWorld()->GetSubsystem<UPGObjectPoolSubsystem>();
    ASkillActor* Spawned = nullptr;

    if (PoolSubsystem)
    {
        Spawned = Cast<ASkillActor>(PoolSubsystem->TryGetPooledActor(Config.ActorClass, SpawnTransform));
    }

    if (Spawned)
    {
        UE_LOG(
            LogSkillSpawnTask,
            Log,
            TEXT("[SpawnActorAtLocation] POOLED Class=%s Actor=%s Owner=%s SpawnLocation=%s"),
            *GetNameSafe(Config.ActorClass.Get()),
            *GetNameSafe(Spawned),
            *GetNameSafe(AvatarActor),
            *SpawnTransform.GetLocation().ToCompactString());

        Spawned->SetOwner(AvatarActor);
        Spawned->SetInstigator(Cast<APawn>(AvatarActor));
        Spawned->InitFromConfig(Config, Ability->GetAbilityLevel());
        Spawned->OnActivatedFromPool();
        bActorSpawned = true;

        EmitRuntimeEvent(PGGameplayTags::Event_Trigger_OnCommit, FGameplayAbilityTargetDataHandle());
        return;
    }

    UE_LOG(
        LogSkillSpawnTask,
        Log,
        TEXT("[SpawnActorAtLocation] FALLBACK_DEFERRED_SPAWN Class=%s Owner=%s SpawnLocation=%s"),
        *GetNameSafe(Config.ActorClass.Get()),
        *GetNameSafe(AvatarActor),
        *SpawnTransform.GetLocation().ToCompactString());

    Spawned = GetWorld()->SpawnActorDeferred<ASkillActor>(
        Config.ActorClass,
        SpawnTransform,
        AvatarActor,
        Cast<APawn>(AvatarActor),
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

    if (!Spawned)
    {
        UE_LOG(
            LogSkillSpawnTask,
            Warning,
            TEXT("[SpawnActorAtLocation] SPAWN_FAILED Class=%s Owner=%s SpawnLocation=%s"),
            *GetNameSafe(Config.ActorClass.Get()),
            *GetNameSafe(AvatarActor),
            *SpawnTransform.GetLocation().ToCompactString());
        OnCancelled.Broadcast({});
        EndTask();
        return;
    }

    UE_LOG(
        LogSkillSpawnTask,
        Log,
        TEXT("[SpawnActorAtLocation] DEFERRED_SPAWNED Class=%s Actor=%s Owner=%s SpawnLocation=%s"),
        *GetNameSafe(Config.ActorClass.Get()),
        *GetNameSafe(Spawned),
        *GetNameSafe(AvatarActor),
        *SpawnTransform.GetLocation().ToCompactString());

    Spawned->SetOwner(AvatarActor);
    Spawned->SetInstigator(Cast<APawn>(AvatarActor));

    Spawned->InitFromConfig(Config, Ability->GetAbilityLevel());
    Spawned->FinishSpawning(SpawnTransform);
    Spawned->OnActivatedFromPool();
    bActorSpawned = true;

    EmitRuntimeEvent(PGGameplayTags::Event_Trigger_OnCommit, FGameplayAbilityTargetDataHandle());
}

void USkillAbilityTask_SpawnActor::OnDestroy(bool bInOwnerFinished)
{
    Super::OnDestroy(bInOwnerFinished);
}
