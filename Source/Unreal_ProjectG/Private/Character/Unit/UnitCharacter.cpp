// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/Unit/UnitCharacter.h"
#include "Character/Unit/SubSystem/UnitSubsystem.h"
#include "Character/Unit/SubSystem/UnitSpawnSubsystem.h"
#include "Components/CapsuleComponent.h"
#include "Engine/AssetManager.h"
#include "DataAssets/StartUp/DataAsset_UnitStartupData.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "AIController.h"
#include "DataAssets/Unit/UnitData.h"
#include "AbilitySystem/PGCharacterAttributeSet.h"
#include "DataAssets/Unit/BranchDataAsset.h"
#include "AbilitySystem/PGAbilitySystemComponent.h"
#include "Components/Visual/CharacterVisualEffectComponent.h"
#include "BrainComponent.h"

AUnitCharacter::AUnitCharacter()
{
    // AI 컨트롤러 빙의 시점 설정
    PrimaryActorTick.bCanEverTick = false;

    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

    UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
    if (MovementComponent)
    {
        MovementComponent->bUseRVOAvoidance = false;
    }

    if (UCapsuleComponent* Capsule = GetCapsuleComponent())
    {
        Capsule->SetRelativeScale3D(FVector(0.7f, 0.7f, 0.7f));
    }

    if (USkeletalMeshComponent* CharacterMesh = GetMesh())
    {
        CharacterMesh->SetRelativeScale3D(FVector(2.5f, 2.5f, 2.5f));
        CharacterMesh->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
        CharacterMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -90.0f)); 
    }

    WeaponMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponMesh"));

    if (WeaponMesh)
    {
        WeaponMesh->SetupAttachment(GetMesh(), TEXT("Weapon_R"));
        WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        WeaponMesh->SetCollisionProfileName(TEXT("NoCollision"));
    }

    UnitVisualEffectComponent = CreateDefaultSubobject<UCharacterVisualEffectComponent>(TEXT("UnitVisualEffectComponent"));
}

void AUnitCharacter::BeginPlay()
{
    Super::BeginPlay();

    AIController = Cast<AAIController>(GetController());

    UE_LOG(LogTemp, Log, TEXT("BeginPlay"));

    InitUnitStartUpData();
}

void AUnitCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    Super::EndPlay(EndPlayReason);
}

void AUnitCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);
}

void AUnitCharacter::SetAOEHighlightEnabled_Implementation(bool bEnabled)
{
    if(UnitVisualEffectComponent)
    {
        UnitVisualEffectComponent->SetAOEHighlightEnabled(bEnabled);
    }
}

void AUnitCharacter::SetHitReactEnabled_Implementation(bool bEnabled)
{
    if(UnitVisualEffectComponent)
    {
        UnitVisualEffectComponent->SetHitReactEnabled(bEnabled);
    }
}

void AUnitCharacter::ResetVisualEffectState_Implementation()
{
    if (UnitVisualEffectComponent)
    {
        UnitVisualEffectComponent->ResetVisualEffectState();
    }
}

void AUnitCharacter::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);

    InitUnitStartUpData();
}

void AUnitCharacter::PostInitializeComponents()
{
    Super::PostInitializeComponents();

    if (!PGAbilitySystemComponent)
    {
        PGAbilitySystemComponent = FindComponentByClass<UPGAbilitySystemComponent>();
    }

    if (!CharacterAttributeSet && PGAbilitySystemComponent)
    {

        CharacterAttributeSet = const_cast<UPGCharacterAttributeSet*>(PGAbilitySystemComponent->GetSet<UPGCharacterAttributeSet>());
    }
}

