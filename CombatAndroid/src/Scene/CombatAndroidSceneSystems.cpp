//-------------------------------------------------------------
//! @file    CombatAndroidSceneSystems.cpp
//! @brief   CombatAndroidSceneへのシステム登録
//! @detail  システムを1本足すときに触るのはこのファイルと SystemPriority.hpp だけです。
//!          シーンの構築本体（アセットのロードとエンティティ生成）は
//!          CombatAndroidScene.cpp に置いています。
//! @author  山﨑愛
//-------------------------------------------------------------
#include <CombatAndroid/Scene/CombatAndroidScene.hpp>

// 条件付きインクルードより先に読む必要がある（TSUKINO_ENABLE_STRESS_TEST等の定義元）
#include <Tsukino/Core/DebugTools/DebugFeatures.hpp>

#include <CombatAndroid/ECS/SystemPriority.hpp>

#include <CombatAndroid/ECS/System/PlayerSystem.hpp>
#include <CombatAndroid/ECS/System/CombatSystem.hpp>
#include <CombatAndroid/ECS/System/ProjectileSystem.hpp>
#include <CombatAndroid/ECS/System/AttackMotionBlurSystem.hpp>
#include <CombatAndroid/ECS/System/EnemyBehaviorSystem.hpp>
#include <CombatAndroid/ECS/System/EnemyAnimationSystem.hpp>
#include <CombatAndroid/ECS/System/HitStopSystem.hpp>
#include <CombatAndroid/ECS/System/TpsCameraSystem.hpp>
#include <CombatAndroid/ECS/System/PlayerAnimationSystem.hpp>
#include <CombatAndroid/ECS/System/PickupSystem.hpp>
#include <CombatAndroid/ECS/System/HealthBarSystem.hpp>
#include <CombatAndroid/ECS/System/DamageNumberSystem.hpp>
#include <CombatAndroid/ECS/System/EnemyWeaponDropSystem.hpp>
#include <CombatAndroid/ECS/System/ExpOrbSystem.hpp>
#include <CombatAndroid/ECS/System/PlayerHudSystem.hpp>
#include <CombatAndroid/ECS/System/PlayerSkillHudSystem.hpp>
#include <CombatAndroid/ECS/System/RunClockSystem.hpp>
#include <CombatAndroid/ECS/System/PlayerDamageEffectSystem.hpp>
#include <CombatAndroid/ECS/System/GameOverSystem.hpp>
#include <CombatAndroid/ECS/System/SkillSelectSystem.hpp>
#include <CombatAndroid/ECS/System/GameLogSystem.hpp>
#include <CombatAndroid/ECS/System/EnemySpawnDirectorSystem.hpp>
#ifdef _DEBUG
#include <CombatAndroid/ECS/System/WeaponGripDebugSystem.hpp>
#include <CombatAndroid/ECS/Component/WeaponGripDebugComponent.hpp>
#include <CombatAndroid/ECS/System/WeaponLevelDebugSystem.hpp>
#include <CombatAndroid/ECS/Component/WeaponLevelDebugComponent.hpp>
#endif
#ifdef TSUKINO_ENABLE_STRESS_TEST
#include <CombatAndroid/ECS/System/EnemyStressTestSystem.hpp>
#include <CombatAndroid/ECS/Component/EnemyStressTestComponent.hpp>
#endif

// 必要なシステムとコンポーネントのインクルード
#include <Tsukino/EngineIntegration/ECS/System/TransformSystem.hpp>
#include <Tsukino/EngineIntegration/ECS/System/CameraSystem.hpp>
#include <Tsukino/EngineIntegration/ECS/System/SpriteRendererSystem.hpp>
#include <Tsukino/EngineIntegration/ECS/System/FontRendererSystem.hpp>
#include <Tsukino/EngineIntegration/ECS/System/AudioSystem.hpp>
#include <Tsukino/EngineIntegration/ECS/System/PhysicsSystem.hpp>
#include <Tsukino/EngineIntegration/ECS/System/ModelSystem.hpp>
#include <Tsukino/EngineIntegration/ECS/System/AnimationSystem.hpp>
#include <Tsukino/EngineIntegration/ECS/System/LightSystem.hpp>
#include <Tsukino/EngineIntegration/ECS/System/SkyAtmosphereSystem.hpp>
#include <Tsukino/EngineIntegration/ECS/System/FogSystem.hpp>
#include <Tsukino/EngineIntegration/ECS/System/MotionBlurSystem.hpp>
#include <Tsukino/EngineIntegration/ECS/System/MotionVectorSnapshotSystem.hpp>
#include <Tsukino/EngineIntegration/ECS/System/DebugCameraSystem.hpp>
#include <Tsukino/EngineIntegration/ECS/System/EffectSystem.hpp>
#include <Tsukino/EngineIntegration/ECS/System/WorldAnchorSystem.hpp>

