# Header-Based Source Structure

This document summarizes the C++ source structure of Potato Expedition based on header files.
The `Public` directory is used as the main reference because it exposes each system's interfaces,
data types, and module boundaries.

## Overview

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

## Directory Roles

| Directory | Role |
|---|---|
| `AbilitySystem` | GAS-based abilities, ability tasks, gameplay effects, execution calculations, magnitude calculations, target actors, and custom effect context types. |
| `Actors` | Runtime combat actors such as projectiles, shields, magic actors, and reusable skill actors. |
| `AnimInstance` | Base character animation instances and hero-specific animation layers. |
| `Character` | Hero, pet, unit characters, unit AI helpers, behavior tree tasks, services, decorators, and unit-related subsystems. |
| `Components` | Actor components for combat, equipment, resources, spawning, visual effects, and pawn extension behavior. |
| `DataAssets` | Data-driven definitions for abilities, equipment, stage spawning, startup grants, UI data, and unit data. |
| `Framework` | Shared framework-level helpers such as GAS macros. |
| `Interfaces` | Interfaces for equipment storage, hero combat, joystick input, and visual effect targets. |
| `Mode` | Game mode, lobby mode, save game, game instance, and collection subsystems. |
| `Notify` | Animation notify and notify state classes for gameplay events and collision toggles. |
| `Pawn` | Pawn-level gameplay objects such as base structures. |
| `Types` | Shared enum types, data table structs, ability config structs, and GAS-related structs. |
| `UI` | Battle UI, lobby UI, data table rows for UI, equipment UI, gacha UI, stage UI, and unit UI. |
| `PGEditor` | Editor-only module and Details panel customizations for data authoring workflows. |

## Feature-Oriented Map

| Feature | Main Header Areas |
|---|---|
| Class-based unit AI | `Character/Unit`, `Character/Unit/Task`, `Character/Unit/Service`, `AbilitySystem/Abilities/Unit` |
| Equipment-based skill system | `Components/Equipment`, `DataAssets/Items`, `Interfaces` |
| Set-effect skill upgrades | `DataAssets/Ability`, `Types`, `PGFunctionLibrary.h` |
| Action-based combat structure | `AbilitySystem/Abilities/Hero`, `AbilitySystem/AbilityTasks`, `Actors/SkillActor` |
| Data-driven content | `DataAssets`, `UI/DataTable`, `Types` |
| GAS common extensions | `AbilitySystem`, `AbilitySystem/Effects`, `AbilitySystem/Types`, `PGFunctionLibrary.h` |
| Editor workflow improvements | `PGEditor`, `PGEditor/Customization` |

## Representative Headers

| Area | Representative Headers |
|---|---|
| GAS core | `PGAbilitySystemComponent.h`, `PGAbilitySystemGlobals.h`, `PGCharacterAttributeSet.h` |
| Gameplay abilities | `PGGameplayAbility.h`, `PGHeroGameplayAbility.h`, `PGUnitGameplayAbility.h`, `PGHeroSkillGameplayAbility.h` |
| Skill tasks | `SkillAbilityTask.h`, `SkillAbilityTask_MeleeTrace.h`, `SkillAbilityTask_SpawnActor.h`, `SkillAbilityTask_Buff.h`, `SkillAbilityTask_Presentation.h` |
| Equipment data | `DataAsset_WeaponData.h`, `DataAsset_ArmorData.h`, `DataAsset_AccessoryData.h`, `EquipmentsStorageComponent.h` |
| Skill data | `DataAsset_HeroSkillData.h`, `DataAsset_SkillData.h`, `DataAsset_HeroSkillPresentation.h`, `DataAsset_SkillVisualData.h` |
| Unit AI | `UnitCharacter.h`, `UnitDetourCrowdAIController.h`, `BTT_Attack.h`, `BTT_FindAlly.h`, `BTT_SearchAttackArea.h`, `RangeCheck.h` |
| Runtime skill actors | `SkillActor.h`, `AOESkillActor.h`, `ProjectileSkillActor.h`, `BuffAuraActor.h` |
| Shared types | `AbilityConfig.h`, `PGDataTableStruct.h`, `PGEnumTypes.h`, `PGGasTypes.h` |
| Editor customizations | `PGEditor.h`, `AbilityEntryCustomization.h`, `AttributeModEntryCustomization.h` |
