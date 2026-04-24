// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/AbilityTasks/SkillAbilityTask_Buff.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystem/Abilities/PGGameplayAbility.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/PGCharacterAttributeSet.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "DataAssets/Ability/DataAsset_SkillVisualData.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "NiagaraFunctionLibrary.h"
#include "PGFunctionLibrary.h"
#include "PGGameplayTags.h"

namespace BuffDebugConstants
{
    // 버프가 적용된 타겟을 디버그로 표시할 때, 우선순위에 따른 색상 그라데이션을 계산하는 헬퍼 함수
    FColor GetPriorityGradientColor(const int32 Index, const int32 NumTargets)
    {
        if (NumTargets <= 1)
        {
            return FColor::Green;
        }

        const float Alpha = FMath::Clamp(
            static_cast<float>(Index) / static_cast<float>(NumTargets - 1),
            0.f,
            1.f);

        // 우선순위가 높을수록 녹색에 가깝고, 낮을수록 빨간색에 가까운 그라데이션 색상을 계산
        const FLinearColor GradientColor = FLinearColor::LerpUsingHSV(
            FLinearColor::Green,
            FLinearColor::Red,
            Alpha);

        return GradientColor.ToFColor(true);
    }
}

USkillAbilityTask_Buff* USkillAbilityTask_Buff::Create(
    UGameplayAbility* OwningAbility,
    const FSkillActionRow& ActionRow,
    const FGameplayAbilityTargetDataHandle& InTriggerTargetData)
{
    USkillAbilityTask_Buff* Task = NewAbilityTask<USkillAbilityTask_Buff>(OwningAbility);
    Task->ActionRowData = ActionRow;
    Task->TriggerTargetData = InTriggerTargetData;
    return Task;
}

void USkillAbilityTask_Buff::Activate()
{
    const FHeroBuffConfig& Config = ActionRowData.BuffConfig;

    if (!Ability || !Ability->GetAvatarActorFromActorInfo())
    {
        CancelTask();
        return;
    }

    bWaitMontageFinish = (Config.Montage != nullptr);
    bBuffApplied = false;
    bTaskResolved = false;

    if (bWaitMontageFinish)
    {
        UAbilityTask_WaitGameplayEvent* BuffEventTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
            Ability,
            PGGameplayTags::Shared_Event_SupportExecute);
        BuffEventTask->EventReceived.AddDynamic(this, &USkillAbilityTask_Buff::OnBuffEventReceived);
        BuffEventTask->ReadyForActivation();

        UAbilityTask_PlayMontageAndWait* MontageTask =
            UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(Ability, NAME_None, Config.Montage);

        MontageTask->OnCompleted.AddDynamic(this, &USkillAbilityTask_Buff::OnMontageFinished);
        MontageTask->OnBlendOut.AddDynamic(this, &USkillAbilityTask_Buff::OnMontageFinished);
        MontageTask->OnCancelled.AddDynamic(this, &USkillAbilityTask_Buff::OnMontageCancelled);
        MontageTask->OnInterrupted.AddDynamic(this, &USkillAbilityTask_Buff::OnMontageCancelled);
        MontageTask->ReadyForActivation();

        return;
    }

    if (ApplyBuffOnce())
    {
        CompleteTask();
    }
    else
    {
        CancelTask();
    }
}

void USkillAbilityTask_Buff::OnMontageFinished()
{
    if (bTaskResolved)
    {
        return;
    }

    if (!bBuffApplied && !ApplyBuffOnce())
    {
        CancelTask();
        return;
    }

    CompleteTask();
}

void USkillAbilityTask_Buff::OnMontageCancelled()
{
    if (bTaskResolved)
    {
        return;
    }

    CancelTask();
}

void USkillAbilityTask_Buff::OnBuffEventReceived(FGameplayEventData Payload)
{
    (void)Payload;

    if (bBuffApplied)
    {
        return;
    }

    ApplyBuffOnce();
}

