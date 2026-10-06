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

#include <CombatAndroid/ECS/System/Player/PlayerSystem.hpp>
#include <CombatAndroid/ECS/System/Combat/CombatSystem.hpp>
#include <CombatAndroid/ECS/System/Combat/ProjectileSystem.hpp>
#include <CombatAndroid/ECS/System/Effect/AttackMotionBlurSystem.hpp>
#include <CombatAndroid/ECS/System/Enemy/EnemyBehaviorSystem.hpp>
#include <CombatAndroid/ECS/System/Enemy/EnemyAnimationSystem.hpp>
#include <CombatAndroid/ECS/System/Enemy/EnemyAttackTelegraphSystem.hpp>
#include <CombatAndroid/ECS/System/Enemy/PaladinWeaponSwitchSystem.hpp>
#include <CombatAndroid/ECS/System/Combat/HitStopSystem.hpp>
#include <CombatAndroid/ECS/System/Menu/CutsceneSystem.hpp>
#include <CombatAndroid/ECS/System/World/TpsCameraSystem.hpp>
#include <CombatAndroid/ECS/System/Player/PlayerAnimationSystem.hpp>
#include <CombatAndroid/ECS/System/Weapon/PickupSystem.hpp>
#include <CombatAndroid/ECS/System/UI/HealthBarSystem.hpp>
#include <CombatAndroid/ECS/System/UI/DamageNumberSystem.hpp>
#include <CombatAndroid/ECS/System/Weapon/EnemyWeaponDropSystem.hpp>
#include <CombatAndroid/ECS/System/Progression/ExpOrbSystem.hpp>
#include <CombatAndroid/ECS/System/UI/PlayerHudSystem.hpp>
#include <CombatAndroid/ECS/System/Menu/ScreenFadeSystem.hpp>
#include <CombatAndroid/ECS/System/UI/InputPromptSystem.hpp>
#include <CombatAndroid/ECS/System/UI/TutorialSystem.hpp>
#include <CombatAndroid/ECS/System/UI/PlayerSkillHudSystem.hpp>
#include <CombatAndroid/ECS/System/Progression/RunClockSystem.hpp>
#include <CombatAndroid/ECS/System/Effect/PlayerDamageEffectSystem.hpp>
#include <CombatAndroid/ECS/System/Menu/PauseMenuSystem.hpp>
#include <CombatAndroid/ECS/System/Menu/RunResultSystem.hpp>
#include <CombatAndroid/ECS/System/Progression/SkillSelectSystem.hpp>
#include <CombatAndroid/ECS/System/UI/GameLogSystem.hpp>
#include <CombatAndroid/ECS/System/Enemy/EnemySpawnDirectorSystem.hpp>
#include <CombatAndroid/ECS/System/Audio/ChargeSoundSystem.hpp>
#include <CombatAndroid/ECS/System/Audio/GameSoundSystem.hpp>
#include <CombatAndroid/ECS/System/Audio/HitSoundSystem.hpp>
#include <CombatAndroid/ECS/System/Effect/HitImpactEffectSystem.hpp>
#include <CombatAndroid/ECS/System/UI/EliteIndicatorSystem.hpp>
#include <CombatAndroid/ECS/System/UI/PickupIndicatorSystem.hpp>
#include <CombatAndroid/ECS/System/World/GroundFollowSystem.hpp>
#include <CombatAndroid/ECS/System/World/GroundVisualSystem.hpp>
#include <CombatAndroid/ECS/System/World/FogFollowSystem.hpp>
#include <CombatAndroid/ECS/System/Enemy/EnemyAttackAreaSystem.hpp>
#ifdef _DEBUG
#include <CombatAndroid/ECS/System/Debug/WeaponGripDebugSystem.hpp>
#include <CombatAndroid/ECS/Component/Debug/WeaponGripDebugComponent.hpp>
#include <CombatAndroid/ECS/System/Debug/WeaponLevelDebugSystem.hpp>
#include <CombatAndroid/ECS/Component/Debug/WeaponLevelDebugComponent.hpp>
#endif
#ifdef TSUKINO_ENABLE_STRESS_TEST
#include <CombatAndroid/ECS/System/Debug/EnemyStressTestSystem.hpp>
#include <CombatAndroid/ECS/Component/Debug/EnemyStressTestComponent.hpp>
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
#include <Tsukino/EngineIntegration/ECS/System/InteractionSystem.hpp>
#include <Tsukino/EngineIntegration/ECS/System/AmbientParticleSystem.hpp>
#include <CombatAndroid/ECS/System/World/GrassFieldSystem.hpp>
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
        // 大技（PlayerFinisherEvent）のインパクトで世界の時間を遅くする。
        // システムではなくシーンが持つのは、倍率をシーンへ渡すdeltaTimeに掛けるため（OnUpdate参照）
        m_slowMotion.Initialize(eventBus);

        // 登録
        // ライトのスポーン/移動はTransformSystemより前に行う。そうしないと
        // 生成・移動したライトのworldMatrixが1フレーム遅れ、LightSystemが古い位置を読む
        // モーションブラー用の前フレーム退避は、TransformSystem/AnimationSystemが
        // 今フレームの値で上書きする前に読む必要があるので最初に登録する
        //
        // マウスの重なり・クリックは、それを読むメニュー（スキル選択・ポーズ・リザルト）より先に書いておく
        m_scene.AddSystem(std::make_shared<Tsukino::BuiltIn::ECS::InteractionSystem>(), (int)ECS::SystemPriority::PointerInput);

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
        // Escのポーズメニュー。スキル選択と同じく、PlayerSystemが同じフレームの入力を消費する前に割り込む
        m_scene.AddSystem(std::make_shared<CombatAndroid::ECS::PauseMenuSystem>(), (int)ECS::SystemPriority::PauseMenu);
        m_scene.AddSystem(std::make_shared<CombatAndroid::ECS::PlayerSystem>(), (int)ECS::SystemPriority::Movement);
        // 敵は全てBehaviorTreeComponentを持つBT駆動（歩いて近づき、射程内で攻撃・被弾でノックバック・死亡演出）
        m_scene.AddSystem(std::make_shared<CombatAndroid::ECS::EnemyBehaviorSystem>(), (int)ECS::SystemPriority::Movement);
        // 地面をプレイヤーへ追従させる。草原と同じく「見た目は無限に続く」を実現するため
        m_scene.AddSystem(std::make_shared<CombatAndroid::ECS::GroundFollowSystem>(), (int)ECS::SystemPriority::GroundFollow);
        m_scene.AddSystem(std::make_shared<CombatAndroid::ECS::PlayerAnimationSystem>(), (int)ECS::SystemPriority::Gameplay);
        // EnemyAnimationSystemが書いたAnimationControllerComponent::nextを同フレームでAnimationSystemが
        // 消費するため、PlayerAnimationSystemと同じくAnimationSystemより前に登録する
        m_scene.AddSystem(std::make_shared<CombatAndroid::ECS::EnemyAnimationSystem>(), (int)ECS::SystemPriority::Gameplay);
        // 攻撃の振りかぶりを赤いリムライトで見せる。EnemyAnimationSystemが今フレームの
        // ステートを確定させた後に読む必要があるので、必ずこの後に登録する
        m_scene.AddSystem(std::make_shared<CombatAndroid::ECS::EnemyAttackTelegraphSystem>(), (int)ECS::SystemPriority::Gameplay);
        // エリートのPaladinの持ち替え。攻撃ステートを抜けた瞬間を見るので、これもEnemyAnimationSystemの後
        m_scene.AddSystem(std::make_shared<CombatAndroid::ECS::PaladinWeaponSwitchSystem>(), (int)ECS::SystemPriority::Gameplay);
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
        // 場面の切り替わりを繋ぐ黒。TransformUIより前に板の位置を書く必要があるのでここに置く
        m_scene.AddSystem(std::make_shared<CombatAndroid::ECS::ScreenFadeSystem>(), (int)ECS::SystemPriority::PlayerHud);
        m_scene.AddSystem(std::make_shared<CombatAndroid::ECS::InputPromptSystem>(), (int)ECS::SystemPriority::InputPrompt);
        // タイトルから始めたときだけ出す操作の案内（TutorialComponentが無ければ何もしない）
        m_scene.AddSystem(std::make_shared<CombatAndroid::ECS::TutorialSystem>(), (int)ECS::SystemPriority::Tutorial);
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
        {
            // 走行の終わり（死亡・クリア）の判定からリザルト表示・リトライまで。
            // HP確定（isDead）と生存時間（RunClock）の後であればよい。撃破数はEnemyDiedEventで数える
            auto runResultSystem = std::make_shared<CombatAndroid::ECS::RunResultSystem>();
            m_scene.AddSystem(runResultSystem, (int)ECS::SystemPriority::PlayerHud);
            runResultSystem->Initialize(eventBus);
        }
        m_scene.AddSystem(std::make_shared<Tsukino::BuiltIn::ECS::TransformSystem>(), (int)ECS::SystemPriority::TransformLate);
        // カットシーンの演出カメラ。TpsCameraSystemより前に登録し、再生中はTransform/fovを
        // 直接書き切ってからTpsCameraSystemに「何もしない」判断をさせる
        m_scene.AddSystem(std::make_shared<CombatAndroid::ECS::CutsceneSystem>(), (int)ECS::SystemPriority::Cutscene);
        {
            // PlayerDamagedEventを購読して、被弾したらカメラを揺らす。
            // Publish元のCombatSystem（WeaponAttach）より後ろに居るので、被弾したフレームのうちに揺れ始める
            auto tpsCameraSystem = std::make_shared<CombatAndroid::ECS::TpsCameraSystem>();
            m_scene.AddSystem(tpsCameraSystem, (int)ECS::SystemPriority::Camera3D);
            tpsCameraSystem->Initialize(eventBus);
        }
