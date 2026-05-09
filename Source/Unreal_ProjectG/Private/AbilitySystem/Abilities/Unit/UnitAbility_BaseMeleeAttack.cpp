
#include "AbilitySystem/Abilities/Unit/UnitAbility_BaseMeleeAttack.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "PGGameplayTags.h"
#include "GameplayCueFunctionLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "PGFunctionLibrary.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "DataAssets/Ability/DataAsset_SkillData.h"
#include "Character/Unit/UnitCharacter.h"
#include "Pawn/BaseStructure.h"

void UUnitAbility_BaseMeleeAttack::OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
    Super::OnGiveAbility(ActorInfo, Spec);

    UDataAsset_SkillData* DataAsset = Cast<UDataAsset_SkillData>(GetCurrentAbilitySpec()->SourceObject.Get());
    if (DataAsset)
    {
        // AbilityConfig(InsancedStruct)에서 FUnitBaseMeleeAttackAbilityConfig 타입으로 캐스트 가능한 데이터 포인터를 가져오는 코드
        const FUnitBaseMeleeAttackAbilityConfig* Config = DataAsset->AbilityEntry.AbilityConfig.GetPtr<FUnitBaseMeleeAttackAbilityConfig>();
        if (Config)
        {
            // 타입이 일치하는 경우, AbilityConfig의 데이터를 MeleeAttackConfig에 복사
            MeleeAttackConfig = *Config;
        }
    }
}

void UUnitAbility_BaseMeleeAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    //==============================================
    // FUnitBaseMeleeAttackAbilityConfig의 SoftPtr 로드
    for (TSoftObjectPtr<UAnimMontage>& AttackMontage : MeleeAttackConfig.MeleeAttackMontages)
    {
        AttackMontage.LoadSynchronous();
    }
    //==============================================
    checkf(MeleeAttackConfig.MeleeAttackMontages.Num() > 0, TEXT("MeleeAttackMontage 배열이 비어있습니다!"));
    checkf(MeleeAttackConfig.DamageEffectClass, TEXT("유닛 %s : DamageEffectClass이 유효하지 않습니다!"), *GetAvatarActorFromActorInfo()->GetName());

    // 전방에 박스 트레이스를 발사하여 가장 가까운 타겟 액터를 찾음
    FVector StartLocation = GetAvatarActorFromActorInfo()->GetActorLocation()
        + GetAvatarActorFromActorInfo()->GetActorForwardVector() * 75.f;
    FVector EndLocation = StartLocation + GetAvatarActorFromActorInfo()->GetActorForwardVector() * 250.0f;
    TArray<FHitResult> HitResults;

    // 가장 가까운 폰 액터 찾기
    UKismetSystemLibrary::SphereTraceMultiForObjects(this, StartLocation, EndLocation, MeleeAttackConfig.MeleeAttackDamageRadius, TArray<TEnumAsByte<EObjectTypeQuery>>{ EObjectTypeQuery::ObjectTypeQuery3 /* Pawn */ }, false, TArray<AActor*>(), bEnableTraceDebug ? EDrawDebugTrace::ForDuration : EDrawDebugTrace::None, HitResults, true, FLinearColor::Red, FLinearColor::Green, TraceDebugDuration);
    
    // 적대적인 타겟 필터링
    int32 CurrentHitTargets = 0;
    for (const FHitResult& HitResult : HitResults)
    {
        if (MeleeAttackConfig.MaxHitTargets <= CurrentHitTargets)
        {
            break;
        }

        AActor* HitActor = HitResult.GetActor();
        if (!IsValid(HitActor)) continue;

        bool bIsAliveTarget = false;

        if (AUnitCharacter* HitUnit = Cast<AUnitCharacter>(HitActor))
        {
            bIsAliveTarget = !HitUnit->bIsDead;
        }
        else if (ABaseStructure* HitBase = Cast<ABaseStructure>(HitActor))
        {
            bIsAliveTarget = !HitBase->IsActorBeingDestroyed();
        }
        else
        {
            bIsAliveTarget = true;
        }

        if (bIsAliveTarget && UPGFunctionLibrary::IsTargetCharacterHostile(GetAvatarActorFromActorInfo(), HitActor))
        {
            CachedTargetActors.AddUnique(HitActor);
            CurrentHitTargets++;
        }
    }

    // 타겟이 없으면 능력 종료
    if (CurrentHitTargets <= 0)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, false, true);
        return;
    }

    // 랜덤하게 하나의 몽타주 선택
    int MontageIndex = FMath::RandRange(0, MeleeAttackConfig.MeleeAttackMontages.Num() - 1);
    UAnimMontage* MeleeAttackMontage = MeleeAttackConfig.MeleeAttackMontages[MontageIndex].Get();

    // 애니메이션 몽타주 재생
    UAbilityTask_PlayMontageAndWait* MeleeMontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, MeleeAttackMontage);

    // 몽타주 완료 이벤트 바인딩
    if (MeleeMontageTask)
    {
        MeleeMontageTask->OnCompleted.AddUniqueDynamic(this, &UUnitAbility_BaseMeleeAttack::OnMontageFinished);
        MeleeMontageTask->OnInterrupted.AddUniqueDynamic(this, &UUnitAbility_BaseMeleeAttack::OnMontageFinished);
        MeleeMontageTask->OnBlendOut.AddUniqueDynamic(this, &UUnitAbility_BaseMeleeAttack::OnMontageFinished);
        MeleeMontageTask->OnCancelled.AddUniqueDynamic(this, &UUnitAbility_BaseMeleeAttack::OnMontageFinished);

        MeleeMontageTask->ReadyForActivation();
    }

    // 게임플레이 이벤트 대기 태스크 생성
    UAbilityTask_WaitGameplayEvent* MeleeHitEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(this, PGGameplayTags::Shared_Event_MeleeHit);

    // 이벤트 수신 핸들러 바인딩
    MeleeHitEventTask->EventReceived.AddUniqueDynamic(this, &UUnitAbility_BaseMeleeAttack::HandleApplyDamage);
    MeleeHitEventTask->ReadyForActivation();
}

