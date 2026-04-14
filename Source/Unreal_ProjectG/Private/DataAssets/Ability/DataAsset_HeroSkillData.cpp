#include "DataAssets/Ability/DataAsset_HeroSkillData.h"

#include "Algo/Sort.h"
#include "AbilitySystemComponent.h"
#include "HAL/PlatformTime.h"
#include "PGFunctionLibrary.h"

namespace HeroSkillDataRuntime
{
    struct FActiveEventModifierRef
    {
        const FHeroSkillEventActionModifier* Modifier = nullptr;
        int32 SourceIndex = INDEX_NONE;
    };

    static TMap<uint64, double> GEventModifierLastProcTime;

    static bool IsSetConditionSatisfied(
        const UAbilitySystemComponent* AbilitySystem,
        const FGameplayTag& RequiredSetTag,
        int32 RequiredSetPieceCount)
    {
        if (!RequiredSetTag.IsValid())
        {
            return true;
        }

        if (!AbilitySystem)
        {
            return false;
        }

        const int32 CurrentPieceCount = UPGFunctionLibrary::ResolveSetPieceCount(AbilitySystem, RequiredSetTag);
        return CurrentPieceCount >= RequiredSetPieceCount;
    }

    static bool IsSourceTagConditionSatisfied(const FHeroSkillEventActionModifier& Modifier, const FGameplayTag& SourceEventTag)
    {
        if (Modifier.RequiredSourceEventTags.IsEmpty())
        {
            return true;
        }

        if (!SourceEventTag.IsValid())
        {
            return false;
        }

        return Modifier.RequiredSourceEventTags.HasTagExact(SourceEventTag);
    }

    static void GatherActiveBuildModifiers(
        const UDataAsset_HeroSkillData* SkillData,
        const UAbilitySystemComponent* AbilitySystem,
        TArray<const FHeroSkillBuildModifier*>& OutActiveModifiers)
    {
        OutActiveModifiers.Reset();

        TArray<const FHeroSkillBuildModifier*> NonExclusive;
        TMap<FGameplayTag, const FHeroSkillBuildModifier*> HighestTierBySet;

        for (const FHeroSkillBuildModifier& Modifier : SkillData->SetBonusModifiers)
        {
            if (!IsSetConditionSatisfied(AbilitySystem, Modifier.RequiredSetTag, Modifier.RequiredSetPieceCount))
            {
                continue;
            }

            if (!Modifier.RequiredSetTag.IsValid() || !Modifier.bUseHighestTierOnly)
            {
                NonExclusive.Add(&Modifier);
                continue;
            }

            // bUseHighestTierOnly가 true인 경우, 동일한 RequiredSetTag를 가진 모디파이어 중 RequiredSetPieceCount가 가장 높은 모디파이어만 찾아서 적용
            const FHeroSkillBuildModifier* const* Found = HighestTierBySet.Find(Modifier.RequiredSetTag);
            if (!Found || ((*Found)->RequiredSetPieceCount < Modifier.RequiredSetPieceCount))
            {
                HighestTierBySet.Add(Modifier.RequiredSetTag, &Modifier);
            }
        }

        OutActiveModifiers = MoveTemp(NonExclusive);
        for (const TPair<FGameplayTag, const FHeroSkillBuildModifier*>& Pair : HighestTierBySet)
        {
            OutActiveModifiers.Add(Pair.Value);
        }

        Algo::SortBy(OutActiveModifiers, [](const FHeroSkillBuildModifier* Modifier)
        {
            return Modifier ? Modifier->RequiredSetPieceCount : 0;
        });
    }

