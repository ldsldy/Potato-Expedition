// Fill out your copyright notice in the Description page of Project Settings.


#include "AbilitySystem/Abilities/Unit/UnitAbility_SpawnProjectile.h"
#include "Actors/PGProjectileBase.h"
#include "PGGameplayTags.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "DataAssets/Ability/DataAsset_SkillData.h"
#include "Character/Unit/SubSystem/PGObjectPoolSubsystem.h"


void UUnitAbility_SpawnProjectile::OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
    Super::OnGiveAbility(ActorInfo, Spec);

    UDataAsset_SkillData* DataAsset = Cast<UDataAsset_SkillData>(Spec.SourceObject.Get());
    if (DataAsset)
    {
        const FUnitSpawnProjectileAbilityConfig* Config = DataAsset->AbilityEntry.AbilityConfig.GetPtr<FUnitSpawnProjectileAbilityConfig>();
        if (Config)
        {
            UnitSpawnProjectileConfig = *Config;
        }
    }

    UnitSpawnProjectileConfig.SpawnedProjectileClass.LoadSynchronous();
    for (TSoftObjectPtr<UAnimMontage>& Montage : UnitSpawnProjectileConfig.SpawnProjectileMontages)
    {
        Montage.LoadSynchronous();
    }

    if (PoolPrewarmCount > 0)
    {
        UWorld* World = ActorInfo && ActorInfo->AvatarActor.IsValid() ? ActorInfo->AvatarActor->GetWorld() : GetWorld();
        if (World)
        {
            if (UPGObjectPoolSubsystem* PoolSubsystem = World->GetSubsystem<UPGObjectPoolSubsystem>())
            {
                if (UClass* ProjectileClass = UnitSpawnProjectileConfig.SpawnedProjectileClass.Get())
                {
                    PoolSubsystem->PrewarmPool(ProjectileClass, PoolPrewarmCount);
                }
            }
        }
    }
}

void UUnitAbility_SpawnProjectile::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    checkf(UnitSpawnProjectileConfig.SpawnProjectileMontages.Num() > 0, TEXT("ProjectileAttackMontage가 비어있습니다!"));

    // 램덤하게 하나의 몽타주 선택
    UAnimMontage* ProjectileAttackMontage = UnitSpawnProjectileConfig.SpawnProjectileMontages[FMath::RandRange(0, UnitSpawnProjectileConfig.SpawnProjectileMontages.Num() - 1)].Get();

    // 애니메이션 몽타주 재생
    UAbilityTask_PlayMontageAndWait* ProjectileMontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, ProjectileAttackMontage);

    // 몽타주 완료 이벤트 바인딩
    if (ProjectileMontageTask)
    {
        ProjectileMontageTask->OnCompleted.AddUniqueDynamic(this, &UUnitAbility_SpawnProjectile::OnMontageFinished);
        ProjectileMontageTask->OnInterrupted.AddUniqueDynamic(this, &UUnitAbility_SpawnProjectile::OnMontageFinished);
        ProjectileMontageTask->OnBlendOut.AddUniqueDynamic(this, &UUnitAbility_SpawnProjectile::OnMontageFinished);
        ProjectileMontageTask->OnCancelled.AddUniqueDynamic(this, &UUnitAbility_SpawnProjectile::OnMontageFinished);

        ProjectileMontageTask->ReadyForActivation();
    }

    // 게임플레이 이벤트 대기 태스크 생성
    UAbilityTask_WaitGameplayEvent* ProjectileSpawnEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, PGGameplayTags::Shared_Event_ProjectileSpawn);

    // 이벤트 수신 핸들러 바인딩
    ProjectileSpawnEventTask->EventReceived.AddUniqueDynamic(this, &UUnitAbility_SpawnProjectile::SpawnProjectile);
    ProjectileSpawnEventTask->ReadyForActivation();
}

void UUnitAbility_SpawnProjectile::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UUnitAbility_SpawnProjectile::SpawnProjectile(FGameplayEventData InEventData)
{
    FVector SpawnLocation = GetAvatarActorFromActorInfo()->GetActorLocation();
    FRotator SpawnRotation = GetAvatarActorFromActorInfo()->GetActorForwardVector().Rotation();
    FTransform SpawnTransform(SpawnRotation, SpawnLocation);

    // 1. 풀링 서브시스템 가져오기
    UPGObjectPoolSubsystem* PoolSubsystem = GetWorld()->GetSubsystem<UPGObjectPoolSubsystem>();
    if (!PoolSubsystem) return;

    // 2.  투사체 꺼내기 (없으면 새로 스폰됨)
    APGProjectileBase* SpawnedProjectile = Cast<APGProjectileBase>(
        PoolSubsystem->GetActorFromPool(UnitSpawnProjectileConfig.SpawnedProjectileClass.Get(), SpawnTransform)
    );

    if (SpawnedProjectile)
    {
        // 3. 소유자 세팅 (데미지 판정을 위해 필수)
        SpawnedProjectile->SetOwner(GetAvatarActorFromActorInfo());
        SpawnedProjectile->SetInstigator(Cast<APawn>(GetAvatarActorFromActorInfo()));

        // 4. 데미지 이펙트 스펙 전달
        float ProjectileMultiplierValue = UnitSpawnProjectileConfig.SkillMultiplier.GetValueAtLevel(GetAbilityLevel());
        FGameplayEffectSpecHandle ProjectileDamageEffectSpecHandle = MakeOutgoingEffectSpecWithMultiplier(UnitSpawnProjectileConfig.DamageEffectClass.Get(), ProjectileMultiplierValue);
        SpawnedProjectile->SetProjectileDamageEffectSpecHandle(ProjectileDamageEffectSpecHandle);

        // 5. 보관된 곳에 자원 꺼냈음을 알리고 활성화 (BeginPlay 대체)
        SpawnedProjectile->OnActivatedFromPool();
    }
}

void UUnitAbility_SpawnProjectile::OnMontageFinished()
{
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, false);
}
