//-------------------------------------------------------------
//! @file   GamePrefab.cpp
//! @brief  ゲーム側ComponentのPrefab登録の実装
//-------------------------------------------------------------
#include <CombatAndroid/ECS/Utility/GamePrefab.hpp>

#include <CombatAndroid/ECS/Component/BehaviorTreeComponent.hpp>
#include <CombatAndroid/ECS/Component/DamageNumberComponent.hpp>
#include <CombatAndroid/ECS/Component/EnemyAnimationSetComponent.hpp>
#include <CombatAndroid/ECS/Component/EnemyAttackHitboxComponent.hpp>
#include <CombatAndroid/ECS/Component/EnemyComponent.hpp>
#include <CombatAndroid/ECS/Component/EnemyStressTestComponent.hpp>
#include <CombatAndroid/ECS/Component/ExpOrbComponent.hpp>
#include <CombatAndroid/ECS/Component/GameLogComponent.hpp>
#include <CombatAndroid/ECS/Component/PickupComponent.hpp>
#include <CombatAndroid/ECS/Component/ProjectileComponent.hpp>
#include <CombatAndroid/ECS/Component/ScreenFadeComponent.hpp>
#include <CombatAndroid/ECS/Component/TitleMenuComponent.hpp>
#include <CombatAndroid/ECS/Component/TitleStageComponent.hpp>
#include <CombatAndroid/ECS/Component/TutorialComponent.hpp>
#include <CombatAndroid/ECS/Component/WeaponComponent.hpp>
#include <CombatAndroid/ECS/Component/WeaponGripDebugComponent.hpp>
#include <CombatAndroid/ECS/Component/WeaponLevelDebugComponent.hpp>
#include <CombatAndroid/ECS/Serialization/BuiltIn/WorldAnchorComponentSerialization.hpp>
#include <CombatAndroid/ECS/Serialization/EnemyComponentSerialization.hpp>
#include <CombatAndroid/ECS/Serialization/TitleStageComponentSerialization.hpp>
#include <CombatAndroid/ECS/Serialization/WeaponComponentSerialization.hpp>
#include <CombatAndroid/ECS/Component/GrassFieldComponent.hpp>
#include <CombatAndroid/ECS/Component/GroundFollowComponent.hpp>
#include <CombatAndroid/ECS/Component/HealthComponent.hpp>
#include <CombatAndroid/ECS/Component/PlayerAnimationSetComponent.hpp>
#include <CombatAndroid/ECS/Component/PlayerComponent.hpp>
#include <CombatAndroid/ECS/Component/PlayerExperienceComponent.hpp>
#include <CombatAndroid/ECS/Component/PlayerSkillComponent.hpp>
#include <CombatAndroid/ECS/Component/TpsCameraComponent.hpp>
#include <CombatAndroid/ECS/Serialization/BuiltIn/AnimationPlayerComponentSerialization.hpp>
#include <CombatAndroid/ECS/Serialization/BuiltIn/CharacterControllerComponentSerialization.hpp>
#include <CombatAndroid/ECS/Serialization/BuiltIn/RimGlowComponentSerialization.hpp>
#include <CombatAndroid/ECS/Serialization/BuiltIn/SpringBoneComponentSerialization.hpp>
#include <CombatAndroid/ECS/Serialization/GrassFieldComponentSerialization.hpp>
#include <CombatAndroid/ECS/Serialization/GroundFollowComponentSerialization.hpp>
#include <CombatAndroid/ECS/Serialization/HealthComponentSerialization.hpp>
#include <CombatAndroid/ECS/Serialization/PlayerAnimationSetComponentSerialization.hpp>
#include <CombatAndroid/ECS/Serialization/PlayerComponentSerialization.hpp>
#include <CombatAndroid/ECS/Serialization/TpsCameraComponentSerialization.hpp>

#include <Tsukino/BuiltIn/ECS/Component/AnimationControllerComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/AnimationPlayerComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/CharacterControllerComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/RimGlowComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/SkeletonOutputComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/SpringBoneComponent.hpp>

#include <Tsukino/EngineIntegration/EngineContext.hpp>
#include <Tsukino/Engine/ECS/Prefab/PrefabFactory.hpp>

