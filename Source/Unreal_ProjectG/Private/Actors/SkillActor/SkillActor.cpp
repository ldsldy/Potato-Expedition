#include "Actors/SkillActor/SkillActor.h"
#include "Components/SphereComponent.h"
#include "NiagaraComponent.h"
#include "Components/AudioComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "DataAssets/Ability/DataAsset_SkillVisualData.h"
#include "PGGameplayTags.h"
#include "PGFunctionLibrary.h"
#include "NiagaraFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Types/PGDataTableStruct.h"

const FGameplayTag ASkillActor::DestroyedEventTag = PGGameplayTags::Shared_Event_ActorDestroy;


ASkillActor::ASkillActor()
{
    PrimaryActorTick.bCanEverTick = false;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);

    CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComponent"));
    CollisionComponent->SetupAttachment(SceneRoot);
    CollisionComponent->SetCollisionProfileName(TEXT("OverlapAllDynamic"));

    ActorVFXComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("ActorVFXComponent"));
    ActorVFXComponent->SetupAttachment(CollisionComponent);

    ActorSFXComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("ActorSFXComponent"));
    ActorSFXComponent->SetupAttachment(CollisionComponent);
}

// 초기화
void ASkillActor::InitFromConfig(const FHeroSpawnableConfig& InConfig, int32 InAbilityLevel)
{
    Config = InConfig;
    CachedAbilityLevel = InAbilityLevel;

    if(Config.Radius > 0.f)
    {
        const float FinalRadius = Config.Radius * RuntimeEffectMultiplier;
        CollisionComponent->SetSphereRadius(FinalRadius);
    }

    if (Config.LifeSpan > 0.f)
    {
        SetLifeSpan(Config.LifeSpan);
    }

    RebuildEffectSpecsFromConfig();
}

FGameplayTag ASkillActor::GetDestroyedEventTag() const
{
    return DestroyedEventTag;
}

void ASkillActor::BeginPlay()
{
    Super::BeginPlay();

    bDestroyNotified = false;

    CollisionComponent->OnComponentBeginOverlap.AddDynamic(this, &ASkillActor::OnSphereBeginOverlap);
    CollisionComponent->OnComponentEndOverlap.AddDynamic(this, &ASkillActor::OnSphereEndOverlap);

    if (Config.HitsPerLifeSpan > 0.f)
    {
        RefreshOverlappingTargets();
        CurrentTickCount = 0;

        TickInterval = Config.LifeSpan / Config.HitsPerLifeSpan;
        GetWorldTimerManager().SetTimer(
            TickEffectTimerHandle,
            this,
            &ASkillActor::HandleTickEffects,
            TickInterval,
            true,
            0.f);
    }
}

// ==================================================
// 오버랩
// ==================================================
bool ASkillActor::IsValidTarget(AActor* TargetActor) const
{
    if (!TargetActor) return false;

        // TargetPolicy에 따른 타겟 유효성 검사 로직 구현 (예: 적 팀인지, 아군 팀인지, 특정 태그가 있는지 등)
    switch (Config.TargetPolicy)
    {
    case ESkillTargetPolicy::Enemy:
    {
        UE_LOG(LogTemp, Log, TEXT("Is Target Hostile? %s"), UPGFunctionLibrary::IsTargetCharacterHostile(GetOwner(), TargetActor) ? TEXT("Yes") : TEXT("No"));
        return UPGFunctionLibrary::IsTargetCharacterHostile(GetOwner(), TargetActor);
    }
    case ESkillTargetPolicy::Ally:
        return !UPGFunctionLibrary::IsTargetCharacterHostile(GetOwner(), TargetActor);
    case ESkillTargetPolicy::AllyWithOutSelf:
        return !UPGFunctionLibrary::IsTargetCharacterHostile(GetOwner(), TargetActor) && TargetActor != GetOwner();
    case ESkillTargetPolicy::Self:
        return TargetActor == GetOwner();
    }

    return false;
}

void ASkillActor::OnSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    if (!OtherActor) return;

    // TargetPolicy 기반 필터링
    if (!IsValidTarget(OtherActor)) return;

    if (Config.HitsPerLifeSpan > 0.f)
    {
        // 틱형은 HandleTickEffects에서 매 틱 재조회로 처리
        return;
    }
    else
    {
        OverlappingTargets.AddUnique(OtherActor);
        HandleInstantEffects(OtherActor);
    }
}