#ifdef _DEBUG
        m_scene.AddSystem(std::make_shared<Tsukino::BuiltIn::ECS::DebugCameraSystem>(), (int)ECS::SystemPriority::Camera3D);
#endif
        m_scene.AddSystem(std::make_shared<Tsukino::BuiltIn::ECS::CameraSystem>(), (int)ECS::SystemPriority::Camera);
        m_scene.AddSystem(std::make_shared<Tsukino::BuiltIn::ECS::WorldAnchorSystem>(), (int)ECS::SystemPriority::WorldAnchor);
        // 画面外のエリートへ向く矢印
        m_scene.AddSystem(std::make_shared<CombatAndroid::ECS::EliteIndicatorSystem>(), (int)ECS::SystemPriority::EliteIndicator);
        // 画面外の拾得アイテム（落ちている武器）へ向く矢印
        m_scene.AddSystem(std::make_shared<CombatAndroid::ECS::PickupIndicatorSystem>(), (int)ECS::SystemPriority::PickupIndicator);
        m_scene.AddSystem(std::make_shared<Tsukino::BuiltIn::ECS::TransformSystem>(), (int)ECS::SystemPriority::TransformUI);
        m_scene.AddSystem(std::make_shared<Tsukino::BuiltIn::ECS::FontRendererSystem>(), (int)ECS::SystemPriority::Font);
        // 攻撃演出→ブラー強度→Rendererの順に流す
        m_scene.AddSystem(std::make_shared<CombatAndroid::ECS::AttackMotionBlurSystem>(), (int)ECS::SystemPriority::AttackMotionBlur);
        m_scene.AddSystem(std::make_shared<Tsukino::BuiltIn::ECS::MotionBlurSystem>(), (int)ECS::SystemPriority::MotionBlur);
        m_scene.AddSystem(std::make_shared<Tsukino::BuiltIn::ECS::SpriteRenderSystem>(), (int)ECS::SystemPriority::Render);
        m_scene.AddSystem(std::make_shared<Tsukino::BuiltIn::ECS::ModelSystem>(), (int)ECS::SystemPriority::Render);
        // 地面（GroundFollowComponentを持つエンティティ）へ土テクスチャの板を描く
        m_scene.AddSystem(std::make_shared<CombatAndroid::ECS::GroundVisualSystem>(), (int)ECS::SystemPriority::Render);
        // 敵の振りかぶり中、攻撃が当たる範囲を足元へ赤く描く。攻撃ステート（Gameplay）が
        // 確定した後に描画コマンドを積むだけなので、地面と同じRenderに置く
        m_scene.AddSystem(std::make_shared<CombatAndroid::ECS::EnemyAttackAreaSystem>(), (int)ECS::SystemPriority::Render);
        {
            auto effectSystem = std::make_shared<Tsukino::BuiltIn::ECS::EffectSystem>();
            m_scene.AddSystem(effectSystem, (int)ECS::SystemPriority::Render);
            effectSystem->Initialize(m_scene.GetRegistry(), eventBus);
            context->effectSystem = effectSystem.get();
        }
        m_scene.AddSystem(std::make_shared<Tsukino::BuiltIn::ECS::AudioSystem>(), (int)ECS::SystemPriority::Audio);
        {
            // WeaponHitEventを購読してプロシージャル生成のヒット音を鳴らす
            auto hitSoundSystem = std::make_shared<CombatAndroid::ECS::HitSoundSystem>();
            m_scene.AddSystem(hitSoundSystem, (int)ECS::SystemPriority::Audio);
            hitSoundSystem->Initialize(eventBus);
        }
        {
            // WeaponHitEventを購読してヒット位置へ命中エフェクトを再生する
            auto hitImpactEffectSystem = std::make_shared<CombatAndroid::ECS::HitImpactEffectSystem>();
            m_scene.AddSystem(hitImpactEffectSystem, (int)ECS::SystemPriority::Audio);
            hitImpactEffectSystem->Initialize(eventBus);
        }
        {
            // 被弾・撃破・取得・メニュー操作など、ゲーム全体の効果音
            auto gameSoundSystem = std::make_shared<CombatAndroid::ECS::GameSoundSystem>();
            m_scene.AddSystem(gameSoundSystem, (int)ECS::SystemPriority::Audio);
            gameSoundSystem->Initialize(eventBus);
        }
        {
            // 溜め攻撃を解放して斬撃弾が飛び出す瞬間に「ドン」を鳴らす
            auto chargeSoundSystem = std::make_shared<CombatAndroid::ECS::ChargeSoundSystem>();
            m_scene.AddSystem(chargeSoundSystem, (int)ECS::SystemPriority::Audio);
            chargeSoundSystem->Initialize(eventBus);
        }
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
        // フォグの距離基準点をプレイヤー位置へ書き込む（TPSカメラの旋回でフォグの入り方が変わらないように）
        m_scene.AddSystem(std::make_shared<CombatAndroid::ECS::FogFollowSystem>(), (int)ECS::SystemPriority::FogFollow);
        m_scene.AddSystem(std::make_shared<Tsukino::BuiltIn::ECS::FogSystem>(), (int)ECS::SystemPriority::Fog);
        m_scene.AddSystem(std::make_shared<Tsukino::BuiltIn::ECS::AmbientParticleSystem>(), (int)ECS::SystemPriority::AmbientParticle);
        m_scene.AddSystem(std::make_shared<CombatAndroid::ECS::GrassFieldSystem>(), (int)ECS::SystemPriority::GrassField);
    }
}    // namespace CombatAndroid
