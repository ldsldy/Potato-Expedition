
#include "AbilitySystem/TargetActor/GATargetActor_AOEGroundTrace.h"
#include "Components/SphereComponent.h"
#include "AbilitySystemComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Components/DecalComponent.h"
#include "PGFunctionLibrary.h"
#include "PGGameplayTags.h"
#include "Interfaces/VisualEffectTargetInterface.h"


AGATargetActor_AOEGroundTrace::AGATargetActor_AOEGroundTrace()
{
    PrimaryActorTick.bCanEverTick = true;
    AOERadiusSphere = CreateDefaultSubobject<USphereComponent>(TEXT("AOERadiusSphere"));
    SetRootComponent(AOERadiusSphere);
    AOERadiusSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    AOERadiusSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
    AOERadiusSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

    AOERadiusSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    AOERadiusSphere->SetGenerateOverlapEvents(true);

    AOEDecalComponent = CreateDefaultSubobject<UDecalComponent>(TEXT("AOEDecalComponent"));
    AOEDecalComponent->SetupAttachment(RootComponent);
    AOEDecalComponent->SetVisibility(false);
}

void AGATargetActor_AOEGroundTrace::StartTargeting(UGameplayAbility* InAbility)
{
    Super::StartTargeting(InAbility);

    //Screen입력 차단 태그 부여
    bAddedBattleUIBlockTag = false;
    CachedOwnerASC.Reset();

    if (InAbility)
    {
        if (UAbilitySystemComponent* ASC = InAbility->GetAbilitySystemComponentFromActorInfo())
        {
            ASC->AddLooseGameplayTag(PGGameplayTags::State_InputBlock_ScreenTouch);
            CachedOwnerASC = ASC;
            bAddedBattleUIBlockTag = true;
        }
    }

    // 오버랩 이벤트 바인딩
    AOERadiusSphere->OnComponentBeginOverlap.AddDynamic(this, &AGATargetActor_AOEGroundTrace::OnSphereOverlapBegin);
    AOERadiusSphere->OnComponentEndOverlap.AddDynamic(this, &AGATargetActor_AOEGroundTrace::OnSphereOverlapEnd);

    // 타게팅 시작 시 이미 눌려 있던 기존 터치는 무시하고,
    // 이후 새로 시작되는 터치만 추적한다.
    bHasActiveTouchIndex = false;
    IgnoredTouchIndices.Empty();
    bIsTouching = false;

    if (APlayerController* PC = Cast<APlayerController>(PrimaryPC))
    {
        for (int32 TouchIndexInt = 0; TouchIndexInt < static_cast<int32>(ETouchIndex::MAX_TOUCHES); ++TouchIndexInt)
        {
            bool bTouchDown = false;
            FVector2D CurrentLocation = FVector2D::ZeroVector;
            const ETouchIndex::Type TouchIndex = static_cast<ETouchIndex::Type>(TouchIndexInt);
            PC->GetInputTouchState(TouchIndex, CurrentLocation.X, CurrentLocation.Y, bTouchDown);
            if (bTouchDown)
            {
                IgnoredTouchIndices.Add(TouchIndex);
            }
        }
    }

    // 데칼 컴포넌트 설정
    AOEDecalComponent->SetDecalMaterial(AOETraceDecalMaterial);
    AOEDecalComponent->SetWorldRotation(AOEDecalRotation);
    AOEDecalComponent->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
    AOEDecalComponent->DecalSize = FVector(10, PreviewRadius, PreviewRadius);

    AOERadiusSphere->SetSphereRadius(PreviewRadius);
}

void AGATargetActor_AOEGroundTrace::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // Screen입력 차단 태그 제거 및 변수 초기화
    if(bAddedBattleUIBlockTag && CachedOwnerASC.IsValid())
    {
        CachedOwnerASC->RemoveLooseGameplayTag(PGGameplayTags::State_InputBlock_ScreenTouch);
    }
    bAddedBattleUIBlockTag = false;
    CachedOwnerASC.Reset();

    ClearAllHighlightedActors();
    Super::EndPlay(EndPlayReason);
}

