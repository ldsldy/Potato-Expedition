// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Actors/SkillActor/SkillActor.h"
#include "ProjectileSkillActor.generated.h"

/**
 * 히어로 스킬 중 투사체 형태로 발사되어 타겟과 충돌 시 효과가 발동되는 스킬 액터 클래스
 * 거리에 따라 커지는 로직은 나중에 분리 가능하면 분리 고려
 */
UCLASS()
class UNREAL_PROJECTG_API AProjectileSkillActor : public ASkillActor
{
	GENERATED_BODY()
	
public:
    AProjectileSkillActor();

protected:
    virtual void InitFromConfig(const FHeroSpawnableConfig& InConfig, int32 InAbilityLevel) override;
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    virtual void SpawnFollowUpActor() override;

private:
    void StartGrowthUpdateTimerIfNeeded();
    void StopGrowthUpdateTimer();
    void UpdateGrowthByTime();

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SkillActor|Components")
    TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

    // ==================================================
    // 성장 사용 여부
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SkillActor|Growth")
    bool bEnableTimeGrowth = false;

    // 아래 변수들은 성장 ON일 때만 노출
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SkillActor|Growth", meta = (EditCondition = "bEnableTimeGrowth", EditConditionHides, ClampMin = "1.0"))
    float MaxGrowthMultiplier = 2.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SkillActor|Growth", meta = (EditCondition = "bEnableTimeGrowth", EditConditionHides, ClampMin = "0.01"))
    float GrowthDuration = 4.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SkillActor|Growth", meta = (EditCondition = "bEnableTimeGrowth", EditConditionHides, ClampMin = "0.01"))
    float GrowthUpdateInterval = 0.05f;

    UPROPERTY(BlueprintReadOnly, Category = "SkillActor|Growth")
    FVector BaseActorScale = FVector::OneVector;

    UPROPERTY(BlueprintReadOnly, Category = "SkillActor|Growth")
    float ElapsedGrowthTime = 0.0f; // 현재 성장 업데이트가 진행된 시간

    UPROPERTY(BlueprintReadOnly, Category = "SkillActor|Growth")
    float CurrentGrowthMultiplier = 1.0f; // 현재 성장 배율

private:
    FTimerHandle GrowthUpdateTimerHandle;
    bool GrowthFinished = false;
};