    static void GatherActiveEventModifiers(
        const UDataAsset_HeroSkillData* SkillData,
        const UAbilitySystemComponent* AbilitySystem,
        EHeroSkillEventTrigger Trigger,
        const FGameplayTag& SourceEventTag,
        TArray<FActiveEventModifierRef>& OutActiveModifiers)
    {
        OutActiveModifiers.Reset();

        TArray<FActiveEventModifierRef> NonExclusive;
        TMap<FGameplayTag, FActiveEventModifierRef> HighestTierBySet;

        for (int32 Index = 0; Index < SkillData->EventActionModifiers.Num(); ++Index)
        {
            const FHeroSkillEventActionModifier& Modifier = SkillData->EventActionModifiers[Index];

            if (Modifier.Trigger != Trigger)
            {
                continue;
            }

            if (!IsSetConditionSatisfied(AbilitySystem, Modifier.RequiredSetTag, Modifier.RequiredSetPieceCount))
            {
                continue;
            }

            if (!IsSourceTagConditionSatisfied(Modifier, SourceEventTag))
            {
                continue;
            }

            const FActiveEventModifierRef Ref{ &Modifier, Index };
            if (!Modifier.RequiredSetTag.IsValid() || !Modifier.bUseHighestTierOnly)
            {
                NonExclusive.Add(Ref);
                continue;
            }

            const FActiveEventModifierRef* Found = HighestTierBySet.Find(Modifier.RequiredSetTag);
            if (!Found || (!Found->Modifier || Found->Modifier->RequiredSetPieceCount < Modifier.RequiredSetPieceCount))
            {
                HighestTierBySet.Add(Modifier.RequiredSetTag, Ref);
            }
        }

        OutActiveModifiers = MoveTemp(NonExclusive);
        for (const TPair<FGameplayTag, FActiveEventModifierRef>& Pair : HighestTierBySet)
        {
            OutActiveModifiers.Add(Pair.Value);
        }

        OutActiveModifiers.Sort([](const FActiveEventModifierRef& A, const FActiveEventModifierRef& B)
        {
            const int32 ACount = A.Modifier ? A.Modifier->RequiredSetPieceCount : 0;
            const int32 BCount = B.Modifier ? B.Modifier->RequiredSetPieceCount : 0;
            return ACount < BCount;
        });
    }

    static uint64 MakeCooldownKey(
        const UDataAsset_HeroSkillData* SkillData,
        const UAbilitySystemComponent* AbilitySystem,
        EHeroSkillEventTrigger Trigger,
        const FGameplayTag& SourceEventTag,
        int32 ModifierSourceIndex,
        EHeroSkillEventCooldownScope Scope)
    {
        uint64 Key = HashCombineFast(GetTypeHash(SkillData), GetTypeHash(AbilitySystem));
        Key = HashCombineFast(Key, GetTypeHash(ModifierSourceIndex));

        if (Scope == EHeroSkillEventCooldownScope::PerTrigger || Scope == EHeroSkillEventCooldownScope::PerSourceTag)
        {
            Key = HashCombineFast(Key, GetTypeHash(static_cast<uint8>(Trigger)));
        }

        if (Scope == EHeroSkillEventCooldownScope::PerSourceTag)
        {
            Key = HashCombineFast(Key, GetTypeHash(SourceEventTag));
        }

        return Key;
    }

    static bool PassEventProcAndCooldown(
        const UDataAsset_HeroSkillData* SkillData,
        const UAbilitySystemComponent* AbilitySystem,
        EHeroSkillEventTrigger Trigger,
        const FGameplayTag& SourceEventTag,
        const FActiveEventModifierRef& Ref)
    {
        if (!Ref.Modifier)
        {
            return false;
        }

        if (Ref.Modifier->ProcChance < 1.f && FMath::FRand() > Ref.Modifier->ProcChance)
        {
            return false;
        }

        if (Ref.Modifier->InternalCooldown <= 0.f)
        {
            return true;
        }

        const uint64 Key = MakeCooldownKey(
            SkillData,
            AbilitySystem,
            Trigger,
            SourceEventTag,
            Ref.SourceIndex,
            Ref.Modifier->CooldownScope);

        const double Now = FPlatformTime::Seconds();
        if (const double* LastProcTime = GEventModifierLastProcTime.Find(Key))
        {
            if ((Now - *LastProcTime) < static_cast<double>(Ref.Modifier->InternalCooldown))
            {
                return false;
            }
        }

        GEventModifierLastProcTime.Add(Key, Now);
        return true;
    }
}

