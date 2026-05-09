# 헤더 기준 소스 구조

이 문서는 감자 원정대의 C++ 소스 구조를 헤더 파일 기준으로 정리한 문서입니다.
각 시스템의 인터페이스, 데이터 타입, 모듈 경계를 확인하기 쉽도록 `Public` 디렉토리를 중심으로 구성했습니다.

## 전체 구조

```txt
Source
+-- Unreal_ProjectG
|   +-- Unreal_ProjectG.h
|   |
|   +-- Public
|       +-- GA_InitializeHero.h
|       +-- PGFunctionLibrary.h
|       +-- PGGameplayTags.h
|       +-- TestActor.h
|       |
|       +-- AbilitySystem
|       |   +-- PGAbilitySystemComponent.h
|       |   +-- PGAbilitySystemGlobals.h
|       |   +-- PGCharacterAttributeSet.h
|       |   +-- Abilities
|       |   |   +-- Hero
|       |   |   +-- Player
|       |   |   +-- Shared
|       |   |   +-- Unit
|       |   +-- AbilityTasks
|       |   +-- Effects
|       |   +-- GEEC
|       |   +-- MMC
|       |   +-- TargetActor
|       |   +-- Types
|       |
|       +-- Actors
|       |   +-- SkillActor
|       |
|       +-- AnimInstance
|       |   +-- Hero
|       |
|       +-- Character
|       |   +-- Hero
|       |   +-- Pet
|       |   +-- Unit
|       |       +-- AI
|       |       +-- Component
|       |       +-- Decorator
|       |       +-- Service
|       |       +-- SubSystem
|       |       +-- Task
|       |
|       +-- Components
|       |   +-- Combat
|       |   +-- Equipment
|       |   +-- Resource
|       |   +-- Spawner
|       |   +-- Visual
|       |
|       +-- DataAssets
|       |   +-- Ability
|       |   +-- Items
|       |   +-- Spawner
|       |   +-- StartUp
|       |   +-- UI
|       |   +-- Unit
|       |
|       +-- Framework
|       +-- Interfaces
|       +-- Mode
|       |   +-- Save
|       +-- Notify
|       |   +-- AnimNotify
|       |   +-- AnimNotifyState
|       +-- Pawn
|       +-- Types
|       +-- UI
|           +-- Battle
|           +-- DataTable
|           +-- Lobby
|               +-- Equip
|               +-- Gacha
|               +-- Main
|               +-- Stage
|               +-- Unit
|
+-- PGEditor
    +-- PGEditor.h
    +-- Customization
        +-- AbilityEntryCustomization.h
        +-- AttributeModEntryCustomization.h
```

## 디렉토리 역할

| 디렉토리 | 역할 |
|---|---|
| `AbilitySystem` | GAS 기반 어빌리티, 어빌리티 태스크, GameplayEffect, 실행 계산식, Magnitude 계산식, 타겟 액터, 커스텀 EffectContext 타입을 관리합니다. |
| `Actors` | 투사체, 보호막, 마법 액터, 재사용 가능한 스킬 액터처럼 전투 중 생성되는 런타임 액터를 관리합니다. |
| `AnimInstance` | 공통 캐릭터 애니메이션 인스턴스와 히어로 전용 애니메이션 레이어를 관리합니다. |
| `Character` | 히어로, 펫, 유닛 캐릭터와 유닛 AI, Behavior Tree Task, Service, Decorator, 유닛 관련 Subsystem을 관리합니다. |
| `Components` | 전투, 장비, 자원, 스폰, 비주얼 이펙트, Pawn 확장 기능처럼 Actor에 부착되는 기능 단위를 관리합니다. |
| `DataAssets` | 스킬, 장비, 스테이지 스폰, 초기 어빌리티 부여, UI, 유닛 정보를 데이터 기반으로 관리합니다. |
| `Framework` | GAS 매크로처럼 프로젝트 전반에서 사용하는 공통 프레임워크 코드를 관리합니다. |
| `Interfaces` | 장비 저장소, 히어로 전투, 조이스틱 입력, 비주얼 이펙트 대상 등 시스템 간 연결 인터페이스를 관리합니다. |
| `Mode` | 게임 모드, 로비 모드, 세이브 게임, 게임 인스턴스, 컬렉션 Subsystem을 관리합니다. |
| `Notify` | 애니메이션 이벤트 전달과 충돌 토글을 위한 AnimNotify 및 AnimNotifyState를 관리합니다. |
| `Pawn` | 기지 구조물처럼 Pawn 계열의 게임플레이 객체를 관리합니다. |
| `Types` | 공통 Enum, DataTable 구조체, 어빌리티 설정 구조체, GAS 관련 타입을 관리합니다. |
| `UI` | 전투 UI, 로비 UI, UI 표시용 DataTable, 장비, 가챠, 스테이지, 유닛 UI를 관리합니다. |
| `PGEditor` | 데이터 작성 흐름을 개선하기 위한 에디터 전용 모듈과 Details 패널 커스터마이징을 관리합니다. |

