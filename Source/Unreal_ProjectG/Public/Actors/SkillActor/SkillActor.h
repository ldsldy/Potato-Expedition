#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayEffectTypes.h"
#include "DataAssets/Ability/DataAsset_HeroSkillData.h"
#include "SkillActor.generated.h"

class USphereComponent;
class UProjectileMovementComponent;
class UDataAsset_SkillVisualData;

/**
* 스킬 액터(투사체 / 장판 / 폭발 등등)
* 1. SpawnActorDeferred->InitFromConfig를 통하여 스킬 액터가 초기화
* BeginPlay에서 타이머/콜리전을 설정하므로 InitFromConfig가 반드시 먼저 호출되어야 함
* 
* 동작 방식은 Config를 통하여 결정
* Speed > 0 : 투사체
* TickInterval > 0 : 다단히트/장판
* TickInterval == 0 충돌 즉시 이펙트 적용
* 
* 반납 시 Owner에게 Shared.Event.ActorDestroy 태그 전달(기존 이벤트 태그 호환)
* GA의 WaitEvent가 이 이벤트를 받아 다음 액션을 실행
*/

UCLASS()
class UNREAL_PROJECTG_API ASkillActor : public AActor
{
	GENERATED_BODY()
	
public:	
	ASkillActor();

    // Beginplay보다 먼저 호출을 보장 받아야 함
    virtual void InitFromConfig(const FHeroSpawnableConfig& InConfig, int32 InAbilityLevel = 1);

    // --- [오브젝트 풀링 전용 이벤트] ---
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Pooling")
    void OnActivatedFromPool();
    virtual void OnActivatedFromPool_Implementation();

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Pooling")
    void OnReturnedToPool();
    virtual void OnReturnedToPool_Implementation();

    // Task가 반납 이벤트 수신에 사용할 태그
    FGameplayTag GetReturnEventTag() const;
    UE_DEPRECATED(5.5, "Use GetReturnEventTag()")
    FGameplayTag GetDestroyedEventTag() const; // legacy API

    UFUNCTION(BlueprintPure, Category = "SkillActor|Runtime")
    FORCEINLINE float GetRuntimeEffectMultiplier() const { return RuntimeEffectMultiplier; }

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    //==================================================
    // BP에서 사용할 이벤트
    //==================================================
    // 틱마다 호출
    UFUNCTION(BlueprintImplementableEvent, Category = "SkillActor|Events", meta = (DisplayName = "On Tick Effect For Target"))
    void OnTickEffectForTarget(AActor* TargetActor);

    // 즉발 충돌 시 호출
    UFUNCTION(BlueprintImplementableEvent, Category = "SkillActor|Events", meta = (DisplayName = "On Instant Hit"))
    void OnInstantHit(AActor* TargetActor);

    UFUNCTION(BlueprintImplementableEvent, Category = "SkillActor|Events", meta = (DisplayName = "On Tick For Owner"))
    void OnTickForOwner(AActor* TargetActor);

    //==================================================
    // BP에서 호출 가능한 함수
    // ==================================================
    // 특정 타겟에 GE적용
    UFUNCTION(BlueprintCallable, Category = "SkillActor")
    void ApplyEffectsToTarget(AActor* TargetActor);

    // 반납 트리거
    UFUNCTION(BlueprintCallable, Category = "SkillActor")
    void NotifyAndReturnToPool();

    // 기존 BP 함수명 호환용
    UFUNCTION(BlueprintCallable, Category = "SkillActor",
        meta = (DeprecatedFunction, DeprecationMessage = "Use NotifyAndReturnToPool"))
    void NotifyAndDestroy();


    //==================================================
    // 스케일 증가 멀티플라이어 함수( 나중에 분리 고려 )
    //==================================================
    UFUNCTION(BlueprintCallable, Category = "SkillActor|Runtime")
    void SetRuntimeMultipliers(float InScaleMultiplier, float InEffectMultiplier);

    UFUNCTION(BlueprintPure, Category = "SkillActor|Runtime")
    FORCEINLINE float GetRuntimeScaleMultiplier() const { return RuntimeScaleMultiplier; }
    
    void RebuildEffectSpecsFromConfig();
    void PropagateRuntimeMultipliersTo(ASkillActor* TargetSkillActor) const;

    // 후속 액터 스폰: Config.NextSpawn이 유효하면 해당 설정으로 액터 스폰
    virtual void SpawnFollowUpActor();
    FHeroSpawnableConfig MakeSpawnableConfigFromFollowUp(const FSkillActorFollowUpSpawnConfig& FollowUpConfig) const;

private:
    void StartLifeSpanTimerIfNeeded();
    void ClearRuntimeTimers();
    void OnLifeSpanExpired();

    // 충돌 이벤트 콜백
    UFUNCTION()
    void OnSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
    
    UFUNCTION()
    void OnSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

    //==================================================
    // 게임 플레이 이펙트 적용 함수
    //==================================================
    
    // TickInterval > 0; 타이머 콜백
    void HandleTickEffects();

    // 단발성, 충돌 시 바로 적용
    void HandleInstantEffects(AActor* TargetActor);

    // TargetPolicy에 따라 타겟이 유효한지 검사
    bool IsValidTarget(AActor* TargetActor) const;

    // 반납 직전 처리: 이벤트 호출 및 GA 이벤트 트리거, 후속 액터 스폰
    void HandleBeforeReturnToPool();

    // 수동으로 겹치는 타겟 목록 갱신 (주로 장판에서 사용, TickInterval > 0인 경우)
    void RefreshOverlappingTargets();

    TArray<FGameplayEffectSpecHandle> BuildEffectSpecsFromConfigs(const TArray<FEffectConfig>& EffectConfigs) const;
protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SkillActor|Components")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SkillActor|Components")
    TObjectPtr<USphereComponent> CollisionComponent;

    // 날아가는 동안 재생할 프로젝타일 이펙트 컴포넌트 (없을 수도 있음)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SkillActor|Components")
    TObjectPtr<UNiagaraComponent> ActorVFXComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SkillActor|Components")
    TObjectPtr<UAudioComponent> ActorSFXComponent;

    UPROPERTY(BlueprintReadOnly, Category = "SkillActor")
    FHeroSpawnableConfig Config;

    UPROPERTY(BlueprintReadOnly, Category = "SkillActor")
    TArray<FGameplayEffectSpecHandle> EffectSpecHandles;

    UPROPERTY(BlueprintReadOnly, Category = "SkillActor")
    TArray<TObjectPtr<AActor>> OverlappingTargets;

    FTimerHandle TickEffectTimerHandle;
    FTimerHandle LifeSpanTimerHandle;

    bool bReturnNotified = false; // 반납 통보 여부 (중복 방지)
    float TickInterval = 0.f; // 타이머 기반 효과 적용 간격 (LifeSpan / HitsPerLifeSpan)
    int32 CurrentTickCount = 0; // 현재 인터발 수 (HitsPerLifeSpan과 비교하여 타이머 종료 여부 결정)
    int32 CachedAbilityLevel = 1; // GA 레벨에 따른 효과 적용을 위해 캐싱

    static const FGameplayTag ReturnEventTag; // 반납 이벤트 태그(기존 태그 재사용)

    //==================================================
    // 스케일 증가 멀티플라이어 함수( 나중에 분리 고려 )
    //==================================================
    UPROPERTY(BlueprintReadOnly, Category = "SkillActor|Runtime")
    float RuntimeScaleMultiplier = 1.f;

    UPROPERTY(BlueprintReadOnly, Category = "SkillActor|Runtime")
    float RuntimeEffectMultiplier = 1.f;
};
