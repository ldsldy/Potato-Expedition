#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Types/AbilityConfig.h"
#include "Types/PGEnumTypes.h"
#include "DataAsset_HeroSkillData.generated.h"

class UAbilitySystemComponent;
class UDataAsset_HeroSkillPresentation;

// 액션 타입. 어떤 종류의 액션인지 정의합니다.
UENUM(BlueprintType)
enum class ESkillActionType : uint8
{
    None            UMETA(DisplayName = "None"),
    SpawnActor      UMETA(DisplayName = "SpawnActor"),
    MeleeTrace      UMETA(DisplayName = "MeleeTrace"),
    Buff            UMETA(DisplayName = "Buff"),
};

// 이벤트 트리거 타입. 어떤 상황에서 이벤트 액션이 발동할지 정의합니다.
UENUM(BlueprintType)
enum class EHeroSkillEventTrigger : uint8
{
    OnHit           UMETA(DisplayName = "히트 시"),
    OnKill          UMETA(DisplayName = "킬 시"),
};

// 업그레이드를 어떻게 적용할지 정의하는 열거형. 액션 시퀀스에 구조 변경이나 수치 변경을 가하는 단일 모디파이어를 정의합니다.
UENUM(BlueprintType)
enum class EHeroSkillBuildOpType : uint8
{
    AddAction               UMETA(DisplayName = "액션 추가(끝)"),
    InsertActionBefore      UMETA(DisplayName = "액션 삽입(앞)"),
    InsertActionAfter       UMETA(DisplayName = "액션 삽입(뒤)"),
    OverrideAction          UMETA(DisplayName = "액션 교체"),
    OverrideActionValue     UMETA(DisplayName = "액션 수치 덮어쓰기"),
    OverrideEffectValue     UMETA(DisplayName = "이펙트 수치 덮어쓰기"),
};

// 액션의 수치 중 어떤 것을 덮어쓸지 정의하는 열거형.
UENUM(BlueprintType)
enum class EHeroSkillActionScalarField : uint8
{
    SpawnRadius,
    SpawnLifeSpan,
    SpawnHtisPerLifeSpan,
    TraceRadius,
    TraceOffsetRange,
    TraceMaxHit,
    BuffRadius,
    BuffMaxTargets,
};

// 액션의 이펙트 수치 중 어떤 것을 덮어쓸지 정의하는 열거형.
UENUM(BlueprintType)
enum class EHeroSkillEffectScalarField : uint8
{
    EffectMultiplier,
    EffectBaseAmount,
    EffectDuration,
};

// 이벤트 액션 쿨다운 적용 범위.
UENUM(BlueprintType)
enum class EHeroSkillEventCooldownScope : uint8
{
    Global          UMETA(DisplayName = "전역"),
    PerTrigger      UMETA(DisplayName = "트리거별"),
    PerSourceTag    UMETA(DisplayName = "소스 태그별"),
};

UENUM(BlueprintType)
enum class EHeroSkillBodyMode : uint8
{
    UpperBody       UMETA(DisplayName = "UpperBody"),
    FullBody        UMETA(DisplayName = "FullBody"),
};

// 스킬 액션 시퀀스의 각 액션을 정의하는 구조체. 액션 타입에 따라 필요한 설정을 담는 여러 구성 구조체를 포함합니다.
USTRUCT(BlueprintType)
struct FSkillActionRow
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="액션|기본", meta=(DisplayName="액션 ID"))
    FName ActionID = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="액션|기본", meta=(DisplayName="액션 타입"))
    ESkillActionType ActionType = ESkillActionType::None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="액션|SpawnActor",
        meta=(DisplayName="SpawnActor 설정", EditCondition="ActionType == ESkillActionType::SpawnActor", EditConditionHides))
    FHeroSpawnableConfig SpawnableConfig;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="액션|MeleeTrace",
        meta=(DisplayName="MeleeTrace 설정", EditCondition="ActionType == ESkillActionType::MeleeTrace", EditConditionHides))
    FHeroMeleeTraceConfig MeleeTraceConfig;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "액션|Buff",
        meta = (DisplayName = "Buff 설정", EditCondition = "ActionType == ESkillActionType::Buff", EditConditionHides))
    FHeroBuffConfig BuffConfig;
};

