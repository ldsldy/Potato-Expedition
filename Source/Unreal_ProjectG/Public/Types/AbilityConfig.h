// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "StructUtils/InstancedStruct.h"
#include "ScalableFloat.h"
#include "GameplayTagContainer.h"
#include "Types/PGEnumTypes.h"
#include "Engine/DataTable.h"
#include "AbilityConfig.generated.h"

class APGProjectileBase;
class UNiagaraComponent;
class UGameplayEffect;
class UPGGameplayAbility;
class APetCharacter;
class AAOESkillActor;
class UDataAsset_SkillVisualData;
class APGMageMagicBase;
/**
 * 유닛과 캐릭터의 어빌리티의 변수들을 담는 구조체
 */

 // 어빌리티 클래스와 그에 대한 설정을 담는 구조체
USTRUCT(BlueprintType)
struct FAbilityEntry
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TSubclassOf<UPGGameplayAbility> AbilityClass;

    UPROPERTY(EditAnywhere, meta=(BaseStruct="/Script/Unreal_ProjectG.AbilityConfig"))
    FInstancedStruct AbilityConfig;
};

//// 버프를 적용하는 이펙트 클래스와 그에 대한 설정을 담는 구조체
USTRUCT(BlueprintType)
struct FBuffEffectConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    EBuffDurationType BuffDurationType = EBuffDurationType::Infinite;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TSubclassOf<UGameplayEffect> BuffEffectClass;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FScalableFloat SkillMultiplier;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FScalableFloat BaseBuffAmount;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (EditCondition = "BuffDurationType == EBuffDurationType::Duration")) // 버프 지속시간이 Duration일 때만 보이는 설정
    FScalableFloat BuffDuration;
};

//==========================================================================================================
//==========================================================================================================
//==========================================================================================================
// 모든 어빌리티 설정의 부모 구조체, FInstancedStruct로 구조체도 다형성을 가지게 하기 위해서 만들어짐
USTRUCT(BlueprintType)
struct FAbilityConfig
{
	GENERATED_BODY()

    virtual ~FAbilityConfig() = default;
};

USTRUCT(BlueprintType)
struct FHeroSpawnPetAbilityConfig : public FAbilityConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<TSoftClassPtr<APetCharacter>> SpawnedPetClasses; // 스폰할 펫 클래스
};


//==========================================================================================================
// 유닛 어빌리티 설정 구조체들
//==========================================================================================================
USTRUCT(BlueprintType)
struct FUnitBaseMeleeAttackAbilityConfig : public FAbilityConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TSubclassOf<UGameplayEffect> DamageEffectClass; // 데미지 계산 클래스

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FScalableFloat SkillMultiplier; // 스킬 계수

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 MaxHitTargets = 1; // 최대 공격 가능한 적의 수

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float MeleeAttackDamageRadius = 100.f; // 공격 트레이스 반경

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<TSoftObjectPtr<UAnimMontage>> MeleeAttackMontages; // 공격 애니메이션 몽타주들

    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (Categories = "GameplayCue"))
    FGameplayTag HitImpactCueTag; // 공격이 적중했을 때 재생할 이펙트 태그
};

USTRUCT(BlueprintType)
struct FUnitSpawnProjectileAbilityConfig : public FAbilityConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TSubclassOf<UGameplayEffect> DamageEffectClass; // 데미지 계산 클래스

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FScalableFloat SkillMultiplier; // 스킬 계수

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TSoftClassPtr<APGProjectileBase> SpawnedProjectileClass; // 스폰할 프로젝타일 클래스

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<TSoftObjectPtr<UAnimMontage>> SpawnProjectileMontages; // 공격 애니메이션 몽타주들

    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (Categories = "GameplayCue"))
    FGameplayTag SpawnCueTag; //스폰할때, 재생할 이펙트 태그 
};

USTRUCT(BlueprintType)
struct FUnitSpawnActorAbilityConfig : public FAbilityConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TSubclassOf<UGameplayEffect> DamageEffectClass; 

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FScalableFloat SkillMultiplier;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TSoftClassPtr<AActor> SpawnedActorClass; 

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 SpawnCount=0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<TSoftObjectPtr<UAnimMontage>> SpawnActorMontages;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (Categories = "GameplayCue"))
    FGameplayTag SpawnCueTag; 
};

USTRUCT(BlueprintType)
struct FSharedBuffAuraAbilityConfig : public FAbilityConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<FBuffEffectConfig> Buffs; // 수치 버프량 계산을 위한 데이터 배열

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float BuffAuraRadius = 0.0; // 버프 오라의 반경

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TSoftObjectPtr<UMaterialInterface> AuraRadiusDecalMaterial; // 버프 오라의 범위를 보여주는 데칼 머티리얼
};

USTRUCT(BlueprintType)
struct FUnitBuffAuraAbilityConfig : public FAbilityConfig
{
    GENERATED_BODY()