## 기능별 구조

| 기능 | 주요 헤더 위치 |
|---|---|
| 병과별 유닛 AI | `Character/Unit`, `Character/Unit/Task`, `Character/Unit/Service`, `AbilitySystem/Abilities/Unit` |
| 장비 기반 스킬 시스템 | `Components/Equipment`, `DataAssets/Items`, `Interfaces` |
| 세트 효과 기반 스킬 업그레이드 | `DataAssets/Ability`, `Types`, `PGFunctionLibrary.h` |
| 행동 단위 기반 전투 구조 | `AbilitySystem/Abilities/Hero`, `AbilitySystem/AbilityTasks`, `Actors/SkillActor` |
| 데이터 드리븐 콘텐츠 | `DataAssets`, `UI/DataTable`, `Types` |
| GAS 공통 기능 확장 | `AbilitySystem`, `AbilitySystem/Effects`, `AbilitySystem/Types`, `PGFunctionLibrary.h` |
| 에디터 작업 흐름 개선 | `PGEditor`, `PGEditor/Customization` |

## 대표 헤더

| 영역 | 대표 헤더 |
|---|---|
| GAS 핵심 | `PGAbilitySystemComponent.h`, `PGAbilitySystemGlobals.h`, `PGCharacterAttributeSet.h` |
| GameplayAbility | `PGGameplayAbility.h`, `PGHeroGameplayAbility.h`, `PGUnitGameplayAbility.h`, `PGHeroSkillGameplayAbility.h` |
| 스킬 Task | `SkillAbilityTask.h`, `SkillAbilityTask_MeleeTrace.h`, `SkillAbilityTask_SpawnActor.h`, `SkillAbilityTask_Buff.h`, `SkillAbilityTask_Presentation.h` |
| 장비 데이터 | `DataAsset_WeaponData.h`, `DataAsset_ArmorData.h`, `DataAsset_AccessoryData.h`, `EquipmentsStorageComponent.h` |
| 스킬 데이터 | `DataAsset_HeroSkillData.h`, `DataAsset_SkillData.h`, `DataAsset_HeroSkillPresentation.h`, `DataAsset_SkillVisualData.h` |
| 유닛 AI | `UnitCharacter.h`, `UnitDetourCrowdAIController.h`, `BTT_Attack.h`, `BTT_FindAlly.h`, `BTT_SearchAttackArea.h`, `RangeCheck.h` |
| 런타임 스킬 액터 | `SkillActor.h`, `AOESkillActor.h`, `ProjectileSkillActor.h`, `BuffAuraActor.h` |
| 공통 타입 | `AbilityConfig.h`, `PGDataTableStruct.h`, `PGEnumTypes.h`, `PGGasTypes.h` |
| 에디터 커스터마이징 | `PGEditor.h`, `AbilityEntryCustomization.h`, `AttributeModEntryCustomization.h` |