void AUnitCharacter::InitUnitStartUpData()
{
    if(CharacterStartupData.IsNull())
    {
        return;
    }

    // 비동기로 스타트업 데이터 로드 및 적용
    UAssetManager::GetStreamableManager().RequestAsyncLoad(
        CharacterStartupData.ToSoftObjectPath(),
        FStreamableDelegate::CreateLambda(
            [this]()
            {
                if (!IsValid(this))
                {
                    return;
                }

                if (UDataAsset_StartupDataBase* LoadedData = CharacterStartupData.Get())
                {
                    if (!bIsAbilitiesInitialized && PGAbilitySystemComponent)
                    {
                        LoadedData->GiveToAbilitySystemComponent(PGAbilitySystemComponent, UnitLevel);
                        bIsAbilitiesInitialized = true; 
                    }

                    // 풀 내부(비활성/숨김)에서는 능력 선지급만 수행한다.
                    // 월드 등록/시각적 초기화는 활성화 시점에만 적용한다.
                    if (IsHidden())
                    {
                        return;
                    }

                    UDataAsset_UnitStartupData* StartUpData = Cast<UDataAsset_UnitStartupData>(LoadedData);
                    if (!StartUpData)
                    {
                        return;
                    }

                    if (StartUpData->BranchData)
                    {
                        DetectRangeKey = StartUpData->BranchData->DetectRange;

                        AttackRangeKey = StartUpData->BranchData->AttackRange;

                        SubBTAssetKey = StartUpData->BranchData->SubBTAsset;

                        AttackMarginKey = AttackRangeKey * 0.9f;

                        if (StartUpData->BranchData->BranchTag.IsValid())
                        {
                            BranchTag = StartUpData->BranchData->BranchTag;
                        }
                    }
                    UE_LOG(LogTemp, Log, TEXT("InitUnitStartUpData"));
                    if (CharacterAttributeSet)
                    {
                        CharacterAttributeSet->SetHealth(CharacterAttributeSet->GetMaxHealth());
                        UnitAttackSpeed = 1/CharacterAttributeSet->GetAttackSpeed();
                        UE_LOG(LogTemp, Log, TEXT("HP : %f"), CharacterAttributeSet->GetHealth());
                    }
                    else
                    {
                        UE_LOG(LogTemp, Error, TEXT("[%s] AttributeSet이 없습니다! 블루프린트를 확인하세요."), *GetName());
                    }

                    if (StartUpData->TeamTag.IsValid())
                    {
                        TeamTag = StartUpData->TeamTag;
                    }

                    if (StartUpData->ElementTag.IsValid())
                    {
                        ElementTag = StartUpData->ElementTag;
                    }


                    if (StartUpData->DeadMontage)
                    {
                        UnitDeadMontage = StartUpData->DeadMontage;
                    }

                    //유닛 서브시스템을 이용한 태그별 팀 설정
                    if (UUnitSubsystem* Subsystem = GetWorld()->GetSubsystem<UUnitSubsystem>())
                    {
                        Subsystem->RegisterUnit(this, TeamTag);

                        if (bIsBoss)
                        {
                            Subsystem->OnBossSpawnDelegate.Broadcast(TeamTag);
                        }

                    }


                    //데이터 삽입이 끝나면 델리게이트를 브로드캐스트해서 블랙보드가 값을 받기 시작함
                    if (OnUnitStartUpDataLoadedDelegate.IsBound())
                    {
                        OnUnitStartUpDataLoadedDelegate.Broadcast();
                    }
                }
            }
        )
    );
}


void AUnitCharacter::SetAttackTarget(AActor* InTargetActor)
{
    TargetActor = InTargetActor;

    if (!IsValid(TargetActor)) return;

    if (!IsValid(AIController))
    {
        AIController = Cast<AAIController>(GetController());
    }

    if (!IsValid(AIController)) return;

    UBlackboardComponent* BBComp = AIController->GetBlackboardComponent();
    if (IsValid(BBComp))
    {
        BBComp->SetValueAsObject(TEXT("AttackTargetBase"), InTargetActor);
    }

}