bool USkillAbilityTask_Buff::ApplyBuffOnce()
{
    if (bBuffApplied)
    {
        return true;
    }

    FVector CenterLocation = FVector::ZeroVector;
    if (!ResolveCenterLocation(CenterLocation))
    {
        return false;
    }

    // CenterLocation을 기준으로 타겟들 수집
    // 수집된 타겟들에게 버프 효과 적용
    TArray<AActor*> Targets;
    GatherTargets(CenterLocation, Targets);
    ApplyEffectsToTargets(Targets);
    bBuffApplied = true;


    DebugDrawAppliedTargets(Targets);

    return true;
}

bool USkillAbilityTask_Buff::ResolveCenterLocation(FVector& OutCenter) const
{
    AActor* AvatarActor = Ability ? Ability->GetAvatarActorFromActorInfo() : nullptr;
    if (!AvatarActor)
    {
        return false;
    }

    const FHeroBuffConfig& Config = ActionRowData.BuffConfig;
    if (Config.TargetPolicy == ESkillTargetPolicy::Self)
    {
        OutCenter = AvatarActor->GetActorLocation();
        return true;
    }

    if (Config.CenterPolicy == EHeroBuffCenterPolicy::AtTriggerPoint && TriggerTargetData.Num() > 0)
    {
        for (int32 Index = 0; Index < TriggerTargetData.Num(); ++Index)
        {
            const FGameplayAbilityTargetData* Data = TriggerTargetData.Get(Index);
            if (!Data)
            {
                continue;
            }

            if (const FHitResult* Hit = Data->GetHitResult())
            {
                OutCenter = Hit->Location;
                return true;
            }

            const TArray<TWeakObjectPtr<AActor>> Actors = Data->GetActors();
            for (const TWeakObjectPtr<AActor>& WeakActor : Actors)
            {
                if (AActor* TargetActor = WeakActor.Get())
                {
                    OutCenter = TargetActor->GetActorLocation();
                    return true;
                }
            }
        }
    }

    OutCenter = AvatarActor->GetActorLocation();
    return true;
}

void USkillAbilityTask_Buff::GatherTargets(const FVector& CenterLocation, TArray<AActor*>& OutTargets) const
{
    OutTargets.Reset();

    AActor* AvatarActor = Ability ? Ability->GetAvatarActorFromActorInfo() : nullptr;
    if (!AvatarActor)
    {
        return;
    }

    const FHeroBuffConfig& Config = ActionRowData.BuffConfig;
    if (Config.TargetPolicy == ESkillTargetPolicy::Self)
    {
        OutTargets.Add(AvatarActor);
        return;
    }

    TArray<AActor*> OverlapActors;
    UKismetSystemLibrary::SphereOverlapActors(
        AvatarActor,
        CenterLocation,
        Config.Radius,
        { UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_Pawn) },
        nullptr,
        TArray<AActor*>(),
        OverlapActors);

    for (AActor* Candidate : OverlapActors)
    {
        if (IsTargetAllowed(Candidate))
        {
            OutTargets.Add(Candidate);
        }
    }

    switch (Config.SelectRule)
    {
    case EHeroBuffSelectRule::AllInRange:
        return;

    case EHeroBuffSelectRule::NearestN:
        OutTargets.Sort([CenterLocation](const AActor& A, const AActor& B)
        {
            return FVector::DistSquared(A.GetActorLocation(), CenterLocation) < FVector::DistSquared(B.GetActorLocation(), CenterLocation);
        });
        break;

    case EHeroBuffSelectRule::LowestHealthN:
        // 체력 퍼센테이지가 낮은 순으로 오름차순 정렬
        OutTargets.Sort([this](const AActor& A, const AActor& B)
        {
            return ResolveTargetHealthPercentage(&A) < ResolveTargetHealthPercentage(&B);
        });
        break;

    default:
        return;
    }

    if (Config.MaxTargets > 0 && OutTargets.Num() > Config.MaxTargets)
    {
        OutTargets.SetNum(Config.MaxTargets);
    }
}