// 업그레이드를 어떻게 적용할지 정의하는 구조체. 액션 시퀀스에 구조 변경이나 수치 변경을 가하는 단일 모디파이어를 정의합니다.
USTRUCT(BlueprintType)
struct FHeroSkillBuildOp
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="업그레이드|기본", meta=(DisplayName="오퍼레이터 타입"))
    EHeroSkillBuildOpType OpType = EHeroSkillBuildOpType::AddAction;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="업그레이드|타겟",
        meta=(DisplayName="대상 액션 ID",
        EditCondition="OpType == EHeroSkillBuildOpType::InsertActionBefore || OpType == EHeroSkillBuildOpType::InsertActionAfter || OpType == EHeroSkillBuildOpType::OverrideAction || OpType == EHeroSkillBuildOpType::OverrideActionValue || OpType == EHeroSkillBuildOpType::OverrideEffectValue",
        EditConditionHides))
    FName TargetActionID = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="업그레이드|구조변경",
        meta=(DisplayName="삽입/추가 액션",
        EditCondition="OpType == EHeroSkillBuildOpType::AddAction || OpType == EHeroSkillBuildOpType::InsertActionBefore || OpType == EHeroSkillBuildOpType::InsertActionAfter",
        EditConditionHides))
    FSkillActionRow ActionToInject;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="업그레이드|구조변경",
        meta=(DisplayName="교체 액션", EditCondition="OpType == EHeroSkillBuildOpType::OverrideAction", EditConditionHides))
    FSkillActionRow OverrideActionRow;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="업그레이드|수치",
        meta=(DisplayName="액션 수치 항목", EditCondition="OpType == EHeroSkillBuildOpType::OverrideActionValue", EditConditionHides))
    EHeroSkillActionScalarField ActionScalarField = EHeroSkillActionScalarField::SpawnRadius;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="업그레이드|수치",
        meta=(DisplayName="액션 수치 값", EditCondition="OpType == EHeroSkillBuildOpType::OverrideActionValue", EditConditionHides))
    FScalableFloat ActionScalarValue = FScalableFloat(0.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="업그레이드|수치",
        meta=(DisplayName="이펙트 인덱스(-1 전체)", EditCondition="OpType == EHeroSkillBuildOpType::OverrideEffectValue", EditConditionHides))
    int32 EffectIndex = -1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="업그레이드|수치",
        meta=(DisplayName="이펙트 수치 항목", EditCondition="OpType == EHeroSkillBuildOpType::OverrideEffectValue", EditConditionHides))
    EHeroSkillEffectScalarField EffectScalarField = EHeroSkillEffectScalarField::EffectMultiplier;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="업그레이드|수치",
        meta=(DisplayName="이펙트 수치 값", EditCondition="OpType == EHeroSkillBuildOpType::OverrideEffectValue", EditConditionHides))
    FScalableFloat EffectScalarValue = FScalableFloat(0.f);
};

// 스킬 업그레이드 시 특정 세트 보너스 조건을 만족할 때 스킬 액션 시퀀스에 구조 변경이나 수치 변경을 가하는 모디파이어
USTRUCT(BlueprintType)
struct FHeroSkillBuildModifier
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="업그레이드|조건", meta=(DisplayName="요구 세트 태그", Categories="Equipment.Set"))
    FGameplayTag RequiredSetTag;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="업그레이드|조건", meta=(DisplayName="요구 세트 개수", ClampMin="1", ClampMax="99"))
    int32 RequiredSetPieceCount = 3;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="업그레이드|조건", meta=(DisplayName="동일 세트 최고 단계만 적용"))
    bool bUseHighestTierOnly = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="업그레이드|실행", meta=(DisplayName="이펙트 계수 배율"))
    FScalableFloat EffectCoefficientMultiplier = FScalableFloat(1.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="업그레이드|실행", meta=(DisplayName="오퍼레이터 목록"))
    TArray<FHeroSkillBuildOp> Ops;
};