void UUnitAbility_BaseMeleeAttack::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    CachedTargetActors.Empty();
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UUnitAbility_BaseMeleeAttack::HandleApplyDamage(FGameplayEventData InEventData)
{
    // 데미지 적용
    // TODO : 스킬의 데미지 Multiflier를 변수화
    float SkillMultiplierValue = MeleeAttackConfig.SkillMultiplier.GetValueAtLevel(GetAbilityLevel());
    FGameplayEffectSpecHandle EffectSpecHandle;
    if (MeleeAttackConfig.DamageEffectClass.Get())
    {
        EffectSpecHandle = MakeOutgoingEffectSpecWithMultiplier(MeleeAttackConfig.DamageEffectClass.Get(), SkillMultiplierValue);
    }
    for( TWeakObjectPtr<AActor> TargetActor : CachedTargetActors)
    {
        if (TargetActor.IsValid())
        {
            // TargetActor에게 게임플레이 큐 실행
            if (MeleeAttackConfig.HitImpactCueTag.IsValid())
            {
                UE_LOG(LogTemp, Log, TEXT("Executing GameplayCue %s on Actor %s"), *MeleeAttackConfig.HitImpactCueTag.ToString(), *TargetActor->GetName());
                UGameplayCueFunctionLibrary::ExecuteGameplayCueOnActor(TargetActor.Get(), MeleeAttackConfig.HitImpactCueTag, FGameplayCueParameters());
            }

            NativeApplyEffectSpecHandleToTarget(TargetActor.Get(), EffectSpecHandle);
            UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(TargetActor.Get(), PGGameplayTags::Shared_Event_HitReact, FGameplayEventData());
        }
    }
}

void UUnitAbility_BaseMeleeAttack::OnMontageFinished()
{
    EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, false, false);
}