void AUnitCharacter::OnDie()
{
    if (bIsDead)
    {
        return;
    }

    bIsDead = true;

    if (IsValid(AIController))
    {
        if (UBrainComponent* BrainComp = AIController->GetBrainComponent())
        {
            BrainComp->StopLogic(TEXT("Unit is Dead"));
        }

        AIController->StopMovement();
        AIController->ClearFocus(EAIFocusPriority::Gameplay);
    }

    if (UCharacterMovementComponent* MovementComp = GetCharacterMovement())
    {
        MovementComp->StopMovementImmediately();
        MovementComp->DisableMovement();
    }

    if (USkeletalMeshComponent* CharacterMesh = GetMesh())
    {
        CharacterMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        CharacterMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
    }

    if (UCapsuleComponent* Capsule = GetCapsuleComponent())
    {
        Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }

    UWorld* World = GetWorld();
    if (!IsValid(World))
    {
        return;
    }

    if (bIsBoss)
    {
        if (UUnitSubsystem* Subsystem = World->GetSubsystem<UUnitSubsystem>())
        {
            Subsystem->OnBossDeadDelegate.Broadcast(TeamTag);
        }
    }

    if (UUnitSpawnSubsystem* SpawnSubsystem = World->GetSubsystem<UUnitSpawnSubsystem>())
    {
        USkeletalMeshComponent* CharacterMesh = GetMesh();
        UAnimInstance* AnimInstance = IsValid(CharacterMesh) ? CharacterMesh->GetAnimInstance() : nullptr;

        if (IsValid(AnimInstance) && IsValid(UnitDeadMontage))
        {
            float Duration = AnimInstance->Montage_Play(UnitDeadMontage) - 0.2f;

            if (Duration > 0.f)
            {
                FTimerHandle TimerHandle;
                World->GetTimerManager().SetTimer(TimerHandle, [SpawnSubsystem, this]()
                    {
                        if (IsValid(this) && IsValid(SpawnSubsystem))
                        {
                            SpawnSubsystem->OnUnitDied(this);
                        }
                    }, Duration, false);
            }
            else
            {
                SpawnSubsystem->OnUnitDied(this);
            }
        }
        else
        {
            SpawnSubsystem->OnUnitDied(this);
        }
    }
    else
    {
        Destroy();
    }
}

void AUnitCharacter::ActivateUnit()
{
    bIsDead = false;
    SetActorHiddenInGame(false);
    SetActorLocation(GetActorLocation() + FVector(0.f, 0.f, 10.f), false, nullptr, ETeleportType::TeleportPhysics);
    SetActorEnableCollision(true);
    SetActorTickEnabled(true);

    if (UCapsuleComponent* Capsule = GetCapsuleComponent())
    {
        Capsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    }

    if (UCharacterMovementComponent* MovementComp = GetCharacterMovement())
    {
        MovementComp->Velocity = FVector::ZeroVector;
        MovementComp->ClearAccumulatedForces();
        MovementComp->SetMovementMode(MOVE_Walking);
    }

    if (Controller == nullptr && AIControllerClass)
    {
        SpawnDefaultController();
    }

    if (!AIController)
    {
        AIController = Cast<AAIController>(GetController());
    }

    if (AIController)
    {
        if (UBrainComponent* BrainComp = AIController->GetBrainComponent())
        {
            BrainComp->RestartLogic();
        }
    }

    if (USkeletalMeshComponent* CharacterMesh = GetMesh())
    {
        if (UAnimInstance* AnimInstance = CharacterMesh->GetAnimInstance())
        {
            AnimInstance->StopAllMontages(0.0f);
        }
    }

    if (TargetActor)
    {
        SetAttackTarget(TargetActor);
    }

    InitUnitStartUpData();
}

void AUnitCharacter::DeactivateUnit()
{
    bIsDead = true; // 상태 변경
    if (AIController)
    {
        if (UBrainComponent* BrainComp = AIController->GetBrainComponent())
        {
            BrainComp->StopLogic(TEXT("Unit Deactivated"));
        }
        AIController->StopMovement();
        AIController->ClearFocus(EAIFocusPriority::Gameplay);
    }

    // 2. 유닛 서브시스템 팀 해제
    if (UUnitSubsystem* Subsystem = GetWorld()->GetSubsystem<UUnitSubsystem>())
    {
        Subsystem->UnregisterUnit(this, TeamTag);
    }

    // 3. 물리/이동 초기화
    if (UCharacterMovementComponent* MovementComp = GetCharacterMovement())
    {
        MovementComp->StopMovementImmediately();
        MovementComp->DisableMovement(); // 바닥으로 꺼지거나 미끄러짐 방지
        MovementComp->Velocity = FVector::ZeroVector;
    }

    // 2) GAS 큐/태스크/게임플레이 큐 정리
    if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
    {
        ASC->CancelAllAbilities();   // WaitGameplayEvent/몽타주 등 AbilityTask 종료
        ASC->RemoveAllGameplayCues(); // 붙어있는 GameplayCue 일괄 제거
    }

    // 4. 시각적 숨김 및 충돌 해제
    SetActorEnableCollision(false);
    SetActorHiddenInGame(true);
    SetActorTickEnabled(false);
}