#include <string>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    namespace {
        constexpr const char* kPrefabRoot      = "CombatAndroid/Assets/Prefabs/";
        constexpr const char* kEnvironmentRoot = "CombatAndroid/Assets/Prefabs/Environment/";
    }    // namespace

    //-------------------------------------------------------------
    //! @brief  Prefab名からPrefabのフォルダのパスを得る
    //-------------------------------------------------------------
    std::string GetPrefabDirectory(std::string_view name) {
        return std::string(kPrefabRoot) + std::string(name);
    }

    //-------------------------------------------------------------
    //! @brief  Prefab名からエンティティを1つ生成する
    //-------------------------------------------------------------
    Tsukino::ECS::Entity InstantiatePrefab(Tsukino::ECS::Registry& registry, Tsukino::EngineIntegration::EngineContext& context,
                                           std::string_view name) {
        return context.prefabFactory->Instantiate(GetPrefabDirectory(name) + "/Prefab.json", registry);
    }

    //-------------------------------------------------------------
    //! @brief  環境（地面・光・空・フォグ・環境パーティクル・草）をPrefabから生成する
    //-------------------------------------------------------------
    void InstantiateEnvironment(Tsukino::ECS::Registry& registry, Tsukino::EngineIntegration::EngineContext& context, const char* sceneName) {
        const std::string sceneRoot = std::string(kEnvironmentRoot) + sceneName + "/";

        (void)context.prefabFactory->Instantiate(sceneRoot + "Ground/Prefab.json", registry);
        (void)context.prefabFactory->Instantiate(sceneRoot + "Sun/Prefab.json", registry);
        (void)context.prefabFactory->Instantiate(std::string(kEnvironmentRoot) + "Sky/Prefab.json", registry);
        (void)context.prefabFactory->Instantiate(sceneRoot + "Fog/Prefab.json", registry);
        (void)context.prefabFactory->Instantiate(sceneRoot + "Particles/Prefab.json", registry);
        (void)context.prefabFactory->Instantiate(sceneRoot + "Grass/Prefab.json", registry);
    }

    //-------------------------------------------------------------
    //! @brief  画面固定UI用の2DカメラをPrefabから生成する
    //-------------------------------------------------------------
    void InstantiateUiCamera2D(Tsukino::ECS::Registry& registry, Tsukino::EngineIntegration::EngineContext& context) {
        (void)context.prefabFactory->Instantiate(std::string(kEnvironmentRoot) + "UiCamera2D/Prefab.json", registry);
    }

    //-------------------------------------------------------------
    //! @brief  シリアライズ対応済みのゲーム側ComponentをPrefabFactoryに登録する
    //-------------------------------------------------------------
    void RegisterGameComponents(Tsukino::Engine::ECS::Prefab::PrefabFactory& factory) {
        //-------------------------------------------------------------
        // エンジンの組み込みComponentのうち、エンジン側が登録していないもの。
        // シリアライズ定義はゲーム側（Serialization/BuiltIn/）で持っている
        //-------------------------------------------------------------
        factory.RegisterComponent<Tsukino::BuiltIn::ECS::RimGlowComponent>("RimGlowComponent");
        factory.RegisterComponent<Tsukino::BuiltIn::ECS::CharacterControllerComponent>("CharacterControllerComponent");
        factory.RegisterComponent<Tsukino::BuiltIn::ECS::AnimationPlayerComponent>("AnimationPlayerComponent");
        factory.RegisterComponent<Tsukino::BuiltIn::ECS::SpringBoneComponent>("SpringBoneComponent");
        factory.RegisterComponent<Tsukino::BuiltIn::ECS::WorldAnchorComponent>("WorldAnchorComponent");

        // 保存する項目が無い（実行時状態だけの）ものは、Prefab JSONに "null" と書いてアタッチだけする
        factory.RegisterComponent<Tsukino::BuiltIn::ECS::AnimationControllerComponent>("AnimationControllerComponent");
        factory.RegisterComponent<Tsukino::BuiltIn::ECS::SkeletonOutputComponent>("SkeletonOutputComponent");

        //-------------------------------------------------------------
        // ゲーム側Component
        //-------------------------------------------------------------
        factory.RegisterComponent<GrassFieldComponent>("GrassFieldComponent");
        factory.RegisterComponent<GroundFollowComponent>("GroundFollowComponent");
        factory.RegisterComponent<TpsCameraComponent>("TpsCameraComponent");
        factory.RegisterComponent<PlayerComponent>("PlayerComponent");
        factory.RegisterComponent<HealthComponent>("HealthComponent");
        factory.RegisterComponent<PlayerAnimationSetComponent>("PlayerAnimationSetComponent");
        factory.RegisterComponent<EnemyComponent>("EnemyComponent");
        factory.RegisterComponent<EnemyAttackHitboxComponent>("EnemyAttackHitboxComponent");
        factory.RegisterComponent<EnemyAnimationSetComponent>("EnemyAnimationSetComponent");
        factory.RegisterComponent<WeaponComponent>("WeaponComponent");
        factory.RegisterComponent<PickupComponent>("PickupComponent");
        factory.RegisterComponent<TitleStageComponent>("TitleStageComponent");

        //-------------------------------------------------------------
        // 既定値で始まる／実行時状態だけのもの（アタッチのみ。中身は生成後にSystemやコードが書く）。
        // プレイヤーに後から付けるHUD・メニューの束（PlayerHud等）はエンティティを作らないので登録しない
        //-------------------------------------------------------------
        factory.RegisterComponent<PlayerExperienceComponent>("PlayerExperienceComponent");
        factory.RegisterComponent<PlayerSkillComponent>("PlayerSkillComponent");
        factory.RegisterComponent<BehaviorTreeComponent>("BehaviorTreeComponent");
        factory.RegisterComponent<ProjectileComponent>("ProjectileComponent");
        factory.RegisterComponent<DamageNumberComponent>("DamageNumberComponent");
        factory.RegisterComponent<ExpOrbComponent>("ExpOrbComponent");
        factory.RegisterComponent<GameLogComponent>("GameLogComponent");
        factory.RegisterComponent<ScreenFadeComponent>("ScreenFadeComponent");
        factory.RegisterComponent<TutorialComponent>("TutorialComponent");
        factory.RegisterComponent<TitleMenuComponent>("TitleMenuComponent");
        factory.RegisterComponent<WeaponGripDebugComponent>("WeaponGripDebugComponent");
        factory.RegisterComponent<WeaponLevelDebugComponent>("WeaponLevelDebugComponent");
        factory.RegisterComponent<EnemyStressTestComponent>("EnemyStressTestComponent");
    }
}    // namespace CombatAndroid::ECS