void UDataAsset_HeroSkillData::BuildRuntimeActionSequence(
    const UAbilitySystemComponent* AbilitySystem,
    int32 AbilityLevel,
    TArray<FSkillActionRow>& OutRuntimeActions) const
{
    OutRuntimeActions = MainSequence;
    if (OutRuntimeActions.IsEmpty())
    {
        return;
    }

    TArray<const FHeroSkillBuildModifier*> ActiveModifiers;
    HeroSkillDataRuntime::GatherActiveBuildModifiers(this, AbilitySystem, ActiveModifiers);

    for (const FHeroSkillBuildModifier* Modifier : ActiveModifiers)
    {
        if (!Modifier)
        {
            continue;
        }

        const float CoefMultiplier = Modifier->EffectCoefficientMultiplier.GetValueAtLevel(AbilityLevel);
        MultiplyAllEffectMultipliers(OutRuntimeActions, CoefMultiplier, AbilityLevel);

        for (const FHeroSkillBuildOp& Op : Modifier->Ops)
        {
            switch (Op.OpType)
            {
            case EHeroSkillBuildOpType::AddAction:
                if (OutRuntimeActions.Num() < MaxRuntimeActionCount)
                {
                    OutRuntimeActions.Add(Op.ActionToInject);
                }
                break;

            case EHeroSkillBuildOpType::InsertActionBefore:
            {
                const int32 TargetIndex = FindUniqueActionIndexById(OutRuntimeActions, Op.TargetActionID);
                if (TargetIndex != INDEX_NONE && OutRuntimeActions.Num() < MaxRuntimeActionCount)
                {
                    OutRuntimeActions.Insert(Op.ActionToInject, TargetIndex);
                }
                break;
            }

            case EHeroSkillBuildOpType::InsertActionAfter:
            {
                const int32 TargetIndex = FindUniqueActionIndexById(OutRuntimeActions, Op.TargetActionID);
                if (TargetIndex != INDEX_NONE && OutRuntimeActions.Num() < MaxRuntimeActionCount)
                {
                    OutRuntimeActions.Insert(Op.ActionToInject, TargetIndex + 1);
                }
                break;
            }

            case EHeroSkillBuildOpType::OverrideAction:
            {
                const int32 TargetIndex = FindUniqueActionIndexById(OutRuntimeActions, Op.TargetActionID);
                if (TargetIndex != INDEX_NONE)
                {
                    FSkillActionRow Overridden = Op.OverrideActionRow;
                    if (Overridden.ActionID.IsNone())
                    {
                        Overridden.ActionID = OutRuntimeActions[TargetIndex].ActionID;
                    }
                    OutRuntimeActions[TargetIndex] = Overridden;
                }
                break;
            }

            case EHeroSkillBuildOpType::OverrideActionValue:
            {
                const int32 TargetIndex = FindUniqueActionIndexById(OutRuntimeActions, Op.TargetActionID);
                if (TargetIndex != INDEX_NONE)
                {
                    const float Value = Op.ActionScalarValue.GetValueAtLevel(AbilityLevel);
                    OverrideActionScalar(OutRuntimeActions[TargetIndex], Op.ActionScalarField, Value);
                }
                break;
            }

            case EHeroSkillBuildOpType::OverrideEffectValue:
            {
                const int32 TargetIndex = FindUniqueActionIndexById(OutRuntimeActions, Op.TargetActionID);
                if (TargetIndex != INDEX_NONE)
                {
                    if (TArray<FEffectConfig>* Effects = GetMutableEffects(OutRuntimeActions[TargetIndex]))
                    {
                        const float Value = Op.EffectScalarValue.GetValueAtLevel(AbilityLevel);
                        OverrideEffectScalar(*Effects, Op.EffectIndex, Op.EffectScalarField, Value);
                    }
                }
                break;
            }

            default:
                break;
            }
        }
    }

    if (OutRuntimeActions.Num() > MaxRuntimeActionCount)
    {
        OutRuntimeActions.SetNum(MaxRuntimeActionCount);
    }
}

void UDataAsset_HeroSkillData::BuildRuntimeEventActionSequence(
    const UAbilitySystemComponent* AbilitySystem,
    int32 AbilityLevel,
    TArray<FSkillActionRow>& OutEventActions,
    EHeroSkillEventTrigger Trigger,
    const FGameplayTag& SourceEventTag) const
{
    OutEventActions.Reset();

    TArray<HeroSkillDataRuntime::FActiveEventModifierRef> ActiveModifiers;
    HeroSkillDataRuntime::GatherActiveEventModifiers(this, AbilitySystem, Trigger, SourceEventTag, ActiveModifiers);

    for (const HeroSkillDataRuntime::FActiveEventModifierRef& Ref : ActiveModifiers)
    {
        if (!Ref.Modifier)
        {
            continue;
        }

        if (!HeroSkillDataRuntime::PassEventProcAndCooldown(this, AbilitySystem, Trigger, SourceEventTag, Ref))
        {
            continue;
        }

        const float CoefMultiplier = Ref.Modifier->EffectCoefficientMultiplier.GetValueAtLevel(AbilityLevel);
        for (const FSkillActionRow& SourceAction : Ref.Modifier->EventActions)
        {
            if (OutEventActions.Num() >= MaxEventActionCount)
            {
                return;
            }

            FSkillActionRow RuntimeAction = SourceAction;
            TArray<FSkillActionRow> TempActions;
            TempActions.Add(RuntimeAction);
            MultiplyAllEffectMultipliers(TempActions, CoefMultiplier, AbilityLevel);
            RuntimeAction = TempActions[0];

            OutEventActions.Add(RuntimeAction);
        }
    }
}