void AGATargetActor_AOEGroundTrace::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (!PrimaryPC)
    {
        return;
    }

    FVector2D TouchLocation = FVector2D::ZeroVector;
    bool bIsCurrentlyTouching = false;

    if (APlayerController* PC = Cast<APlayerController>(PrimaryPC))
    {
        bool bLostActiveTouchThisFrame = false;

        // 활성중인 터치 인덱스가 있었지만 손을 뗀경우 bHasActiveTouchIndex를 False로
        if (bHasActiveTouchIndex)
        {
            PC->GetInputTouchState(ActiveTouchIndex, TouchLocation.X, TouchLocation.Y, bIsCurrentlyTouching);
            if (!bIsCurrentlyTouching)
            {
                bHasActiveTouchIndex = false;
                bLostActiveTouchThisFrame = true;
            }
        }

        if (!bHasActiveTouchIndex && !bLostActiveTouchThisFrame)
        {
            // 0부터 터치 인덱스를 돌면서 떼진 상태인지 확인. 이미 무시 중인 인덱스는 건너뛰고, 새로 눌린 인덱스가 있으면 그걸 활성 터치 인덱스로 삼는다.
            for (int32 TouchIndexInt = 0; TouchIndexInt < static_cast<int32>(ETouchIndex::MAX_TOUCHES); ++TouchIndexInt)
            {
                bool bTouchDown = false;
                FVector2D CurrentLocation = FVector2D::ZeroVector;
                const ETouchIndex::Type TouchIndex = static_cast<ETouchIndex::Type>(TouchIndexInt);

                PC->GetInputTouchState(TouchIndex, CurrentLocation.X, CurrentLocation.Y, bTouchDown);
                if (!bTouchDown)
                {
                    // 기존에 눌려서 무시 중이던 인덱스도 손을 떼면 다시 후보에 포함
                    IgnoredTouchIndices.Remove(TouchIndex);
                    continue;
                }

                //현재 터치 인덱스가 이미 누른 
                if (IgnoredTouchIndices.Contains(TouchIndex))
                {
                    continue;
                }

                bHasActiveTouchIndex = true;     //활성 터치 인덱스가 있음.
                ActiveTouchIndex = TouchIndex;   //현재 활성 터치 인덱스 저장
                TouchLocation = CurrentLocation; //현재 터치 위치 저장
                bIsCurrentlyTouching = true;
                break;
            }
        }
    }

    // 터치 시작 및 종료 감지
    if (bIsCurrentlyTouching && !bIsTouching)
    {
        OnTouchPressed();
    }
    else if (!bIsCurrentlyTouching && bIsTouching)
    {
        OnTouchReleased();
    }

    bIsTouching = bIsCurrentlyTouching;

    if (bIsTouching)
    {
        FVector WorldLocation, WorldDirection;
        if (PrimaryPC->DeprojectScreenPositionToWorld(TouchLocation.X, TouchLocation.Y, WorldLocation, WorldDirection))
        {
            FHitResult HitResult;
            FVector TraceEnd = WorldLocation + (WorldDirection * 10000.0f);

            FCollisionQueryParams QueryParams;
            QueryParams.AddIgnoredActor(OwnerActor);
            if(PrimaryPC->GetPawn())
            {
                QueryParams.AddIgnoredActor(PrimaryPC->GetPawn());
            }
            if (GetWorld()->LineTraceSingleByChannel(HitResult, WorldLocation, TraceEnd, ECC_GameTraceChannel4))
            {
                LastTouchLocation = HitResult.Location;

                SetActorLocation(LastTouchLocation);

                GEngine->AddOnScreenDebugMessage(-1, 0.f, FColor::Yellow, FString::Printf(TEXT("Hit Location: %s"), *LastTouchLocation.ToString()));
            }
        }
    }

    if (bShowDebugSphere)
    {
        DrawDebugSphere(GetWorld(), LastTouchLocation, AOERadiusSphere->GetScaledSphereRadius(), 12, FColor::Green, false, -1, 0, 2);
    }
}