void ASkillActor::OnSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
    if (!OtherActor) return;

    // OverlappingTargets를 돌면서 OtherActor와 일치하는 항목 제거
    OverlappingTargets.RemoveAll([OtherActor](const TObjectPtr<AActor>& Target) {
        return Target.Get() == OtherActor;
        });
}

// ==================================================
// 내부 효과 처리
// ==================================================

void ASkillActor::HandleTickEffects()
{
    // 디버그용 콜리전 스피어 시각화
    //DrawDebugSphere(GetWorld(), GetActorLocation(), CollisionComponent->GetScaledSphereRadius(), 12, FColor::Red, false, TickInterval);

    if(CurrentTickCount >= Config.HitsPerLifeSpan)
    {
        GetWorldTimerManager().ClearTimer(TickEffectTimerHandle);
        return;
    }

    // 매 틱마다 현재 overlap 상태 재수집
    RefreshOverlappingTargets();

    OnTickForOwner(GetOwner());
    
    for (const TObjectPtr<AActor>& WeakTarget : OverlappingTargets)
    {
        if (!WeakTarget) continue;

        // 1. GE 적용
        ApplyEffectsToTarget(WeakTarget.Get());

        // 2. BP 이벤트 호출
        // BP_SkillActor 자손에서 OnTickEffectForTarget 이벤트 구현하여 이펙트 적용 시각화
        OnTickEffectForTarget(WeakTarget.Get());
    }

    CurrentTickCount++;
}

void ASkillActor::HandleInstantEffects(AActor* TargetActor)
{
    // 1. GE 적용
    ApplyEffectsToTarget(TargetActor);

    // 2. BP 이벤트 호출
    OnInstantHit(TargetActor);

    // 투사체는 첫 충돌 후 파괴
    if (Config.Speed > 0.f)
    {
        NotifyAndDestroy();
    }
}

//=================================================
// GE 적용
//==================================================
void ASkillActor::ApplyEffectsToTarget(AActor* TargetActor)
{
    if (!TargetActor || EffectSpecHandles.IsEmpty()) return;

    UAbilitySystemComponent* TargetASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(TargetActor);
    if (!TargetASC) return;

    UAbilitySystemComponent* SourceASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(GetOwner());
    if (!SourceASC) return;

    for(const FGameplayEffectSpecHandle& SpecHandle : EffectSpecHandles)
    {
        if (!SpecHandle.IsValid()) continue;
        SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);

        UE_LOG(LogTemp, Log, TEXT("Applied EffectSpec to Target: %s"), *TargetActor->GetName());
    }
}

//===================================================
// 헬퍼 함수
//===================================================
void ASkillActor::HandlePreDestroy()
{
    if (bDestroyNotified) return;
    bDestroyNotified = true;

    if (ActorSFXComponent && ActorSFXComponent->IsPlaying())
    {
        ActorSFXComponent->Stop();
    }

    // 파괴 위치 전달
    FGameplayAbilityTargetData_SingleTargetHit* Data = new FGameplayAbilityTargetData_SingleTargetHit();

    FHitResult HitResult;
    HitResult.Location = GetActorLocation();
    Data->HitResult = HitResult;

    FGameplayEventData Payload;
    Payload.TargetData.Add(Data);

    UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(GetOwner(), DestroyedEventTag, Payload);

    // 후속 스폰
    SpawnFollowUpActor();

    // BP 이벤트 호출
    OnSkillActorDestroyed();
}

void ASkillActor::RefreshOverlappingTargets()
{
    OverlappingTargets.Reset();

    if (!CollisionComponent) return;

    TArray<AActor*> CurrentOverlaps;
    CollisionComponent->GetOverlappingActors(CurrentOverlaps);

    for (AActor* Candidate : CurrentOverlaps)
    {
        if (!IsValid(Candidate)) continue;
        if (!IsValidTarget(Candidate)) continue;

        OverlappingTargets.AddUnique(Candidate);
    }
}

FHeroSpawnableConfig ASkillActor::MakeSpawnableConfigFromFollowUp(const FSkillActorFollowUpSpawnConfig& FollowUpConfig) const
{
    FHeroSpawnableConfig OutConfig;
    OutConfig.ActorClass = FollowUpConfig.ActorClass;
    OutConfig.ActorType = FollowUpConfig.ActorType;
    OutConfig.Effects = FollowUpConfig.Effects;
    OutConfig.SpawnLocationPolicy = ESpawnLocation::AtCaster; // 액터는 항상 현재 액터 위치에 스폰이기에 큰 의미 없음
    OutConfig.TargetPolicy = FollowUpConfig.TargetPolicy;
    OutConfig.Speed = FollowUpConfig.Speed;
    OutConfig.MaxRange = FollowUpConfig.MaxRange;
    OutConfig.LifeSpan = FollowUpConfig.LifeSpan;
    OutConfig.HitsPerLifeSpan = FollowUpConfig.HitsPerLifeSpan;
    OutConfig.Radius = FollowUpConfig.Radius;
    OutConfig.VisualAsset = FollowUpConfig.VisualAsset;
    OutConfig.SpawnOffsetRow = FollowUpConfig.SpawnOffsetRow;
    return OutConfig;
}