bool USkillAbilityTask_Buff::IsTargetAllowed(AActor* Candidate) const
{
    if (!Candidate || !Ability)
    {
        return false;
    }

    AActor* AvatarActor = Ability->GetAvatarActorFromActorInfo();
    if (!AvatarActor)
    {
        return false;
    }

    switch (ActionRowData.BuffConfig.TargetPolicy)
    {
    case ESkillTargetPolicy::Enemy:
        return UPGFunctionLibrary::IsTargetCharacterHostile(AvatarActor, Candidate);

    case ESkillTargetPolicy::Ally:
        return !UPGFunctionLibrary::IsTargetCharacterHostile(AvatarActor, Candidate);

    case ESkillTargetPolicy::AllyWithOutSelf:
        return !UPGFunctionLibrary::IsTargetCharacterHostile(AvatarActor, Candidate) && Candidate != AvatarActor;

    case ESkillTargetPolicy::Self:
        return Candidate == AvatarActor;

    default:
        return false;
    }
}

float USkillAbilityTask_Buff::GetTargetHealth(const AActor* Target) const
{
    // Target이 유효하지 않거나 ASC를 가져올 수 없는 경우 최대값을 반환하여 정렬 시 뒤로 가도록 함
    if (!Target)
    {
        return TNumericLimits<float>::Max();
    }

    UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(const_cast<AActor*>(Target));
    if (!TargetASC)
    {
        return TNumericLimits<float>::Max();
    }

    return TargetASC->GetNumericAttribute(UPGCharacterAttributeSet::GetHealthAttribute());
}

float USkillAbilityTask_Buff::GetTargetMaxHealth(const AActor* Target) const
{
    // Target이 유효하지 않거나 ASC를 가져올 수 없는 경우 최솟값을 반환하여 정렬 시 뒤로 가도록 함
    if (!Target)
    {
        return TNumericLimits<float>::Min();
    }

    UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(const_cast<AActor*>(Target));
    if (!TargetASC)
    {
        return TNumericLimits<float>::Min();
    }

    return TargetASC->GetNumericAttribute(UPGCharacterAttributeSet::GetMaxHealthAttribute());
}

float USkillAbilityTask_Buff::ResolveTargetHealthPercentage(const AActor* Target) const
{
    const float Health = GetTargetHealth(Target);
    const float MaxHealth = GetTargetMaxHealth(Target);
    if (MaxHealth <= 0.f)
    {
        return 0.f;
    }
    return Health / MaxHealth;
}

void USkillAbilityTask_Buff::DebugDrawAppliedTargets(const TArray<AActor*>& Targets) const
{
#if UE_BUILD_SHIPPING || UE_BUILD_TEST
    return;
#else

    const FHeroBuffConfig& Config = ActionRowData.BuffConfig;

    if (!Config.bDebugMode)
    {
        return;
    }

    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }

    const FVector OwnerLocation = GetAvatarActor()->GetActorLocation();
    const FVector OwnerDrawLocation = OwnerLocation + FVector(0.f, 0.f, 50.f);

    switch (Config.SelectRule)
    {
        case EHeroBuffSelectRule::NearestN:
        {
            for (int32 TargetIndex = 0; TargetIndex < Targets.Num(); ++TargetIndex)
            {
                AActor* Target = Targets[TargetIndex];
                if (!Target) continue;

                const FVector TargetLocation = Target->GetActorLocation();
                const FVector TargetDrawLocation = TargetLocation + FVector(0.f, 0.f, 50.f);
                const float Distance = FVector::Dist(OwnerLocation, TargetLocation);
                const FColor PriorityColor = BuffDebugConstants::GetPriorityGradientColor(TargetIndex, Targets.Num());
                const FString DebugText = FString::Printf(
                    TEXT("Applied Buff\nPriority: %d/%d\nDistance: %.1f"),
                    TargetIndex + 1,
                    Targets.Num(),
                    Distance 
                );
                DrawDebugLine(World, OwnerDrawLocation, TargetDrawLocation, PriorityColor, false, 3, 0, 1.8f);
                DrawDebugSphere(World, TargetDrawLocation, 35.f, 12, PriorityColor, false, 3, 0, 1.8f);
                DrawDebugString(World, TargetDrawLocation + FVector(0.f, 0.f, 45.f), DebugText, nullptr, FColor::White, 3, false);
            }
            break;
        }

        case EHeroBuffSelectRule::LowestHealthN:
        {
            for (int32 TargetIndex = 0; TargetIndex < Targets.Num(); ++TargetIndex)
            {
                AActor* Target = Targets[TargetIndex];
                if (!Target) continue;

                const FVector TargetLocation = Target->GetActorLocation();
                const FVector TargetDrawLocation = TargetLocation + FVector(0.f, 0.f, 50.f);
                const float HealthPercentage = ResolveTargetHealthPercentage(Target) * 100.f;
                const FColor PriorityColor = BuffDebugConstants::GetPriorityGradientColor(TargetIndex, Targets.Num());
                const FString DebugText = FString::Printf(
                    TEXT("Applied Buff\nPriority: %d/%d\nHealth: %.1f%%"),
                    TargetIndex + 1,
                    Targets.Num(),
                    HealthPercentage
                );
                DrawDebugLine(World, OwnerDrawLocation, TargetDrawLocation, PriorityColor, false, 3, 0, 1.8f);
                DrawDebugSphere(World, TargetDrawLocation, 35.f, 12, PriorityColor, false, 3, 0, 1.8f);
                DrawDebugString(World, TargetDrawLocation + FVector(0.f, 0.f, 45.f), DebugText, nullptr, FColor::White, 3, false);
            }
            break;
        }

        case EHeroBuffSelectRule::AllInRange:
        {
            for (AActor* Target : Targets)
            {
                const FVector TargetLocation = Target->GetActorLocation();
                const FVector TargetDrawLocation = TargetLocation + FVector(0.f, 0.f, 50.f);
                DrawDebugLine(World, OwnerDrawLocation, TargetDrawLocation, FColor::Green, false, 3, 0, 1.8f);
                DrawDebugSphere(World, TargetDrawLocation, 35.f, 12, FColor::Blue, false, 3, 0, 1.8f);
            }
            break;
        }

        default:
            break;
    }