    // 장판 반경 내 아군에게 부여할 힐/버프 이펙트 (GE)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Support")
    TSubclassOf<UGameplayEffect> SupportEffectClass;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TSoftObjectPtr<class UNiagaraSystem> BuffEffect;

    // 시전 애니메이션
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability|Support")
    TObjectPtr<UAnimMontage> SupportMontage;

    // 힐/버프 계수
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Support")
    FScalableFloat SupportSkillMultiplier;

    // 지원 유닛 주위에 깔릴 장판의 탐색 반경
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Support")
    float SupportRadius = 500.f;

    // 장판이 유닛을 따라다닐지(오라 형태), 시전 위치에 남을지 결정
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability|Support")
    bool bAttachToUnit = false;
};

USTRUCT(BlueprintType)
struct FUnitSpawnMagicAbilityConfig : public FAbilityConfig
{
    GENERATED_BODY()

    // 데미지 계산 클래스
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<TSubclassOf<UGameplayEffect>> DamageEffectClass;

    // 스킬 계수
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FScalableFloat SkillMultiplier; 

    // 스폰할 클래스를 APGMageMagicBase로 지정
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TSoftClassPtr<APGMageMagicBase> SpawnedMagicClass;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<TSoftObjectPtr<UAnimMontage>> SpawnMagicMontages; // 시전 애니메이션 몽타주들

    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta = (Categories = "GameplayCue"))
    FGameplayTag SpawnCueTag; // 스폰 시 재생할 이펙트 태그 (필요시)
};

//==========================================================================================================
// EffectConfig
//==========================================================================================================

USTRUCT(BlueprintType)
struct FEffectConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "이펙트 클래스"))
    TSubclassOf<UGameplayEffect> EffectClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "스킬 배수 (0 = 설정 안함)"))
    FScalableFloat Multiplier = FScalableFloat(1.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", DisplayName = "기본 수치 (버프만 사용)"))
    FScalableFloat BaseAmount = FScalableFloat(0.f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", DisplayName = "지속 시간"))
    FScalableFloat Duration = FScalableFloat(0.f);

    //액션에 적용할 큐 태그
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "적용할 큐 태그", Categories = "GameplayCue"))
    FGameplayTag AppliedCueTag;

    // 큐 태그와 함께 보낼 Enum
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "큐 스위처", Categories = "GameplayCue"))
    FGameplayTag CueVariantTag; // 큐 내부에서 Switch 구분을 위한 태그입니다.
};

//==========================================================================================================
// AOE 스킬용 Enum
//==========================================================================================================

UENUM(BlueprintType)
enum class ESpawnLocation : uint8
{
    AtCaster                    UMETA(DisplayName = "시전자위치"),   // 캐릭터 위치에서 즉시 소환
    AtTargetPoint               UMETA(DisplayName = "타겟 포인트"),  // 타겟 포인트에 소환
};

UENUM(BlueprintType)
enum class ESkillActorType : uint8
{
    None            UMETA(DisplayName = "없음"),
    Projectile      UMETA(DisplayName = "투사체"),
    InstantAOE      UMETA(DisplayName = "즉시 범위"),
    PersistentAOE   UMETA(DisplayName = "지속 범위"),
};

//==========================================================================================================
// 캐릭터 어빌리티 태스크의 설정 구조체들
//==========================================================================================================

USTRUCT(BlueprintType)
struct FHeroMeleeTraceConfig : public FAbilityConfig
{
    GENERATED_BODY()

    // 근접 공격 트레이스 반경
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Radius = 100.f;

    // 근접 공격 트레이스 사거리
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float TraceOffsetRange = 150.f;

    // 최대 공격 가능한 적의 수
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 MaxHit = 1;

    // 적용할 이펙트 배열
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TArray<FEffectConfig> Effects;

    // 액터가 재생할 몽타주
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TObjectPtr<UAnimMontage> Montage;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "디버그용 트레이스 시작화"))
    bool bDrawDebugTrace = false;
};

USTRUCT(BlueprintType)
struct FSkillActorFollowUpSpawnConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "액터 클래스"))
    TSubclassOf<AActor> ActorClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "스폰 타입"))
    ESkillActorType ActorType = ESkillActorType::None;

    // 적용할 이펙트 배열
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "이펙트 배열"))
    TArray<FEffectConfig> Effects;

    // 스킬 타겟팅 정책
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "타겟팅 정책"))
    ESkillTargetPolicy TargetPolicy = ESkillTargetPolicy::Enemy;

    // 액터 속도, 0이면 고정(장판/폭발)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "속도", EditCondition = "ActorType == ESkillActorType::Projectile", EditConditionHides))
    float Speed = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "최대 사거리", EditCondition = "ActorType == ESkillActorType::Projectile", EditConditionHides))
    float MaxRange = 0.f;

    // 액터가 존재하는 시간
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "지속 시간", EditCondition = "ActorType == ESkillActorType::PersistentAOE", EditConditionHides))
    float LifeSpan = 0.1f;

    // 0이면 단발성, 0초과면 장형 틱 데미지
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "시간 동안 히트 횟수", EditCondition = "ActorType == ESkillActorType::PersistentAOE", EditConditionHides))
    float HitsPerLifeSpan = 1.f;

    // 콜리전 반경(0이면 기본 콜리전 사용)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "반경"))
    float Radius = 0.f;

    // 시각 연출을 위한 에셋
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "FX 에셋"))
    TObjectPtr<UDataAsset_SkillVisualData> VisualAsset;

    // 소환 위치 조정을 위한 오프셋
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "소환 오프셋"))
    FDataTableRowHandle SpawnOffsetRow;
};