USTRUCT(BlueprintType)
struct FHeroSkillEventActionModifier
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="이벤트 액션|조건", meta=(DisplayName="이벤트 트리거"))
    EHeroSkillEventTrigger Trigger = EHeroSkillEventTrigger::OnHit;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="이벤트 액션|조건", meta=(DisplayName="요구 세트 태그", Categories="Equipment.Set"))
    FGameplayTag RequiredSetTag;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="이벤트 액션|조건", meta=(DisplayName="요구 세트 개수", ClampMin="1", ClampMax="99"))
    int32 RequiredSetPieceCount = 3;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="이벤트 액션|조건", meta=(DisplayName="동일 세트 최고 단계만 적용"))
    bool bUseHighestTierOnly = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="이벤트 액션|조건", meta=(DisplayName="소스 이벤트 태그 필터"))
    FGameplayTagContainer RequiredSourceEventTags;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="이벤트 액션|제어", meta=(DisplayName="발동 확률", ClampMin="0.0", ClampMax="1.0"))
    float ProcChance = 1.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="이벤트 액션|제어", meta=(DisplayName="내부 쿨다운(초)", ClampMin="0.0"))
    float InternalCooldown = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="이벤트 액션|제어", meta=(DisplayName="쿨다운 스코프", EditCondition="InternalCooldown > 0.0", EditConditionHides))
    EHeroSkillEventCooldownScope CooldownScope = EHeroSkillEventCooldownScope::PerTrigger;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="이벤트 액션|실행", meta=(DisplayName="이펙트 계수 배율"))
    FScalableFloat EffectCoefficientMultiplier = FScalableFloat(1.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="이벤트 액션|실행", meta=(DisplayName="실행 액션 목록"))
    TArray<FSkillActionRow> EventActions;
};

UCLASS()
class UNREAL_PROJECTG_API UDataAsset_HeroSkillData : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "스킬", meta = (DisplayName = "스킬 토글 여부"))
    bool bIsToggleSkill = false; // UI용

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "스킬", meta = (DisplayName = "바디 모드"))
    EHeroSkillBodyMode BodyMode = EHeroSkillBodyMode::UpperBody;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "스킬", meta = (DisplayName = "스킬 쿨다운"))
    FScalableFloat SkillCooldown = FScalableFloat(0.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="스킬|연출", meta=(DisplayName="스킬 연출 데이터[없으면 연출 없음]"))
    TObjectPtr<UDataAsset_HeroSkillPresentation> PresentationData = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="스킬|메인 시퀀스", meta=(DisplayName="기본 액션 시퀀스"))
    TArray<FSkillActionRow> MainSequence;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="스킬|업그레이드(시작/종료)", meta=(DisplayName="세트 보너스 모디파이어"))
    TArray<FHeroSkillBuildModifier> SetBonusModifiers;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="스킬|이벤트 액션(히트/킬)", meta=(DisplayName="이벤트 액션 모디파이어"))
    TArray<FHeroSkillEventActionModifier> EventActionModifiers;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="스킬|제한", meta=(DisplayName="최대 런타임 액션 수", ClampMin="1"))
    int32 MaxRuntimeActionCount = 64;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="스킬|제한", meta=(DisplayName="최대 이벤트 액션 수", ClampMin="1"))
    int32 MaxEventActionCount = 32;

public:
    UFUNCTION(BlueprintCallable, Category="스킬 데이터")
    void BuildRuntimeActionSequence(const UAbilitySystemComponent* AbilitySystem, int32 AbilityLevel, TArray<FSkillActionRow>& OutRuntimeActions) const;

    UFUNCTION(BlueprintCallable, Category="스킬 데이터")
    void BuildRuntimeEventActionSequence(const UAbilitySystemComponent* AbilitySystem, int32 AbilityLevel, TArray<FSkillActionRow>& OutEventActions, EHeroSkillEventTrigger Trigger, const FGameplayTag& SourceEventTag = FGameplayTag()) const;

private:
    static int32 FindUniqueActionIndexById(const TArray<FSkillActionRow>& Actions, const FName& ActionId);
    static TArray<FEffectConfig>* GetMutableEffects(FSkillActionRow& Action);
    static void OverrideActionScalar(FSkillActionRow& Action, EHeroSkillActionScalarField Field, float Value);
    static void OverrideEffectScalar(TArray<FEffectConfig>& Effects, int32 EffectIndex, EHeroSkillEffectScalarField Field, float Value);
    static void MultiplyAllEffectMultipliers(TArray<FSkillActionRow>& Actions, float MultiplierAtLevel, int32 AbilityLevel);
};