#endif
}

void USkillAbilityTask_Buff::ApplyEffectsToTargets(const TArray<AActor*>& Targets) const
{
    if (Targets.IsEmpty()) return;

    UPGGameplayAbility* PGAbility = Cast<UPGGameplayAbility>(Ability);
    
    if (!PGAbility) return;

    for (const FEffectConfig& EffectConfig : ActionRowData.BuffConfig.Effects)
    {
        FGameplayEffectSpecHandle SpecHandle = PGAbility->MakeOutgoingEffectSpecFromEffectConfig(EffectConfig);
        
        if (!SpecHandle.IsValid()) continue;

        const FGameplayEffectContextHandle CueContext = AddActorCueIntoSpecHandle(SpecHandle, EffectConfig); // 적용 전

        for (AActor* TargetActor : Targets)
        {
            if (!TargetActor) continue;

            const FActiveGameplayEffectHandle AppliedHandle = PGAbility->NativeApplyEffectSpecHandleToTarget(TargetActor, SpecHandle);

            const FGameplayEffectContextHandle ContextToUse = CueContext.IsValid() ? CueContext : (SpecHandle.Data.IsValid() ? SpecHandle.Data->GetContext() : FGameplayEffectContextHandle());
            
            ExecuteStaticCue(TargetActor, EffectConfig, ContextToUse); // 적용 후

        }
    }

    /*const TArray<FGameplayEffectSpecHandle> SpecHandles =
        PGAbility->MakeOutgoingEffectSpecsFromEffectConfigs(ActionRowData.BuffConfig.Effects);

    for (AActor* TargetActor : Targets)
    {
        if (!TargetActor)
        {
            continue;
        }

        for (const FGameplayEffectSpecHandle& SpecHandle : SpecHandles)
        {
            if (SpecHandle.IsValid())
            {
                PGAbility->NativeApplyEffectSpecHandleToTarget(TargetActor, SpecHandle);

            }
        }
    }*/
}

void USkillAbilityTask_Buff::CompleteTask()
{
    if (bTaskResolved)
    {
        return;
    }

    bTaskResolved = true;
    OnCompleted.Broadcast({});
    EndTask();
}

void USkillAbilityTask_Buff::CancelTask()
{
    if (bTaskResolved)
    {
        return;
    }

    bTaskResolved = true;
    OnCancelled.Broadcast({});
    EndTask();
}

void USkillAbilityTask_Buff::OnDestroy(bool bInOwnerFinished)
{
    Super::OnDestroy(bInOwnerFinished);
}