TArray<FGameplayEffectSpecHandle> ASkillActor::BuildEffectSpecsFromConfigs(const TArray<FEffectConfig>& EffectConfigs) const
{
    AActor* SourceActor = GetOwner();
    if (!SourceActor) SourceActor = GetInstigator();
    if (!SourceActor) return {};

    UPGAbilitySystemComponent* SourceASC = UPGFunctionLibrary::NativeGetPGASCFromActor(SourceActor);

    if (!SourceASC) return {};

    return UPGFunctionLibrary::MakeOutgoingGameplayEffectSpecsFromEffectConfigs(
        SourceASC,
        EffectConfigs,
        SourceActor,
        SourceActor,
        CachedAbilityLevel);
}

void ASkillActor::SpawnFollowUpActor()
{
    if (!Config.NextSpawn.ActorClass) return;

    AActor* OwnerActor = GetOwner();
    if (!OwnerActor || !GetWorld()) return;

    FVector SpawnOffset = FVector::ZeroVector;
    if (const FSpawnOffsetRow* Row = Config.NextSpawn.SpawnOffsetRow.GetRow<FSpawnOffsetRow>(TEXT("SpawnOffset")))
    {
        SpawnOffset = Row->SpawnOffset;
    }

    const FTransform SpawnTransform(GetActorRotation(), GetActorLocation() + SpawnOffset);

    ASkillActor* Spawned = GetWorld()->SpawnActorDeferred<ASkillActor>(
        Config.NextSpawn.ActorClass,
        SpawnTransform,
        OwnerActor,
        Cast<APawn>(OwnerActor),
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

    if (!Spawned) return;

    FHeroSpawnableConfig FollowUpConfig = MakeSpawnableConfigFromFollowUp(Config.NextSpawn);


    // 현재 배율을 후속 액터로 전달(후속이 Init에서 Spec 생성할 때 반영됨)
    Spawned->SetOwner(OwnerActor);
    Spawned->SetInstigator(Cast<APawn>(OwnerActor));
    PropagateRuntimeMultipliersTo(Spawned);
    Spawned->InitFromConfig(FollowUpConfig, CachedAbilityLevel);
    Spawned->FinishSpawning(SpawnTransform);
}

// ==================================================
// 파괴
// ==================================================
void ASkillActor::NotifyAndDestroy()
{
    if (bDestroyNotified) return;
    
    HandlePreDestroy();
    Destroy(); // TODO: 나중에 풀링 시스템 고려
}

void ASkillActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    GetWorldTimerManager().ClearTimer(TickEffectTimerHandle);

    HandlePreDestroy();

    Super::EndPlay(EndPlayReason);
}


// ==================================================
// ==================================================
// ==================================================
void ASkillActor::SetRuntimeMultipliers(float InScaleMultiplier, float InEffectMultiplier)
{
    RuntimeScaleMultiplier = FMath::Max(0.f, InScaleMultiplier);
    RuntimeEffectMultiplier = FMath::Max(0.f, InEffectMultiplier);
}

void ASkillActor::PropagateRuntimeMultipliersTo(ASkillActor* TargetSkillActor) const
{
    if (!TargetSkillActor)
    {
        return;
    }

    TargetSkillActor->SetRuntimeMultipliers(RuntimeScaleMultiplier, RuntimeEffectMultiplier);
}


void ASkillActor::RebuildEffectSpecsFromConfig()
{
    TArray<FEffectConfig> Adjusted = Config.Effects;

    if (!FMath::IsNearlyEqual(RuntimeEffectMultiplier, 1.f))
    {
        for (FEffectConfig& Effect : Adjusted)
        {
            const float Current = Effect.Multiplier.GetValueAtLevel(CachedAbilityLevel);
            Effect.Multiplier = FScalableFloat(Current * RuntimeEffectMultiplier);
        }
    }

    EffectSpecHandles = BuildEffectSpecsFromConfigs(Adjusted);
}