USTRUCT(BlueprintType)
struct FHeroSpawnableConfig : public FAbilityConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "액터 클래스"))
    TSubclassOf<AActor> ActorClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "스폰 타입"))
    ESkillActorType ActorType = ESkillActorType::None;

    // 적용할 이펙트 배열
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "이펙트 배열"))
    TArray<FEffectConfig> Effects;

    // 스폰 위치 정책
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "스폰 위치 정책"))
    ESpawnLocation SpawnLocationPolicy = ESpawnLocation::AtCaster;

    // 스킬 타겟팅 정책
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "타겟팅 정책"))
    ESkillTargetPolicy TargetPolicy = ESkillTargetPolicy::Enemy;

    // 액터 속도, 0이면 고정(장판/폭발)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "속도", EditCondition = "ActorType == ESkillActorType::Projectile", EditConditionHides))
    float Speed = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "최대 사거리", EditCondition = "ActorType == ESkillActorType::Projectile", EditConditionHides))
    float MaxRange = 0.f;

    // 액터가 존재하는 시간
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "지속 시간", EditCondition = "ActorType == ESkillActorType::PersistentAOE", EditConditionHides))
    float LifeSpan = 0.1f;

    // 0이면 단발성, 0초과면 장형 틱 데미지
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "시간 동안 히트 횟수", EditCondition = "ActorType == ESkillActorType::PersistentAOE", EditConditionHides))
    float HitsPerLifeSpan = 0.f;

    // 콜리전 반경(0이면 기본 콜리전 사용)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "반경"))
    float Radius = 0.f;

    // AtTargetPoint일 때, 사용할 데칼  머티리얼
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "범위 지정 데칼", EditCondition = "SpawnLocationPolicy == ESpawnLocation::AtTargetPoint", EditConditionHides))
    TObjectPtr<UMaterialInterface> IndicatorDecalMaterial;

    // 액터가 재생할 몽타주
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "몽타주"))
    TObjectPtr<UAnimMontage> Montage;

    // 시각 연출을 위한 에셋
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "FX 에셋"))
    TObjectPtr<UDataAsset_SkillVisualData> VisualAsset;

    // 소환 위치 조정을 위한 오프셋
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "소환 오프셋"))
    FDataTableRowHandle SpawnOffsetRow;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "연속된 스폰 액터 배열"))
    FSkillActorFollowUpSpawnConfig NextSpawn; // 체인 스폰을 위한 다음 스폰 설정 배열
};

UENUM(BlueprintType)
enum class EHeroBuffCenterPolicy : uint8
{
    AtCaster        UMETA(DisplayName = "시전자 위치"),
    AtTriggerPoint  UMETA(DisplayName = "트리거 위치"),
};

UENUM(BlueprintType)
enum class EHeroBuffSelectRule : uint8
{
    AllInRange      UMETA(DisplayName = "범위 내 전체"),
    NearestN        UMETA(DisplayName = "가까운 순"),
    LowestHealthN   UMETA(DisplayName = "체력 낮은 순"),
};

USTRUCT(BlueprintType)
struct FHeroBuffConfig : public FAbilityConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "이펙트 배열"))
    TArray<FEffectConfig> Effects;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "타겟팅 정책"))
    ESkillTargetPolicy TargetPolicy = ESkillTargetPolicy::Ally;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "중심 위치", EditCondition = "TargetPolicy != ESkillTargetPolicy::Self", EditConditionHides))
    EHeroBuffCenterPolicy CenterPolicy = EHeroBuffCenterPolicy::AtCaster;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0.0", DisplayName = "범위", EditCondition = "TargetPolicy != ESkillTargetPolicy::Self", EditConditionHides))
    float Radius = 500.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "적용 정책", EditCondition = "TargetPolicy != ESkillTargetPolicy::Self", EditConditionHides))
    EHeroBuffSelectRule SelectRule = EHeroBuffSelectRule::AllInRange;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
        meta = (ClampMin = "1", DisplayName = "최대 적용 대상 수", EditCondition = "TargetPolicy != ESkillTargetPolicy::Self && SelectRule != EHeroBuffSelectRule::AllInRange", EditConditionHides))
    int32 MaxTargets = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "몽타주"))
    TObjectPtr<UAnimMontage> Montage = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (DisplayName = "디버그 모드"))
    bool bDebugMode = false;
};