#include <Tsukino/EngineIntegration/EngineContext.hpp>
#include <Tsukino/Core/ECS/Event/EventBus.hpp>

#include <memory>
// 名前空間 : CombatAndroid
namespace CombatAndroid {
    //-------------------------------------------------------------
    //! シーンに全てのシステムを登録します。
    //-------------------------------------------------------------
    void CombatAndroidScene::RegisterSystems(Tsukino::EngineIntegration::EngineContext* context, Tsukino::ECS::EventBus& eventBus) {
        // 登録
        // ライトのスポーン/移動はTransformSystemより前に行う。そうしないと
        // 生成・移動したライトのworldMatrixが1フレーム遅れ、LightSystemが古い位置を読む
        // モーションブラー用の前フレーム退避は、TransformSystem/AnimationSystemが
        // 今フレームの値で上書きする前に読む必要があるので最初に登録する
        //
        // 走行の経過時間と危険度ランクは、それらを読む湧き潰しより先に進めておく
        m_scene.AddSystem(std::make_shared<CombatAndroid::ECS::RunClockSystem>(), (int)ECS::SystemPriority::RunClock);
        m_scene.AddSystem(std::make_shared<CombatAndroid::ECS::EnemySpawnDirectorSystem>(), (int)ECS::SystemPriority::EnemySpawn);
#ifdef TSUKINO_ENABLE_STRESS_TEST
        m_scene.AddSystem(std::make_shared<CombatAndroid::ECS::EnemyStressTestSystem>(), (int)ECS::SystemPriority::StressTest);
#endif
        m_scene.AddSystem(std::make_shared<Tsukino::BuiltIn::ECS::MotionVectorSnapshotSystem>(), (int)ECS::SystemPriority::MotionVectorSnapshot);
        m_scene.AddSystem(std::make_shared<Tsukino::BuiltIn::ECS::TransformSystem>(), (int)ECS::SystemPriority::Transform);
        // レベルアップ時のスキル選択。PlayerSystemが同じフレームの入力を消費する前に割り込む
        m_scene.AddSystem(std::make_shared<CombatAndroid::ECS::SkillSelectSystem>(), (int)ECS::SystemPriority::SkillSelect);
        m_scene.AddSystem(std::make_shared<CombatAndroid::ECS::PlayerSystem>(), (int)ECS::SystemPriority::Movement);
        // 敵は全てBehaviorTreeComponentを持つBT駆動（歩いて近づき、射程内で攻撃・被弾でノックバック・死亡演出）
        m_scene.AddSystem(std::make_shared<CombatAndroid::ECS::EnemyBehaviorSystem>(), (int)ECS::SystemPriority::Movement);
        m_scene.AddSystem(std::make_shared<CombatAndroid::ECS::PlayerAnimationSystem>(), (int)ECS::SystemPriority::Gameplay);
        // EnemyAnimationSystemが書いたAnimationControllerComponent::nextを同フレームでAnimationSystemが
        // 消費するため、PlayerAnimationSystemと同じくAnimationSystemより前に登録する
        m_scene.AddSystem(std::make_shared<CombatAndroid::ECS::EnemyAnimationSystem>(), (int)ECS::SystemPriority::Gameplay);
        // ヒットストップ（プレイヤー/被弾した敵だけを止める）。Player/EnemyAnimationSystemが
        // 今フレームのplayback_speedを確定させた後、AnimationSystemがそれを消費する前に
        // 対象エンティティだけ掛け算で減速させる
        m_scene.AddSystem(std::make_shared<CombatAndroid::ECS::HitStopSystem>(), (int)ECS::SystemPriority::Gameplay);
        m_scene.AddSystem(std::make_shared<Tsukino::BuiltIn::ECS::AnimationSystem>(), (int)ECS::SystemPriority::Gameplay);
        // カメラ行列を必要としなくなった（座標変換はWorldAnchorSystemが行う）ため、他のゲームプレイ系と同じ並びで良い
        m_scene.AddSystem(std::make_shared<CombatAndroid::ECS::PickupSystem>(), (int)ECS::SystemPriority::Gameplay);
#ifdef _DEBUG
        // 武器の握り位置・角度を実機で調整するためのデバッグ操作（F6で有効化）。
        // 詳細はWeaponGripDebugSystem.cppを参照
        m_scene.AddSystem(std::make_shared<CombatAndroid::ECS::WeaponGripDebugSystem>(), (int)ECS::SystemPriority::WeaponGripDebug);

        // 所持武器のレベルを表示する常時表示デバッグHUD。PickupSystem（Gameplay）が
        // その回のフレームのレベルアップを確定させた後に読めればよいので、同じ並びでよい
        m_scene.AddSystem(std::make_shared<CombatAndroid::ECS::WeaponLevelDebugSystem>(), (int)ECS::SystemPriority::WeaponGripDebug);
#endif
        m_scene.AddSystem(std::make_shared<CombatAndroid::ECS::CombatSystem>(), (int)ECS::SystemPriority::WeaponAttach);
        m_scene.AddSystem(std::make_shared<CombatAndroid::ECS::ProjectileSystem>(), (int)ECS::SystemPriority::Projectile);
        m_scene.AddSystem(std::make_shared<CombatAndroid::ECS::HealthBarSystem>(), (int)ECS::SystemPriority::HealthBar);
        {
            // WeaponHitEventを購読してダメージ数値を出す。購読解除はSystemが持つ
            // ScopedConnectionのデストラクタに任せる（EventBusはSystemManagerより長生きする）
            auto damageNumberSystem = std::make_shared<CombatAndroid::ECS::DamageNumberSystem>();
            m_scene.AddSystem(damageNumberSystem, (int)ECS::SystemPriority::DamageNumber);
            damageNumberSystem->Initialize(eventBus);
        }
        {
            // EnemyDiedEventを購読してEXP玉のドロップ演出を行う
            auto expOrbSystem = std::make_shared<CombatAndroid::ECS::ExpOrbSystem>();
            m_scene.AddSystem(expOrbSystem, (int)ECS::SystemPriority::ExpOrb);
            expOrbSystem->Initialize(eventBus);
        }
        {
            // EnemyDiedEventを購読して、敵が持っていた武器を拾える状態で地面へ落とす
            auto enemyWeaponDropSystem = std::make_shared<CombatAndroid::ECS::EnemyWeaponDropSystem>();
            m_scene.AddSystem(enemyWeaponDropSystem, (int)ECS::SystemPriority::EnemyWeaponDrop);
            enemyWeaponDropSystem->Initialize(eventBus);
        }
        m_scene.AddSystem(std::make_shared<CombatAndroid::ECS::PlayerHudSystem>(), (int)ECS::SystemPriority::PlayerHud);
        // EXPバーの下に並べる取得済みスキル一覧。取得段階はSkillSelectSystem（ECS::SystemPriority::SkillSelect）が
        // 同じフレームの手前で確定させているため、選んだ内容がその回のフレームから一覧へ載る
        m_scene.AddSystem(std::make_shared<CombatAndroid::ECS::PlayerSkillHudSystem>(), (int)ECS::SystemPriority::PlayerHud);
        {
            // GameLogEventを購読して画面右の取得ログを流す。発火元（PickupSystem=Gameplay、
            // ExpOrbSystem=ExpOrb、SkillSelectSystem=SkillSelect、RunClockSystem=RunClock）が
            // 全てここより手前に居るので同じフレームで拾えて表示が遅れず、
            // 位置を書き込むTransformUIより手前でもある
            auto gameLogSystem = std::make_shared<CombatAndroid::ECS::GameLogSystem>();
            m_scene.AddSystem(gameLogSystem, (int)ECS::SystemPriority::PlayerHud);
            gameLogSystem->Initialize(eventBus);
        }
        {
            // PlayerDamagedEventを購読して被弾演出（点滅・画面フラッシュ）を進行させる。
            // HP確定（WeaponAttachでCombatSystemがPublish）の後であればよいので、PlayerHudと同じ並びでよい
            auto playerDamageEffectSystem = std::make_shared<CombatAndroid::ECS::PlayerDamageEffectSystem>();
            m_scene.AddSystem(playerDamageEffectSystem, (int)ECS::SystemPriority::PlayerHud);
            playerDamageEffectSystem->Initialize(eventBus);
        }
        // 死亡演出からGAME OVER表示・リトライまでの進行。HP確定（isDead）の後であればよい
        m_scene.AddSystem(std::make_shared<CombatAndroid::ECS::GameOverSystem>(), (int)ECS::SystemPriority::PlayerHud);
        m_scene.AddSystem(std::make_shared<Tsukino::BuiltIn::ECS::TransformSystem>(), (int)ECS::SystemPriority::TransformLate);
        m_scene.AddSystem(std::make_shared<CombatAndroid::ECS::TpsCameraSystem>(), (int)ECS::SystemPriority::Camera3D);
#ifdef _DEBUG
        m_scene.AddSystem(std::make_shared<Tsukino::BuiltIn::ECS::DebugCameraSystem>(), (int)ECS::SystemPriority::Camera3D);
#endif
        m_scene.AddSystem(std::make_shared<Tsukino::BuiltIn::ECS::CameraSystem>(), (int)ECS::SystemPriority::Camera);
        m_scene.AddSystem(std::make_shared<Tsukino::BuiltIn::ECS::WorldAnchorSystem>(), (int)ECS::SystemPriority::WorldAnchor);
        m_scene.AddSystem(std::make_shared<Tsukino::BuiltIn::ECS::TransformSystem>(), (int)ECS::SystemPriority::TransformUI);
        m_scene.AddSystem(std::make_shared<Tsukino::BuiltIn::ECS::FontRendererSystem>(), (int)ECS::SystemPriority::Font);
        // 攻撃演出→ブラー強度→Rendererの順に流す
        m_scene.AddSystem(std::make_shared<CombatAndroid::ECS::AttackMotionBlurSystem>(), (int)ECS::SystemPriority::AttackMotionBlur);
        m_scene.AddSystem(std::make_shared<Tsukino::BuiltIn::ECS::MotionBlurSystem>(), (int)ECS::SystemPriority::MotionBlur);
        m_scene.AddSystem(std::make_shared<Tsukino::BuiltIn::ECS::SpriteRenderSystem>(), (int)ECS::SystemPriority::Render);
        m_scene.AddSystem(std::make_shared<Tsukino::BuiltIn::ECS::ModelSystem>(), (int)ECS::SystemPriority::Render);
        {
            auto effectSystem = std::make_shared<Tsukino::BuiltIn::ECS::EffectSystem>();
            m_scene.AddSystem(effectSystem, (int)ECS::SystemPriority::Render);
            effectSystem->Initialize(m_scene.GetRegistry(), eventBus);
            context->effectSystem = effectSystem.get();
        }
        m_scene.AddSystem(std::make_shared<Tsukino::BuiltIn::ECS::AudioSystem>(), (int)ECS::SystemPriority::Audio);
        {
            auto physicsSystem = std::make_shared<Tsukino::BuiltIn::ECS::PhysicsSystem>(eventBus);
#ifdef TSUKINO_DEBUG_COLLISION_DRAW
            // CombatAndroidでは常にコリジョンのワイヤーフレームを表示する（F5で従来通りOFFも可能）
            physicsSystem->SetDebugDrawEnabled(true);
#endif
            m_scene.AddSystem(physicsSystem, (int)ECS::SystemPriority::Physics);
            context->physicsSystem = physicsSystem.get();
        }
        m_scene.AddSystem(std::make_shared<Tsukino::BuiltIn::ECS::LightSystem>(), (int)ECS::SystemPriority::Light);
        m_scene.AddSystem(std::make_shared<Tsukino::BuiltIn::ECS::SkyAtmosphereSystem>(), (int)ECS::SystemPriority::SkyAtmosphere);
        m_scene.AddSystem(std::make_shared<Tsukino::BuiltIn::ECS::FogSystem>(), (int)ECS::SystemPriority::Fog);
    }
}    // namespace CombatAndroid