void AGATargetActor_AOEGroundTrace::OnSphereOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    // 오버랩 팀 확인 및 리턴 추가
    if (!OtherActor)
        return;
    if (OverlappedActors.Contains(OtherActor))
        return;

    // 플레이어 자신의 캐릭터는 무시
    if (PrimaryPC && PrimaryPC->GetPawn() == OtherActor)
        return;

    // 적팀이면 액터 빛나게 하기
    if (IsValidTarget(OtherActor))
    {
        // 오버랩된 액터 추가
        OverlappedActors.AddUnique(OtherActor);
        OnHighlightActorInAOE(OtherActor);
    }
}

void AGATargetActor_AOEGroundTrace::OnSphereOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
    if (!OtherActor)
        return;
    if(!OverlappedActors.Contains(OtherActor))
        return;

    // 오버랩된 액터 제거
    OverlappedActors.Remove(OtherActor);
    OnUnhighlightActorOutAOE(OtherActor);
}

void AGATargetActor_AOEGroundTrace::OnTouchPressed()
{
    // 터치 시작 시에 데칼 컴포넌트 생성
    bIsTouching = true;

    // 데칼 컴포넌트 표시
    AOEDecalComponent->SetVisibility(true);
    
    // 스피어 컴포넌트의 반지름을 설정
    AOERadiusSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
}

void AGATargetActor_AOEGroundTrace::OnTouchReleased()
{
    bIsTouching = false;

    AOERadiusSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // 하이라이트된 액터 모두 초기화
    ClearAllHighlightedActors();

    // 현재 오버랩된 액터들을 기반으로 타겟 데이터 생성
    FGameplayAbilityTargetDataHandle TargetDataHandle;

    // 히트 로케이션만 포함하는 타겟 데이터 생성
    FGameplayAbilityTargetDataHandle TargetData;
    FHitResult HitResult;
    HitResult.Location = LastTouchLocation;
    TargetData.Add(new FGameplayAbilityTargetData_SingleTargetHit(HitResult));
    TargetDataReadyDelegate.Broadcast(TargetData);
    
}

void AGATargetActor_AOEGroundTrace::OnHighlightActorInAOE(AActor* InActor)
{
    // 머티리얼 변경하여 하이라이트 효과 적용
    if (!IsValid(InActor)) return;

    if (!IsValidTarget(InActor)) return;

    if(InActor->GetClass()->ImplementsInterface(UVisualEffectTargetInterface::StaticClass()))
    {
        IVisualEffectTargetInterface::Execute_SetAOEHighlightEnabled(InActor, true);
    }
}

void AGATargetActor_AOEGroundTrace::OnUnhighlightActorOutAOE(AActor* InActor)
{
    // 머티리얼 원래대로 복원
    if (!IsValid(InActor)) return;

    if(InActor->GetClass()->ImplementsInterface(UVisualEffectTargetInterface::StaticClass()))
    {
        IVisualEffectTargetInterface::Execute_SetAOEHighlightEnabled(InActor, false);
    }
}

void AGATargetActor_AOEGroundTrace::ClearAllHighlightedActors()
{
    for(AActor* Actor : OverlappedActors)
    {
        OnUnhighlightActorOutAOE(Actor);
    }
    OverlappedActors.Empty();
}

bool AGATargetActor_AOEGroundTrace::IsValidTarget(AActor* InActor)
{
    if (!IsValid(InActor)) return false;

    // 타겟팅 정책에 따라 유효한 타겟인지 확인
    switch (TargetPolicy)
    {
        case ESkillTargetPolicy::Enemy:
            return UPGFunctionLibrary::IsTargetCharacterHostile(OwnerActor, InActor);
        case ESkillTargetPolicy::AllyWithOutSelf:
            return !UPGFunctionLibrary::IsTargetCharacterHostile(OwnerActor, InActor) && InActor != OwnerActor;
        case ESkillTargetPolicy::Ally:
            return !UPGFunctionLibrary::IsTargetCharacterHostile(OwnerActor, InActor);
        case ESkillTargetPolicy::Self:
            return InActor == OwnerActor;
        default:
            return false;
    }
}