int32 UDataAsset_HeroSkillData::FindUniqueActionIndexById(const TArray<FSkillActionRow>& Actions, const FName& ActionId)
{
    if (ActionId.IsNone())
    {
        return INDEX_NONE;
    }

    int32 FoundIndex = INDEX_NONE;
    for (int32 i = 0; i < Actions.Num(); ++i)
    {
        if (Actions[i].ActionID == ActionId)
        {
            if (FoundIndex != INDEX_NONE)
            {
                return INDEX_NONE;
            }
            FoundIndex = i;
        }
    }

    return FoundIndex;
}

TArray<FEffectConfig>* UDataAsset_HeroSkillData::GetMutableEffects(FSkillActionRow& Action)
{
    switch (Action.ActionType)
    {
    case ESkillActionType::SpawnActor:
        return &Action.SpawnableConfig.Effects;
    case ESkillActionType::MeleeTrace:
        return &Action.MeleeTraceConfig.Effects;
    case ESkillActionType::Buff:
        return &Action.BuffConfig.Effects;
    default:
        return nullptr;
    }
}

void UDataAsset_HeroSkillData::OverrideActionScalar(FSkillActionRow& Action, EHeroSkillActionScalarField Field, float Value)
{
    if (Action.ActionType == ESkillActionType::SpawnActor)
    {
        switch (Field)
        {
        case EHeroSkillActionScalarField::SpawnRadius:
            Action.SpawnableConfig.Radius = Value;
            return;
        case EHeroSkillActionScalarField::SpawnLifeSpan:
            Action.SpawnableConfig.LifeSpan = Value;
            return;
        case EHeroSkillActionScalarField::SpawnHtisPerLifeSpan:
            Action.SpawnableConfig.HitsPerLifeSpan = Value;
            return;
        default:
            return;
        }
    }

    if (Action.ActionType == ESkillActionType::MeleeTrace)
    {
        switch (Field)
        {
        case EHeroSkillActionScalarField::TraceRadius:
            Action.MeleeTraceConfig.Radius = Value;
            return;
        case EHeroSkillActionScalarField::TraceOffsetRange:
            Action.MeleeTraceConfig.TraceOffsetRange = Value;
            return;
        case EHeroSkillActionScalarField::TraceMaxHit:
            Action.MeleeTraceConfig.MaxHit = FMath::Max(1, FMath::RoundToInt(Value));
            return;
        default:
            return;
        }
    }

    if (Action.ActionType == ESkillActionType::Buff)
    {
        switch (Field)
        {
        case EHeroSkillActionScalarField::BuffRadius:
            Action.BuffConfig.Radius = Value;
            return;
        case EHeroSkillActionScalarField::BuffMaxTargets:
            Action.BuffConfig.MaxTargets = FMath::Max(1, FMath::RoundToInt(Value));
            return;
        default:
            return;
        }
    }
}

void UDataAsset_HeroSkillData::OverrideEffectScalar(
    TArray<FEffectConfig>& Effects,
    int32 EffectIndex,
    EHeroSkillEffectScalarField Field,
    float Value)
{
    auto ApplyScalar = [&](FEffectConfig& Effect)
    {
        switch (Field)
        {
        case EHeroSkillEffectScalarField::EffectMultiplier:
            Effect.Multiplier = FScalableFloat(Value);
            break;
        case EHeroSkillEffectScalarField::EffectBaseAmount:
            Effect.BaseAmount = FScalableFloat(Value);
            break;
        case EHeroSkillEffectScalarField::EffectDuration:
            Effect.Duration = FScalableFloat(Value);
            break;
        default:
            break;
        }
    };

    if (EffectIndex < 0)
    {
        for (FEffectConfig& Effect : Effects)
        {
            ApplyScalar(Effect);
        }
        return;
    }

    if (Effects.IsValidIndex(EffectIndex))
    {
        ApplyScalar(Effects[EffectIndex]);
    }
}

void UDataAsset_HeroSkillData::MultiplyAllEffectMultipliers(TArray<FSkillActionRow>& Actions, float MultiplierAtLevel, int32 AbilityLevel)
{
    if (FMath::IsNearlyEqual(MultiplierAtLevel, 1.f))
    {
        return;
    }

    for (FSkillActionRow& Action : Actions)
    {
        TArray<FEffectConfig>* Effects = GetMutableEffects(Action);
        if (!Effects)
        {
            continue;
        }

        for (FEffectConfig& Effect : *Effects)
        {
            const float Current = Effect.Multiplier.GetValueAtLevel(AbilityLevel);
            Effect.Multiplier = FScalableFloat(Current * MultiplierAtLevel);
        }
    }
}
