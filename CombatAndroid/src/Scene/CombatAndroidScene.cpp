//-------------------------------------------------------------
//! @file    CombatAndroidScene.cpp
//! @brief   CombatAndroidのメインゲームシーンの実装
//! @author  山﨑愛
//-------------------------------------------------------------
#include <CombatAndroid/Scene/CombatAndroidScene.hpp>

// 条件付きインクルードより先に読む必要がある（TSUKINO_ENABLE_STRESS_TEST等の定義元）
#include <Tsukino/Core/DebugTools/DebugFeatures.hpp>

#include <CombatAndroid/ECS/Component/PlayerComponent.hpp>
#include <CombatAndroid/ECS/Component/HealthComponent.hpp>
#include <CombatAndroid/ECS/Component/WeaponComponent.hpp>
#include <CombatAndroid/ECS/Component/EnemyComponent.hpp>
#include <CombatAndroid/ECS/Component/TpsCameraComponent.hpp>
#include <CombatAndroid/ECS/Component/PlayerAnimationSetComponent.hpp>
#include <CombatAndroid/ECS/Component/PickupComponent.hpp>
#include <CombatAndroid/ECS/Component/PickupPromptComponent.hpp>
#include <CombatAndroid/ECS/Component/DamageNumberComponent.hpp>
#include <CombatAndroid/ECS/Component/BehaviorTreeComponent.hpp>
#include <CombatAndroid/ECS/Component/EnemyAnimationSetComponent.hpp>
#include <CombatAndroid/ECS/Component/EnemyAttackHitboxComponent.hpp>
#include <CombatAndroid/ECS/Component/ExpOrbComponent.hpp>
#include <CombatAndroid/ECS/Component/PlayerExperienceComponent.hpp>
#include <CombatAndroid/ECS/Component/PlayerHudComponent.hpp>
#include <CombatAndroid/ECS/Component/RunClockComponent.hpp>
#include <CombatAndroid/ECS/Component/PlayerDamageEffectComponent.hpp>
#include <CombatAndroid/ECS/Component/GameOverComponent.hpp>
#include <CombatAndroid/ECS/Component/PlayerSkillComponent.hpp>
#include <CombatAndroid/ECS/Component/PlayerSkillHudComponent.hpp>
#include <CombatAndroid/ECS/Component/SkillSelectComponent.hpp>
#include <CombatAndroid/ECS/Component/GameLogComponent.hpp>
#include <CombatAndroid/UI/UiSortOrder.hpp>
#include <CombatAndroid/ECS/AI/ZombieBehavior.hpp>
#include <CombatAndroid/ECS/Utility/EnemySpawner.hpp>
#include <CombatAndroid/ECS/Utility/WeaponSpawner.hpp>
#include <CombatAndroid/ECS/System/PlayerSystem.hpp>
#include <CombatAndroid/ECS/System/CombatSystem.hpp>
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

#include <Tsukino/EngineIntegration/EngineAPI.hpp>
#include <Tsukino/EngineIntegration/EngineContext.hpp>
#include <Tsukino/Engine/Asset/AssetManager.hpp>

#include <Tsukino/Core/Path.hpp>
#include <Tsukino/Core/Log.hpp>
#include <Tsukino/Core/Window.hpp>

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

#include <Tsukino/BuiltIn/ECS/Component/TransformComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/CameraComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/SpriteComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/WorldAnchorComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/FontComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/AudioComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/ModelComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/AnimationPlayerComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/SkeletonOutputComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/CollisionComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/RigidBodyComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/CharacterControllerComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/AnimationControllerComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/DirectionalLightComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/PointLightComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/SpotLightComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/SkyAtmosphereComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/FogComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/MotionBlurComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/SpringBoneComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/DebugCameraComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/DebugCameraTag.hpp>
#include <Tsukino/BuiltIn/ECS/Component/EffectComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/HighlightComponent.hpp>

#include <entt/entt.hpp>
#include <hlsl++.h>
// 名前空間 : CombatAndroid
namespace CombatAndroid {
    //-------------------------------------------------------------
    //! @brief  シーン固有の初期化処理
    //-------------------------------------------------------------
    void CombatAndroidScene::OnInitialize(Tsukino::EngineIntegration::EngineAPI& api) {
        //-------------------------------------------------------------
        // コンテキストをレジストリから取得
        //-------------------------------------------------------------
        Tsukino::EngineIntegration::EngineContext* context = m_scene.GetRegistry().GetContext<Tsukino::EngineIntegration::EngineContext*>();
        //-------------------------------------------------------------
        // イベントバスをレジストリから取得
        //-------------------------------------------------------------
        Tsukino::ECS::EventBus& eventBus = m_scene.GetEventBus();

        //--------------------------------------------------------------
        // クリアカラーを透明に設定
        //--------------------------------------------------------------
        context->renderer->SetClearColor(0.0f, 0.0f, 0.0f, 0.0f);

        //--------------------------------------------------------------
        // システムの生成と追加
        //--------------------------------------------------------------
        enum class SystemPriority : int {
            RunClock = -5,                // 走行の経過時間と危険度ランクを進める。全システムの先頭に置く。
                                          // EnemySpawnが同じフレームでこの危険度を読んで敵を強化し、
                                          // PlayerHudが同じ値を表示するため、ここが最初に進んでいないと
                                          // 「画面に出ている危険度」と「実際に湧いた敵の危険度」が1フレームずれる
            EnemySpawn = -4,               // サバイバーの湧き潰し（フォグの外から比重抽選で敵を湧かせる）。
                                          // 生成・破棄はStressTestと同じく全システムの先頭で済ませ、その回の
                                          // フレームからTransform/Animation/Physicsが新しい敵を正しく扱えるように
                                          // する。StressTestより手前に置くのは、F1で負荷試験が数を作り直す前に
                                          // 本Systemの生存数の数え上げを終わらせておくため
            StressTest = -3,              // （負荷試験ビルドのみ）敵の大量スポーン。生成・破棄を
                                          // 全システムの先頭で済ませることで、その回のフレームから
                                          // Transform/Animation/Physicsが新しい敵を正しく扱える
            MotionVectorSnapshot = -2,    // モーションブラー用に前フレームのworldMatrix/ボーン行列を退避する。
                                          // TransformSystem・AnimationSystemが今フレームの値を書く「前」に読むことで、
                                          // ダブルバッファなしに前フレームの値を取り出している。ここより後ろへ動かすと
                                          // 速度が常にゼロになりブラーが効かなくなる
            Transform = 0,    // 一番最初に計算する
            SkillSelect,      // レベルアップ時のスキル選択メニュー。PlayerSystem（Movement）が
                              // 同じフレームのスペース（回避）・ホイール（武器切替）・左クリック（攻撃）を
                              // 消費する前に割り込んで入力を横取りする必要があるため、Movementより前に置く。
                              // 併せてメニュー表示中はCharacterControllerComponent::moveInputを毎フレーム潰す：
                              // PhysicsSystemはdeltaTimeが0以下でも1/60秒ぶん必ずステップするため、
                              // シーン側でdeltaTime=0にするだけではキャラクタが滑り続けてしまう
            Movement,         // プレイヤー入力・敵AIの移動をTransformの後、Physicsの前に反映する
            Gameplay,         // ダメージ処理・アニメーション更新は移動確定後に行う
            WeaponGripDebug,  // （デバッグビルドのみ）握り位置調整はisAttackingを上書きするため、
                              // PlayerAnimationSystem（Gameplay）が今フレームのisAttackingを確定させた後、
                              // CombatSystem（WeaponAttach）がそれを読む前に割り込む
            WeaponAttach,     // 武器の追従（ボーンアタッチ）はAnimationSystemが今フレームのボーン姿勢を書き込んだ後に行う
            HealthBar,        // 頭上HPバーの表示可否・残量幅の更新。被弾（WeaponAttachでCombatSystemが
                              // hpBarVisibleTimerをセット）の後、WorldAnchorSystemが座標を確定させる前に行う
            DamageNumber,     // ダメージ数値のスロット割り当てとアニメーション更新。被弾（WeaponAttachで
                              // CombatSystemがWeaponHitEventをPublish）の後でなければ表示が1フレーム遅れる。
                              // またWorldAnchorSystemが座標を確定させ、TransformUIがworldMatrixへ焼き込む前に
                              // fixedWorldPosition/screenOffset/scaleを書き終えている必要がある
                              // （さもないとポップの拡大率が1フレーム古い値で描かれてガタつく）
            ExpOrb,           // EXP玉のスロット割り当てと落下→ホーミング→吸収の状態更新。敵の死亡通知
                              // （EnemyDiedEvent、Movementで動くBTから発火）の後、かつプレイヤーの今フレームの
                              // 位置（Movementで確定済み）を吸い寄せ先に使うため、DamageNumberと同じ並びでよい
            EnemyWeaponDrop,    // 敵が持っていた武器を地面へ落とす。EXP玉と同じくEnemyDiedEventを
                                // 購読するだけなので、ExpOrbと同じ並びでよい
            PlayerHud,        // 画面左上のHP/EXPバー更新。HP（WeaponAttachでCombatSystemが確定）とEXP
                              // （ExpOrbが確定）の両方より後に置く
            TransformLate,    // Movement/WeaponAttachで更新したposition/rotationをworldMatrixへ反映する2回目のTransformSystem。
                              // これが無いと、このフレームで更新された所有者の回転がworldMatrix（描画に使われる）へ
                              // 反映されるのは次フレームになり、武器はowner.rotationを直接読むため1フレーム分
                              // 先行してしまい、回転中（旋回中）だけ武器の位置が体からずれて見える不具合が起きる
            Camera3D,         // TPS/デバッグカメラの追従は移動確定後、カメラ行列計算の前に行う
            Camera,           // カメラ行列は描画前に計算する
            WorldAnchor,      // WorldAnchorComponentを持つエンティティ（拾得プロンプト・HPバー等）の
                              // ワールド→スクリーン座標変換は今フレームのカメラ行列を使うためCameraの後に置く
            TransformUI,      // WorldAnchorSystemが書いたUI要素のposition（画面ピクセル座標）をworldMatrixへ反映する。
                              // FontRendererSystemはworldMatrix[3]を読むため、これが無いと1フレーム遅れて表示がスウィムする
            AttackMotionBlur,    // 攻撃の進行度（CombatSystemが更新するattackBlend）をブラー強度へ反映する。
                                 // WeaponAttachより後、MotionBlurより前
            MotionBlur,          // ブラーパラメータをRendererへ転送し、MotionVectorComponentを自動アタッチする。
                                 // ModelSystem（Render）が描画コマンドを積む前である必要がある
            Render,
            Font,    // 文字の描画コマンドを積む。SpriteRenderSystem（Render）との前後関係は
                     // もう登録順では決まらない：RendererがOverlayパスを DrawCommand::sortOrder で
                     // 並べ替えるため、スプライトと文字の重なりは
                     // CombatAndroid/UI/UiSortOrder.hpp の層の値だけで決まる。
                     // ここがRenderの後ろにあるのは同じ層同士の並びが積んだ順で決まる名残であり、
                     // 正しさの条件ではない。一方でworldMatrixを書くTransformUIより後、という
                     // 条件は引き続き必須（FontRendererSystemはworldMatrix[3]から描画位置を読む）
            Audio,
            Physics,    // コリジョンの更新は最後に行う
            Light,      // ディレクショナル/点光源/スポットをまとめてRendererへ渡す。
                        // worldMatrixから位置を取るのでTransform系より後である必要がある
            SkyAtmosphere,
            Fog,    // フォグはカメラ位置と太陽方向を使うため、Camera / Light より後に置く
        };

        // 登録
        // ライトのスポーン/移動はTransformSystemより前に行う。そうしないと
        // 生成・移動したライトのworldMatrixが1フレーム遅れ、LightSystemが古い位置を読む
        // モーションブラー用の前フレーム退避は、TransformSystem/AnimationSystemが
        // 今フレームの値で上書きする前に読む必要があるので最初に登録する
        //
        // 走行の経過時間と危険度ランクは、それらを読む湧き潰しより先に進めておく
        m_scene.AddSystem(std::make_shared<CombatAndroid::ECS::RunClockSystem>(), (int)SystemPriority::RunClock);
        m_scene.AddSystem(std::make_shared<CombatAndroid::ECS::EnemySpawnDirectorSystem>(), (int)SystemPriority::EnemySpawn);
#ifdef TSUKINO_ENABLE_STRESS_TEST
        m_scene.AddSystem(std::make_shared<CombatAndroid::ECS::EnemyStressTestSystem>(), (int)SystemPriority::StressTest);
#endif
        m_scene.AddSystem(std::make_shared<Tsukino::BuiltIn::ECS::MotionVectorSnapshotSystem>(), (int)SystemPriority::MotionVectorSnapshot);
        m_scene.AddSystem(std::make_shared<Tsukino::BuiltIn::ECS::TransformSystem>(), (int)SystemPriority::Transform);
        // レベルアップ時のスキル選択。PlayerSystemが同じフレームの入力を消費する前に割り込む
        m_scene.AddSystem(std::make_shared<CombatAndroid::ECS::SkillSelectSystem>(), (int)SystemPriority::SkillSelect);
        m_scene.AddSystem(std::make_shared<CombatAndroid::ECS::PlayerSystem>(), (int)SystemPriority::Movement);
        // 敵は全てBehaviorTreeComponentを持つBT駆動（歩いて近づき、射程内で攻撃・被弾でノックバック・死亡演出）
        m_scene.AddSystem(std::make_shared<CombatAndroid::ECS::EnemyBehaviorSystem>(), (int)SystemPriority::Movement);
        m_scene.AddSystem(std::make_shared<CombatAndroid::ECS::PlayerAnimationSystem>(), (int)SystemPriority::Gameplay);
        // EnemyAnimationSystemが書いたAnimationControllerComponent::nextを同フレームでAnimationSystemが
        // 消費するため、PlayerAnimationSystemと同じくAnimationSystemより前に登録する
        m_scene.AddSystem(std::make_shared<CombatAndroid::ECS::EnemyAnimationSystem>(), (int)SystemPriority::Gameplay);
        // ヒットストップ（プレイヤー/被弾した敵だけを止める）。Player/EnemyAnimationSystemが
        // 今フレームのplayback_speedを確定させた後、AnimationSystemがそれを消費する前に
        // 対象エンティティだけ掛け算で減速させる
        m_scene.AddSystem(std::make_shared<CombatAndroid::ECS::HitStopSystem>(), (int)SystemPriority::Gameplay);
        m_scene.AddSystem(std::make_shared<Tsukino::BuiltIn::ECS::AnimationSystem>(), (int)SystemPriority::Gameplay);
        // カメラ行列を必要としなくなった（座標変換はWorldAnchorSystemが行う）ため、他のゲームプレイ系と同じ並びで良い
        m_scene.AddSystem(std::make_shared<CombatAndroid::ECS::PickupSystem>(), (int)SystemPriority::Gameplay);
#ifdef _DEBUG
        // 武器の握り位置・角度を実機で調整するためのデバッグ操作（F6で有効化）。
        // 詳細はWeaponGripDebugSystem.cppを参照
        m_scene.AddSystem(std::make_shared<CombatAndroid::ECS::WeaponGripDebugSystem>(), (int)SystemPriority::WeaponGripDebug);

        // 所持武器のレベルを表示する常時表示デバッグHUD。PickupSystem（Gameplay）が
        // その回のフレームのレベルアップを確定させた後に読めればよいので、同じ並びでよい
        m_scene.AddSystem(std::make_shared<CombatAndroid::ECS::WeaponLevelDebugSystem>(), (int)SystemPriority::WeaponGripDebug);
#endif
        m_scene.AddSystem(std::make_shared<CombatAndroid::ECS::CombatSystem>(), (int)SystemPriority::WeaponAttach);
        m_scene.AddSystem(std::make_shared<CombatAndroid::ECS::HealthBarSystem>(), (int)SystemPriority::HealthBar);
        {
            // WeaponHitEventを購読してダメージ数値を出す。購読解除はSystemが持つ
            // ScopedConnectionのデストラクタに任せる（EventBusはSystemManagerより長生きする）
            auto damageNumberSystem = std::make_shared<CombatAndroid::ECS::DamageNumberSystem>();
            m_scene.AddSystem(damageNumberSystem, (int)SystemPriority::DamageNumber);
            damageNumberSystem->Initialize(eventBus);
        }
        {
            // EnemyDiedEventを購読してEXP玉のドロップ演出を行う
            auto expOrbSystem = std::make_shared<CombatAndroid::ECS::ExpOrbSystem>();
            m_scene.AddSystem(expOrbSystem, (int)SystemPriority::ExpOrb);
            expOrbSystem->Initialize(eventBus);
        }
        {
            // EnemyDiedEventを購読して、敵が持っていた武器を拾える状態で地面へ落とす
            auto enemyWeaponDropSystem = std::make_shared<CombatAndroid::ECS::EnemyWeaponDropSystem>();
            m_scene.AddSystem(enemyWeaponDropSystem, (int)SystemPriority::EnemyWeaponDrop);
            enemyWeaponDropSystem->Initialize(eventBus);
        }
        m_scene.AddSystem(std::make_shared<CombatAndroid::ECS::PlayerHudSystem>(), (int)SystemPriority::PlayerHud);
        // EXPバーの下に並べる取得済みスキル一覧。取得段階はSkillSelectSystem（SystemPriority::SkillSelect）が
        // 同じフレームの手前で確定させているため、選んだ内容がその回のフレームから一覧へ載る
        m_scene.AddSystem(std::make_shared<CombatAndroid::ECS::PlayerSkillHudSystem>(), (int)SystemPriority::PlayerHud);
        {
            // GameLogEventを購読して画面右の取得ログを流す。発火元（PickupSystem=Gameplay、
            // ExpOrbSystem=ExpOrb、SkillSelectSystem=SkillSelect、RunClockSystem=RunClock）が
            // 全てここより手前に居るので同じフレームで拾えて表示が遅れず、
            // 位置を書き込むTransformUIより手前でもある
            auto gameLogSystem = std::make_shared<CombatAndroid::ECS::GameLogSystem>();
            m_scene.AddSystem(gameLogSystem, (int)SystemPriority::PlayerHud);
            gameLogSystem->Initialize(eventBus);
        }
        {
            // PlayerDamagedEventを購読して被弾演出（点滅・画面フラッシュ）を進行させる。
            // HP確定（WeaponAttachでCombatSystemがPublish）の後であればよいので、PlayerHudと同じ並びでよい
            auto playerDamageEffectSystem = std::make_shared<CombatAndroid::ECS::PlayerDamageEffectSystem>();
            m_scene.AddSystem(playerDamageEffectSystem, (int)SystemPriority::PlayerHud);
            playerDamageEffectSystem->Initialize(eventBus);
        }
        // 死亡演出からGAME OVER表示・リトライまでの進行。HP確定（isDead）の後であればよい
        m_scene.AddSystem(std::make_shared<CombatAndroid::ECS::GameOverSystem>(), (int)SystemPriority::PlayerHud);
        m_scene.AddSystem(std::make_shared<Tsukino::BuiltIn::ECS::TransformSystem>(), (int)SystemPriority::TransformLate);
        m_scene.AddSystem(std::make_shared<CombatAndroid::ECS::TpsCameraSystem>(), (int)SystemPriority::Camera3D);
#ifdef _DEBUG
        m_scene.AddSystem(std::make_shared<Tsukino::BuiltIn::ECS::DebugCameraSystem>(), (int)SystemPriority::Camera3D);
#endif
        m_scene.AddSystem(std::make_shared<Tsukino::BuiltIn::ECS::CameraSystem>(), (int)SystemPriority::Camera);
        m_scene.AddSystem(std::make_shared<Tsukino::BuiltIn::ECS::WorldAnchorSystem>(), (int)SystemPriority::WorldAnchor);
        m_scene.AddSystem(std::make_shared<Tsukino::BuiltIn::ECS::TransformSystem>(), (int)SystemPriority::TransformUI);
        m_scene.AddSystem(std::make_shared<Tsukino::BuiltIn::ECS::FontRendererSystem>(), (int)SystemPriority::Font);
        // 攻撃演出→ブラー強度→Rendererの順に流す
        m_scene.AddSystem(std::make_shared<CombatAndroid::ECS::AttackMotionBlurSystem>(), (int)SystemPriority::AttackMotionBlur);
        m_scene.AddSystem(std::make_shared<Tsukino::BuiltIn::ECS::MotionBlurSystem>(), (int)SystemPriority::MotionBlur);
        m_scene.AddSystem(std::make_shared<Tsukino::BuiltIn::ECS::SpriteRenderSystem>(), (int)SystemPriority::Render);
        m_scene.AddSystem(std::make_shared<Tsukino::BuiltIn::ECS::ModelSystem>(), (int)SystemPriority::Render);
        {
            auto effectSystem = std::make_shared<Tsukino::BuiltIn::ECS::EffectSystem>();
            m_scene.AddSystem(effectSystem, (int)SystemPriority::Render);
            effectSystem->Initialize(m_scene.GetRegistry(), eventBus);
            context->effectSystem = effectSystem.get();
        }
        m_scene.AddSystem(std::make_shared<Tsukino::BuiltIn::ECS::AudioSystem>(), (int)SystemPriority::Audio);
        {
            auto physicsSystem = std::make_shared<Tsukino::BuiltIn::ECS::PhysicsSystem>(eventBus);
#ifdef TSUKINO_DEBUG_COLLISION_DRAW
            // CombatAndroidでは常にコリジョンのワイヤーフレームを表示する（F5で従来通りOFFも可能）
            physicsSystem->SetDebugDrawEnabled(true);
#endif
            m_scene.AddSystem(physicsSystem, (int)SystemPriority::Physics);
            context->physicsSystem = physicsSystem.get();
        }
        m_scene.AddSystem(std::make_shared<Tsukino::BuiltIn::ECS::LightSystem>(), (int)SystemPriority::Light);
        m_scene.AddSystem(std::make_shared<Tsukino::BuiltIn::ECS::SkyAtmosphereSystem>(), (int)SystemPriority::SkyAtmosphere);
        m_scene.AddSystem(std::make_shared<Tsukino::BuiltIn::ECS::FogSystem>(), (int)SystemPriority::Fog);

        //--------------------------------------------------------------
        // アセットのロード
        //--------------------------------------------------------------

        Tsukino::Asset::AssetHandle modelHandle =
            context->assetManager->Load(Tsukino::Core::Path("CombatAndroid/Assets/Models/Player.fbx"));

        // プレイヤーのアニメーションステートマシン（PlayerAnimationSystem）が使うクリップ。
        // 他キャラ（BigZombie/SmallZombie）と同じくAssets/Anims/Player/以下にまとめてある
        Tsukino::Asset::AssetHandle idleAnimHandle = context->assetManager->Load(Tsukino::Core::Path("CombatAndroid/Assets/Anims/Player/Idle.fbx"));
        Tsukino::Asset::AssetHandle runAnimHandle  = context->assetManager->Load(Tsukino::Core::Path("CombatAndroid/Assets/Anims/Player/Run.fbx"));
        Tsukino::Asset::AssetHandle fastRunAnimHandle =
            context->assetManager->Load(Tsukino::Core::Path("CombatAndroid/Assets/Anims/Player/Fast Run.fbx"));
        // 回避（前転）。クリップのルート前進はin_placeで殺し、移動はCharacterControllerが担当する
        Tsukino::Asset::AssetHandle dodgeAnimHandle =
            context->assetManager->Load(Tsukino::Core::Path("CombatAndroid/Assets/Anims/Player/Sprinting Forward Roll.fbx"));
        // Hammer Attack.fbx（プレイヤー既定の攻撃クリップ）は3回斬るモーションが1クリップに
        // 入っており、連撃の各段は同じハンドルを時間レンジだけ変えて3回参照する
        // （下のattackSteps初期化を参照）
        Tsukino::Asset::AssetHandle hammerAttackAnimHandle =
            context->assetManager->Load(Tsukino::Core::Path("CombatAndroid/Assets/Anims/Player/Hammer Attack.fbx"));
        // 武器ごとの専用攻撃クリップ（Great Sword Slash / Standing Melee Attack Backhand）と、
        // ウォーハンマー3段目のAoE(範囲攻撃)エフェクトのロードは WeaponSpawner の
        // ConfigureWeapon 側へ移した。AssetManager がパスでキャッシュするため、
        // 何本生成しても実際のロードは1回で済む
        // 死亡モーション（HP0でPlayerAnimationSystemがDeathステートへ遷移する。GameOverSystem参照）
        Tsukino::Asset::AssetHandle deathAnimHandle =
            context->assetManager->Load(Tsukino::Core::Path("CombatAndroid/Assets/Anims/Player/Falling Back Death.fbx"));

        // 敵（BigZombie / SmallZombie）が使うクリップのロードは
        // MakeBigZombieConfig / MakeSmallZombieConfig 側へ移した（EnemySpawner.cpp）。
        // AssetManager がパスでキャッシュするため、何体生成しても実際のロードは1回で済む

        Tsukino::ECS::Registry& registry = m_scene.GetRegistry();

        //--------------------------------------------------------------
        // 地面エンティティ
        //--------------------------------------------------------------
        {
            Tsukino::ECS::Entity groundEntity = m_scene.CreateEntity();
            // TransformComponent の追加と初期化
            // JumpGameSample等と同じ「1ユニット≒1cm」規約。半径5の薄い床にして、上面がちょうどy=0に来るよう中心をy=-5に置く
            Tsukino::BuiltIn::ECS::TransformComponent& groundTransform = registry.AddComponent<Tsukino::BuiltIn::ECS::TransformComponent>(groundEntity);
            groundTransform.position                                   = hlslpp::float3(0.0f, -5.0f, 0.0f);
            groundTransform.rotation                                   = hlslpp::quaternion(0.0f, 0.0f, 0.0f, 1.0f);    // 無回転
            groundTransform.scale                                      = hlslpp::float3(1.0f, 1.0f, 1.0f);
            groundTransform.dirty                                      = true;          // 初回計算のためフラグを立てる
            groundTransform.parent                                     = entt::null;    // 親なし

            // コリジョンをつける（一辺1000 x 厚さ10の床）
            Tsukino::BuiltIn::ECS::CollisionComponent& collision = registry.AddComponent<Tsukino::BuiltIn::ECS::CollisionComponent>(groundEntity);
            collision.extent                                     = {5000.0f, 5.0f, 5000.0f};
            collision.type                                       = Tsukino::BuiltIn::ECS::ColliderType::Box;
            collision.isSensor                                   = false;    // 明示的にソリッド判定にする（デフォルトも今はfalse）

            // RBをつける
            Tsukino::BuiltIn::ECS::RigidbodyComponent& rb = registry.AddComponent<Tsukino::BuiltIn::ECS::RigidbodyComponent>(groundEntity);
            rb.type                                       = Tsukino::BuiltIn::ECS::RigidbodyType::Static;
        }

        //--------------------------------------------------------------
        // プレイヤーエンティティ生成
        //--------------------------------------------------------------
        Tsukino::ECS::Entity playerEntity = m_scene.CreateEntity();

        // TransformComponent の追加と初期化
        // CharacterControllerComponent.centerOffsetを使うため、position＝カプセル底面（足元/接地位置）
        // を表す。地面の上面はy=0なので、埋まった状態で出現しないよう少し余裕を持たせてy=5から開始する
        Tsukino::BuiltIn::ECS::TransformComponent& playerTransform = registry.AddComponent<Tsukino::BuiltIn::ECS::TransformComponent>(playerEntity);
        playerTransform.position                                   = hlslpp::float3(0.0f, 5.0f, 0.0f);
        playerTransform.rotation                                   = hlslpp::quaternion(0.0f, 0.0f, 0.0f, 1.0f);    // 無回転
        // CharaTest.fbxの実寸を計測したところ身長はY=0〜100（足元がローカルY=0）で、想定していた
        // 「身長約210」の半分以下だったため、2.1倍(=210/100)スケールして合わせる
        playerTransform.scale  = hlslpp::float3(2.1f, 2.1f, 2.1f);
        playerTransform.dirty  = true;          // 初回計算のためフラグを立てる
        playerTransform.parent = entt::null;    // 親なし

        // プレイヤーとして動かすためCharacterControllerComponentをつける
        // （JumpGameSampleのカプセル(radius=35, halfHeight=70)と同じ規約に合わせる。
        //   CharacterVirtualの重力計算は手動なので、gravityFactorで底上げしないとほぼ落下しない）
        Tsukino::BuiltIn::ECS::CharacterControllerComponent& characterController =
            registry.AddComponent<Tsukino::BuiltIn::ECS::CharacterControllerComponent>(playerEntity);
        characterController.radius        = 35.0f;
        characterController.halfHeight    = 70.0f;
        characterController.maxSlopeDeg   = 45.0f;
        characterController.gravityFactor = 100.0f;    // 1ユニット=1cm換算でほぼ実重力(9.81m/s^2)相当
        // jumpSpeedは設定しない（ジャンプは回避へ差し替えたため、jumpRequestedを立てる箇所が無い）
        // カプセル中心をTransform位置から (halfHeight+radius) だけ上にずらし、
        // Transform位置＝カプセル底面（足元）を表すようにする（モデルの足元原点と揃えるため）
        characterController.centerOffset = hlslpp::float3(0.0f, characterController.halfHeight + characterController.radius, 0.0f);

        //-------------------------------------------------------------
        // プレイヤーはCharacterVirtual（Jolt物理のBodyではない仮想キャラクタ）で動いているため、
        // NarrowPhaseQuery::CollideShape（=PhysicsSystem::OverlapCapsule）では検出できない。
        // 敵の攻撃判定（CombatSystem）がプレイヤーを見つけられるよう、敵と同じ構成
        // （Kinematic + isSensor）のセンサーカプセルを別途持たせる。isSensor=trueなので
        // 物理的な押し出しは発生せず、移動は引き続きCharacterVirtualが担当する
        //-------------------------------------------------------------
        Tsukino::BuiltIn::ECS::RigidbodyComponent& playerRigidbody = registry.AddComponent<Tsukino::BuiltIn::ECS::RigidbodyComponent>(playerEntity);
        playerRigidbody.type                                       = Tsukino::BuiltIn::ECS::RigidbodyType::Kinematic;

        Tsukino::BuiltIn::ECS::CollisionComponent& playerCollision = registry.AddComponent<Tsukino::BuiltIn::ECS::CollisionComponent>(playerEntity);
        playerCollision.type           = Tsukino::BuiltIn::ECS::ColliderType::Capsule;
        playerCollision.extent         = hlslpp::float3(characterController.radius, characterController.halfHeight, 0.0f);
        playerCollision.isSensor       = true;
        playerCollision.offsetPosition = characterController.centerOffset;

        // プレイヤーコンポーネントをつける（PlayerSystemが入力を読み取るための目印）
        CombatAndroid::ECS::PlayerComponent& player = registry.AddComponent<CombatAndroid::ECS::PlayerComponent>(playerEntity);

        // HPを持たせる（敵の攻撃当たり判定によるダメージ計算に使用）
        registry.AddComponent<CombatAndroid::ECS::HealthComponent>(playerEntity);

        // EXP・レベルを持たせる（画面左上のEXPバー表示、ExpOrbSystemの吸収先に使用）
        registry.AddComponent<CombatAndroid::ECS::PlayerExperienceComponent>(playerEntity);

        // 取得済みスキルを持たせる（レベルアップ時のスキル選択で伸ばし、ExpOrbSystem/CombatSystemが効果を読む）
        registry.AddComponent<CombatAndroid::ECS::PlayerSkillComponent>(playerEntity);

        // ModelComponent の追加
        Tsukino::BuiltIn::ECS::ModelComponent& model = registry.AddComponent<Tsukino::BuiltIn::ECS::ModelComponent>(playerEntity);
        model.modelHandle                            = modelHandle;
        model.visible                                = true;

        // 溜め攻撃の段階表示（白→青→紫のリムライト）用。既定はactive=falseなので、溜めていない間は
        // 通常のモデル描画に一切影響しない（PlayerAnimationSystemが溜め中のみ書き込む）
        registry.AddComponent<Tsukino::BuiltIn::ECS::HighlightComponent>(playerEntity);

        // アニメーションを再生・制御するコンポーネント（初期状態はIdle。以後はPlayerAnimationSystemが管理する）
        Tsukino::BuiltIn::ECS::AnimationPlayerComponent& animPlayer = registry.AddComponent<Tsukino::BuiltIn::ECS::AnimationPlayerComponent>(playerEntity);
        animPlayer.current_clip_id                                  = idleAnimHandle;
        // index 0はMixamo製FBX共通の1tickスタブ、index 1が実モーション（PlayerAnimationSystemと合わせる）
        animPlayer.animation_index                                  = 1;
        animPlayer.elapsed_time                                     = 0.0f;
        animPlayer.playback_speed                                   = 1.0f;
        animPlayer.is_looping                                       = true;    // ループさせる
        animPlayer.is_playing                                       = true;    // 再生状態にする
        // In Placeの固定対象ノード名（Mixamoのリグ命名。WeaponComponent::handBoneNameと同じ流儀）。
        // 空にすると自動判定（スキニング対象ボーンのうち最も浅いもの）にフォールバックする
        animPlayer.root_motion_node_name                            = "mixamorig:Hips";

        // クリップの切り替え（AnimationSystemが読む「次に再生するクリップ」の受け皿）
        registry.AddComponent<Tsukino::BuiltIn::ECS::AnimationControllerComponent>(playerEntity);

        // PlayerAnimationSystemが参照する、ステートごとのアニメーションクリップ一式
        CombatAndroid::ECS::PlayerAnimationSetComponent& animSet = registry.AddComponent<CombatAndroid::ECS::PlayerAnimationSetComponent>(playerEntity);
        animSet.idleClip                                      = idleAnimHandle;
        animSet.runClip                                       = runAnimHandle;
        animSet.fastRunClip                                   = fastRunAnimHandle;
        animSet.dodgeClip                                     = dodgeAnimHandle;
        animSet.deathClip                                     = deathAnimHandle;
        animSet.currentState                                  = CombatAndroid::ECS::PlayerAnimState::Idle;

        //-------------------------------------------------------------
        // 回避（スペースキー）のチューニング値。攻撃の分割定数と同じく、実機で見ながら
        // ここで詰める前提の初期値（_DEBUGビルドのPlayerAnimationSystemが出すDODGEログを見る）。
        // dodgeInvincibleDurationは回避全体より短くして「終わり際は被弾する」ようにしている
        //-------------------------------------------------------------
        player.dodgeSpeed              = 600.0f;    // moveSpeed(300)の2倍
        player.dodgePlaybackSpeed      = 1.5f;      // 前転を等速より速く。回避時間が1/1.5になり、進む距離も同じだけ縮む
        player.dodgeInvincibleDuration = 0.3f;      // 実時間。dodgePlaybackSpeedを変えたら合わせて見直す
        player.dodgeCooldown           = 0.3f;

        // Hammer Attack.fbx は3回斬るモーションが1クリップ（30fps / 106フレーム = 3.5333秒）に
        // 入っている。各段のstartTime/endTime/playbackSpeedは実機で見ながら個別に微調整する前提の
        // 初期値（_DEBUGビルドのPlayerAnimationSystemが出すATTACKログとWeaponGripDebugSystemの
        // F10/F11コマ送りで追い込む）。ループではなく段ごとに書き下すことで、1段ずつ独立して
        // 長さ・速度を変えられるようにしている
        constexpr float kAttackClipDuration = 3.5333f;
        constexpr float kAttackStepLength   = kAttackClipDuration / 3.0f;    // 約1.178秒 ≒ 35.3フレーム
        constexpr float kAttackPlaybackSpeed = 1.5f;    // 攻撃全体を等速より少し速く（1.0で従来通りの速さ）

        animSet.attackSteps[0].clip           = hammerAttackAnimHandle;
        animSet.attackSteps[0].animationIndex = 1;
        animSet.attackSteps[0].startTime      = kAttackStepLength * 0.0f;
        animSet.attackSteps[0].endTime        = kAttackStepLength * 1.0f;
        animSet.attackSteps[0].playbackSpeed  = kAttackPlaybackSpeed;
        // 攻撃モーションのルート前進を殺す（コリジョンから離れる/戻る瞬間に吸い寄せられる問題への対処）
        animSet.attackSteps[0].inPlace        = true;

        animSet.attackSteps[1].clip           = hammerAttackAnimHandle;
        animSet.attackSteps[1].animationIndex = 1;
        animSet.attackSteps[1].startTime      = kAttackStepLength * 1.0f;
        animSet.attackSteps[1].endTime        = kAttackStepLength * 1.3f;
        animSet.attackSteps[1].playbackSpeed  = kAttackPlaybackSpeed;
        animSet.attackSteps[1].inPlace        = true;

        animSet.attackSteps[2].clip           = hammerAttackAnimHandle;
        animSet.attackSteps[2].animationIndex = 1;
        animSet.attackSteps[2].startTime      = kAttackStepLength * 1.3f;
        animSet.attackSteps[2].endTime        = kAttackClipDuration;
        animSet.attackSteps[2].playbackSpeed  = kAttackPlaybackSpeed;
        animSet.attackSteps[2].inPlace        = true;
        // 3段目は他の2段よりモーションが長い（実時間約1.34秒）ため、固定0.25秒の判定窓では
        // 斬撃が敵へ届く前にヒット判定が閉じてしまう。暫定的に長めの値を設定する。
        // 最終値は実機でF10/F11 + ATTACKログ（PlayerAnimationSystem）を見ながら詰めること
        animSet.attackSteps[2].hitWindowDuration = 0.6f;
        // 3段目（フィニッシュ）だけ重い一撃としてダメージを2倍にする。
        // 敵のknockbackDamageThresholdと組み合わせて「重い武器の3段目だけノックバックする」を実現する
        animSet.attackSteps[2].damageMultiplier   = 2.0f;
        // 3段目はAoE(範囲攻撃)を要求する段として扱う。実際に発動するのは装備武器が
        // WeaponComponent::areaAttackRadius>0を持つ場合のみ（下でwarhammerにのみ設定）
        animSet.attackSteps[2].areaAttack      = true;
        animSet.attackSteps[2].areaAttackDelay = 0.35f;    // 実機でF10/F11 + AoEデバッグ円（マゼンタ）を見ながら調整する

        Tsukino::BuiltIn::ECS::SpringBoneComponent& springBone = registry.AddComponent<Tsukino::BuiltIn::ECS::SpringBoneComponent>(playerEntity);

        Tsukino::BuiltIn::ECS::SpringBoneComponent::ChainDef breastL;
        breastL.name                   = "Breast_L";
        breastL.rootNodeName           = "Breast_L";
        breastL.maxDepth               = 1;
        breastL.settings.stiffness     = 0.35f;    // リアル(0.55)より少し柔らかく、揺れ幅を出す
        breastL.settings.drag          = 0.13f;    // 収まりをやや長めに（2〜3往復くらい残る）
        breastL.settings.inertia       = 0.5f;     // 体の動きに対して、わずかに「置いていかれる」感を演出
        breastL.settings.gravityScale  = 1.0f;
        breastL.settings.angleLimitDeg = 26.0f;
        springBone.chainDefs.push_back(breastL);

        Tsukino::BuiltIn::ECS::SpringBoneComponent::ChainDef breastR;
        breastR.name                   = "Breast_R";
        breastR.rootNodeName           = "Breast_R";
        breastR.maxDepth               = 1;
        breastR.settings.stiffness     = 0.35f;
        breastR.settings.drag          = 0.13f;
        breastR.settings.inertia       = 0.5f;
        breastR.settings.gravityScale  = 1.0f;
        breastR.settings.angleLimitDeg = 26.0f;
        springBone.chainDefs.push_back(breastR);

        // 計算されたボーン行列の出力先（スキニング用）コンポーネント
        Tsukino::BuiltIn::ECS::SkeletonOutputComponent& skeletonOutput = registry.AddComponent<Tsukino::BuiltIn::ECS::SkeletonOutputComponent>(playerEntity);

        //--------------------------------------------------------------
        // 武器エンティティ生成。
        // 「武器1種類をどう組み立てるか」（メッシュ・握り・専用攻撃モーション・
        // AoE(範囲攻撃)・溜め攻撃）は WeaponSpawner の定義テーブルへ集約してあるため、
        // ここでは「どこに何を置くか」だけを書く。敵（Paladin）が持って湧く武器も、
        // 撃破時に落とす武器も同じ経路を通るので、出所によらず性能はまったく同じになる
        //--------------------------------------------------------------

        // 最初から装備している唯一の武器。プレイヤーの右肩斜め上で浮遊させる。
        // 位置・回転はCombatSystemが毎フレーム所有者（プレイヤー）を基準に計算するため、
        // ここで渡す位置は次のフレームまでの初期値でしかない
        Tsukino::ECS::Entity warhammerEntity =
            CombatAndroid::ECS::SpawnWeapon(registry, *context, CombatAndroid::ECS::WeaponId::Warhammer,
                                            playerTransform.position, playerEntity);

        // 切り替え対象の武器一覧（PlayerSystemがマウスホイール入力でここを順送りする）。
        // 初期状態はwarhammerのみ。他の武器はワールドに落ちており、Fキーで拾うとここに増える
        player.weaponInventory     = {warhammerEntity};
        player.selectedWeaponIndex = 0;
        player.weaponEntity        = warhammerEntity;
        {
            auto& warhammerWeapon = registry.GetComponent<CombatAndroid::ECS::WeaponComponent>(warhammerEntity);

            // 非攻撃時：右手ボーンへのアタッチは、Idle.fbx（アニメーションクリップ）側のボーン姿勢データが
            // 実際の見た目のポーズと一致しない（別アセットのため、ボーン名は一致してもリグの前提が食い違っている）
            // ため使わない。SpawnWeaponが入れるhandTrackingWeight=0のまま所有者のルートTransformからの
            // 固定オフセットにのみ追従させ、floatEnabledで「手に持つ」のではなく肩の斜め上を
            // ふわふわ浮遊する演出にする。
            // 攻撃時：PlayerAnimationSystemがWeaponComponent::isAttackingをセットし、CombatSystemはこの間
            // floatEnabledを無視してattackHandTrackingWeight（既定1.0=完全追従）でAttackクリップの
            // 右手ボーン姿勢へ追従させる（Idleと違い、Attackクリップ自体の腕の振りに合わせて動くため）
            warhammerWeapon.floatEnabled  = true;
            warhammerWeapon.floatSelected = true;
        }

        //--------------------------------------------------------------
        // 地面に落ちている武器の生成（Fキーで拾える）。
        // ownerを設定しないため、CombatSystemの追従処理（owner != entt::nullが条件）には入らず
        // その場に留まる。PickupSystemが範囲内・最近傍の1本だけをハイライトし、Fキーで
        // WeaponComponent::ownerをプレイヤーへ設定して浮遊武器へ昇格させる。
        // 動作確認用に近い位置へまとめて置き、「同時に範囲内でも1つだけ光る」ことを
        // 確認できるようにしている
        //--------------------------------------------------------------
        CombatAndroid::ECS::SpawnWeapon(registry, *context, CombatAndroid::ECS::WeaponId::Greatsword,
                                        hlslpp::float3(250.0f, 10.0f, 0.0f));
        CombatAndroid::ECS::SpawnWeapon(registry, *context, CombatAndroid::ECS::WeaponId::Battleaxe,
                                        hlslpp::float3(300.0f, 10.0f, 100.0f));
        CombatAndroid::ECS::SpawnWeapon(registry, *context, CombatAndroid::ECS::WeaponId::Warhammer,
                                        hlslpp::float3(340.0f, 10.0f, 0.0f));

        //--------------------------------------------------------------
        // 敵エンティティ生成。全敵共通でビヘイビアツリー駆動（歩く→射程内で攻撃、被弾でノックバック、
        // 死亡でStunned→フェードアウト）にする。当たり判定は物理形状（Joltのカプセルセンサー）で行う。
        // 1体分の生成処理は CombatAndroid/src/ECS/Utility/EnemySpawner.cpp へ切り出してあり、
        // パラメータはMakeSmallZombieConfig / MakeBigZombieConfig が持つ。
        //
        // 実行中の湧き潰し（サバイバー化）は EnemySpawnDirectorSystem が担当する。
        // ここで置く4体はSpawnedEnemyComponentを持たないため同System の間引き対象外で、
        // 起動直後に画面が空にならないための最低限の見た目と、武器の当たり・ノックバック
        // 閾値を確認するための固定サンプルを兼ねる
        //--------------------------------------------------------------
        CombatAndroid::ECS::SpawnBehaviorEnemy(registry, *context,
                                               CombatAndroid::ECS::MakeSmallZombieConfig(*context, hlslpp::float3(200.0f, 20.0f, 200.0f)));
        CombatAndroid::ECS::SpawnBehaviorEnemy(registry, *context,
                                               CombatAndroid::ECS::MakeSmallZombieConfig(*context, hlslpp::float3(-200.0f, 20.0f, 200.0f)));
        CombatAndroid::ECS::SpawnBehaviorEnemy(registry, *context,
                                               CombatAndroid::ECS::MakeSmallZombieConfig(*context, hlslpp::float3(0.0f, 20.0f, -250.0f)));

        CombatAndroid::ECS::SpawnBehaviorEnemy(registry, *context,
                                               CombatAndroid::ECS::MakeBigZombieConfig(*context, hlslpp::float3(-250.0f, 20.0f, -250.0f)));



        //--------------------------------------------------------------
        // 2Dカメラエンティティの生成
        //--------------------------------------------------------------
        Tsukino::ECS::Entity cameraEntity2D = m_scene.CreateEntity();

        // TransformComponent (カメラの位置)
        Tsukino::BuiltIn::ECS::TransformComponent& camTransform2D = registry.AddComponent<Tsukino::BuiltIn::ECS::TransformComponent>(cameraEntity2D);
        camTransform2D.position                                   = hlslpp::float3(0.0f, 0.0f, -1.0f);    // 手前に引く

        // CameraComponent (投影設定)
        Tsukino::BuiltIn::ECS::CameraComponent& camera2D = registry.AddComponent<Tsukino::BuiltIn::ECS::CameraComponent>(cameraEntity2D);
        camera2D.projectionType                          = Tsukino::BuiltIn::ECS::CameraComponent::ProjectionType::Orthographic;
        camera2D.orthoSize                               = 1000.0f;    // 画面の縦幅を 720 ユニットにする
        camera2D.isPrimary                               = false;      // これをメインカメラにしない

        //--------------------------------------------------------------
        // 「Fキーで拾う」UIラベル用エンティティの生成。
        // 毎フレーム生成せず1つを使い回し、PickupSystemがWorldAnchorComponent.target/textを書き換え、
        // 実際の座標変換（position書き込み）はWorldAnchorSystemが行う
        //--------------------------------------------------------------
        Tsukino::ECS::Entity pickupPromptEntity = m_scene.CreateEntity();

        Tsukino::BuiltIn::ECS::TransformComponent& promptTransform = registry.AddComponent<Tsukino::BuiltIn::ECS::TransformComponent>(pickupPromptEntity);
        promptTransform.position                                    = hlslpp::float3(0.0f, 0.0f, 0.0f);    // 以後WorldAnchorSystemが毎フレーム上書きする
        promptTransform.scale                                       = hlslpp::float3(1.0f, 1.0f, 1.0f);
        promptTransform.dirty                                       = true;

        Tsukino::BuiltIn::ECS::WorldAnchorComponent& promptAnchor = registry.AddComponent<Tsukino::BuiltIn::ECS::WorldAnchorComponent>(pickupPromptEntity);
        promptAnchor.target                                        = entt::null;    // 以後PickupSystemが毎フレーム更新する

        Tsukino::BuiltIn::ECS::FontComponent& promptFont = registry.AddComponent<Tsukino::BuiltIn::ECS::FontComponent>(pickupPromptEntity);
        promptFont.text                                   = L"";    // 空文字の間はFontRendererSystemが描画しない
        promptFont.color                                  = hlslpp::float4(1.0f, 1.0f, 1.0f, 1.0f);
        promptFont.origin                                 = hlslpp::float2(0.0f, 0.0f);
        promptFont.sortOrder                              = CombatAndroid::UI::kPickupPrompt;    // 操作の案内なのでダメージ数値より手前に置く
        // fontHandle未設定 → builtinAssets->fonts.defaultFont（Default.dfont、動的フォントアトラス経路）が使われるため
        // 日本語をそのまま渡してよい（旧Arial.spritefontはASCII専用でDirectXTKが例外を投げる）

        registry.AddComponent<CombatAndroid::ECS::PickupPromptComponent>(pickupPromptEntity);

        //--------------------------------------------------------------
        // ダメージ数値用エンティティのプール。「Fキーで拾う」ラベルと同じく毎フレーム生成せず
        // 固定数を使い回す。WeaponHitEventはCombatSystemのview.eachの内側からPublishされるため、
        // そのハンドラでエンティティを生成するとEnTTのイテレータが壊れる。
        // DamageNumberSystemはここで作ったスロットの空きを探して再利用する
        //--------------------------------------------------------------
        for(int i = 0; i < CombatAndroid::ECS::kDamageNumberPoolSize; ++i) {
            Tsukino::ECS::Entity damageNumberEntity = m_scene.CreateEntity();

            Tsukino::BuiltIn::ECS::TransformComponent& damageNumberTransform =
                registry.AddComponent<Tsukino::BuiltIn::ECS::TransformComponent>(damageNumberEntity);
            damageNumberTransform.scale = hlslpp::float3(0.0f, 0.0f, 0.0f);    // 未使用スロットは非表示
            damageNumberTransform.dirty = true;

            Tsukino::BuiltIn::ECS::WorldAnchorComponent& damageNumberAnchor =
                registry.AddComponent<Tsukino::BuiltIn::ECS::WorldAnchorComponent>(damageNumberEntity);
            damageNumberAnchor.target = entt::null;    // 以後DamageNumberSystemがfixedWorldPositionを使う

            Tsukino::BuiltIn::ECS::FontComponent& damageNumberFont =
                registry.AddComponent<Tsukino::BuiltIn::ECS::FontComponent>(damageNumberEntity);
            damageNumberFont.text      = L"";    // 空文字の間はFontRendererSystemが描画しない
            damageNumberFont.sortOrder = CombatAndroid::UI::kDamageNumber;
            // fontHandle未設定 → builtinAssets->fonts.defaultFont（Default.dfont）が使われる

            registry.AddComponent<CombatAndroid::ECS::DamageNumberComponent>(damageNumberEntity);
        }

        //--------------------------------------------------------------
        // EXP玉用エンティティのプール。ダメージ数値と同じく毎フレーム生成せず固定数を使い回す。
        // EnemyDiedEventはビヘイビアツリーのアクション（View反復中）からPublishされるため、
        // ExpOrbSystemはここで作ったスロットの空きを探して再利用する
        //--------------------------------------------------------------
        {
            Tsukino::Asset::AssetHandle expOrbTextureHandle =
                context->assetManager->Load(Tsukino::Core::Path("CombatAndroid/Assets/Textures/UI/ExpOrb.png"));

            for(int i = 0; i < CombatAndroid::ECS::kExpOrbPoolSize; ++i) {
                Tsukino::ECS::Entity expOrbEntity = m_scene.CreateEntity();

                Tsukino::BuiltIn::ECS::TransformComponent& expOrbTransform =
                    registry.AddComponent<Tsukino::BuiltIn::ECS::TransformComponent>(expOrbEntity);
                expOrbTransform.scale = hlslpp::float3(0.0f, 0.0f, 0.0f);    // 未使用スロットは非表示
                expOrbTransform.dirty = true;

                // 3Dワールド上を落下・飛行する演出のため、WorldAnchorComponent（画面固定UI用）は使わず、
                // SpriteComponent.space=Worldでpositionを直接3D座標として扱い、主カメラを向く
                // ビルボードとして深度テストされる形で描画する（敵の後ろに回ったら正しく隠れる）
                Tsukino::BuiltIn::ECS::SpriteComponent& expOrbSprite = registry.AddComponent<Tsukino::BuiltIn::ECS::SpriteComponent>(expOrbEntity);
                expOrbSprite.textureHandle = expOrbTextureHandle;
                expOrbSprite.blendMode     = Tsukino::BuiltIn::ECS::SpriteBlendMode::Additive;    // 発光して見えるよう加算合成にする
                expOrbSprite.space         = Tsukino::BuiltIn::ECS::SpriteSpace::World;
                expOrbSprite.sortOrder     = CombatAndroid::UI::World::kExpOrb;    // 同じWorldパス内の他スプライトより手前に描く

                registry.AddComponent<CombatAndroid::ECS::ExpOrbComponent>(expOrbEntity);
            }
        }

        //--------------------------------------------------------------
        // 画面左上のプレイヤーHP/EXPバー。WorldAnchorComponentは使わず固定ピクセル座標に置き、
        // PlayerHudSystemが毎フレームHealthComponent/PlayerExperienceComponentの値へ合わせて更新する
        //--------------------------------------------------------------
        {
            Tsukino::Asset::AssetHandle hudBarTextureHandle =
                context->assetManager->Load(Tsukino::Core::Path("CombatAndroid/Assets/Textures/UI/WhitePixel.png"));

            auto makeBarSprite = [&](int sortOrder) {
                Tsukino::ECS::Entity barEntity = m_scene.CreateEntity();

                Tsukino::BuiltIn::ECS::TransformComponent& barTransform =
                    registry.AddComponent<Tsukino::BuiltIn::ECS::TransformComponent>(barEntity);
                barTransform.dirty = true;    // 実際の位置・スケールはPlayerHudSystemが毎フレーム書く

                Tsukino::BuiltIn::ECS::SpriteComponent& barSprite = registry.AddComponent<Tsukino::BuiltIn::ECS::SpriteComponent>(barEntity);
                barSprite.textureHandle = hudBarTextureHandle;
                barSprite.sortOrder     = sortOrder;

                return barEntity;
            };

            auto makeHudText = [&]() {
                Tsukino::ECS::Entity textEntity = m_scene.CreateEntity();

                registry.AddComponent<Tsukino::BuiltIn::ECS::TransformComponent>(textEntity);

                Tsukino::BuiltIn::ECS::FontComponent& font = registry.AddComponent<Tsukino::BuiltIn::ECS::FontComponent>(textEntity);
                font.text                                   = L"";
                font.color                                  = hlslpp::float4(1.0f, 1.0f, 1.0f, 1.0f);
                font.outlineColor                           = hlslpp::float4(0.0f, 0.0f, 0.0f, 1.0f);
                font.outlineWidth                           = 2.0f;
                font.verticalAlign                          = Tsukino::BuiltIn::ECS::VerticalAlign::Middle;
                font.sortOrder                              = CombatAndroid::UI::kHudText;    // バーより手前に描く

                return textEntity;
            };

            // 走行の経過時間と危険度ランク。RunClockSystemだけが書き込み、
            // PlayerHudSystem（表示）とEnemySpawnDirectorSystem（敵の強化）が読む
            registry.AddComponent<CombatAndroid::ECS::RunClockComponent>(playerEntity);

            CombatAndroid::ECS::PlayerHudComponent& hud = registry.AddComponent<CombatAndroid::ECS::PlayerHudComponent>(playerEntity);
            hud.hpBarBackgroundEntity                    = makeBarSprite(CombatAndroid::UI::kHudBarBackground);
            hud.hpBarFillEntity                          = makeBarSprite(CombatAndroid::UI::kHudBarFill);
            hud.hpTextEntity                             = makeHudText();
            hud.expBarBackgroundEntity                   = makeBarSprite(CombatAndroid::UI::kHudBarBackground);
            hud.expBarFillEntity                         = makeBarSprite(CombatAndroid::UI::kHudBarFill);
            hud.expTextEntity                            = makeHudText();

            //-------------------------------------------------------------
            // 画面上部中央の生存時間テキスト。HPバーと同じくWorldAnchorComponentは使わず
            // 固定ピクセル座標に置き、PlayerHudSystemが毎フレームtextだけ書き換える
            // （位置は開始時点のウィンドウ幅基準。GAME OVERテキストと同じ割り切り）
            //-------------------------------------------------------------
            float survivalTimeScreenCenterX = context->window ? static_cast<float>(context->window->GetWidth()) * 0.5f : 850.0f;

            Tsukino::ECS::Entity survivalTimeEntity = m_scene.CreateEntity();

            Tsukino::BuiltIn::ECS::TransformComponent& survivalTimeTransform =
                registry.AddComponent<Tsukino::BuiltIn::ECS::TransformComponent>(survivalTimeEntity);
            survivalTimeTransform.position = hlslpp::float3(survivalTimeScreenCenterX, 24.0f, 0.0f);
            survivalTimeTransform.dirty     = true;

            Tsukino::BuiltIn::ECS::FontComponent& survivalTimeFont =
                registry.AddComponent<Tsukino::BuiltIn::ECS::FontComponent>(survivalTimeEntity);
            survivalTimeFont.text              = L"";
            survivalTimeFont.color              = hlslpp::float4(1.0f, 1.0f, 1.0f, 1.0f);
            survivalTimeFont.outlineColor      = hlslpp::float4(0.0f, 0.0f, 0.0f, 1.0f);
            survivalTimeFont.outlineWidth      = 2.0f;
            survivalTimeFont.horizontalAlign  = Tsukino::BuiltIn::ECS::HorizontalAlign::Center;
            survivalTimeFont.verticalAlign    = Tsukino::BuiltIn::ECS::VerticalAlign::Top;
            survivalTimeFont.sortOrder         = CombatAndroid::UI::kHudText;

            hud.survivalTimeTextEntity = survivalTimeEntity;

            //-------------------------------------------------------------
            // 生存時間のすぐ下に置く危険度テキスト。時間経過で敵が強くなることを
            // 見せておかないと、難易度上昇が原因不明の理不尽になる。
            // 横並びにしないのは、"12:34" の描画幅を知らないと重なるため
            //-------------------------------------------------------------
            //! 生存時間テキストの上端からの縦オフセット（ピクセル）。フォントの行高ぶん下げる
            constexpr float kDangerRankTextOffsetY = 34.0f;

            Tsukino::ECS::Entity dangerRankEntity = m_scene.CreateEntity();

            Tsukino::BuiltIn::ECS::TransformComponent& dangerRankTransform =
                registry.AddComponent<Tsukino::BuiltIn::ECS::TransformComponent>(dangerRankEntity);
            dangerRankTransform.position = hlslpp::float3(survivalTimeScreenCenterX, 24.0f + kDangerRankTextOffsetY, 0.0f);
            dangerRankTransform.dirty     = true;

            Tsukino::BuiltIn::ECS::FontComponent& dangerRankFont =
                registry.AddComponent<Tsukino::BuiltIn::ECS::FontComponent>(dangerRankEntity);
            dangerRankFont.text              = L"";
            dangerRankFont.color              = hlslpp::float4(1.0f, 1.0f, 1.0f, 1.0f);
            dangerRankFont.outlineColor      = hlslpp::float4(0.0f, 0.0f, 0.0f, 1.0f);
            dangerRankFont.outlineWidth      = 2.0f;
            dangerRankFont.horizontalAlign  = Tsukino::BuiltIn::ECS::HorizontalAlign::Center;
            dangerRankFont.verticalAlign    = Tsukino::BuiltIn::ECS::VerticalAlign::Top;
            dangerRankFont.sortOrder         = CombatAndroid::UI::kHudText;

            hud.dangerRankTextEntity = dangerRankEntity;

            //-------------------------------------------------------------
            // EXPバーの下に並べる「取得済みスキル一覧」。1行＝「アイコン枠スプライト＋文字」で、
            // スキルの種類数ぶんを非表示（スケール0／空文字）で作っておき、
            // PlayerSkillHudSystemが取得済みのものだけを上から詰めて書き込む。
            //
            // 今はアイコン枠にWhitePixelを入れてスキル色で着色した四角を出しているだけだが、
            // スキルごとのアイコン画像を用意したらSystem側の差し替え先を変えるだけで絵になる
            // （バー・文字と同じ作りなので、makeBarSprite／makeHudTextをそのまま使える）
            //-------------------------------------------------------------
            CombatAndroid::ECS::PlayerSkillHudComponent& skillHud =
                registry.AddComponent<CombatAndroid::ECS::PlayerSkillHudComponent>(playerEntity);

            for(CombatAndroid::ECS::PlayerSkillHudRow& skillHudRow : skillHud.rows) {
                skillHudRow.iconEntity = makeBarSprite(CombatAndroid::UI::kHudSkillIcon);
                skillHudRow.textEntity = makeHudText();
            }
        }

        //--------------------------------------------------------------
        // 画面右の取得ログ用エンティティのプール。ダメージ数値と同じく毎フレーム生成せず
        // 固定数を使い回す（GameLogEventはExpOrbSystemのview.eachの内側からもPublishされるため、
        // そのハンドラでエンティティを生成するとEnTTのイテレータが壊れる）。
        //
        // 1行＝「パネル・アクセントバー・種別ラベル・主題」の4エンティティで、
        // 根であるパネルにGameLogComponentを付け、残り3つはそこからエンティティ参照で辿る。
        // 位置・大きさ・色は全てGameLogSystemが毎フレーム画面サイズから計算して書くため、
        // ここでは非表示（スケール0／空文字）の状態だけ作っておく
        //--------------------------------------------------------------
        {
            Tsukino::Asset::AssetHandle gameLogTextureHandle =
                context->assetManager->Load(Tsukino::Core::Path("CombatAndroid/Assets/Textures/UI/WhitePixel.png"));

            auto makeGameLogSprite = [&](int sortOrder) {
                Tsukino::ECS::Entity spriteEntity = m_scene.CreateEntity();

                Tsukino::BuiltIn::ECS::TransformComponent& spriteTransform =
                    registry.AddComponent<Tsukino::BuiltIn::ECS::TransformComponent>(spriteEntity);
                spriteTransform.scale = hlslpp::float3(0.0f, 0.0f, 0.0f);    // 未使用スロットは非表示
                spriteTransform.dirty = true;

                Tsukino::BuiltIn::ECS::SpriteComponent& sprite = registry.AddComponent<Tsukino::BuiltIn::ECS::SpriteComponent>(spriteEntity);
                sprite.textureHandle                           = gameLogTextureHandle;
                sprite.sortOrder                               = sortOrder;

                return spriteEntity;
            };

            auto makeGameLogText = [&]() {
                Tsukino::ECS::Entity textEntity = m_scene.CreateEntity();

                registry.AddComponent<Tsukino::BuiltIn::ECS::TransformComponent>(textEntity);

                Tsukino::BuiltIn::ECS::FontComponent& font = registry.AddComponent<Tsukino::BuiltIn::ECS::FontComponent>(textEntity);
                font.text                                  = L"";    // 空文字の間はFontRendererSystemが描画しない
                font.outlineColor                          = hlslpp::float4(0.0f, 0.0f, 0.0f, 1.0f);
                font.outlineWidth                          = 2.0f;
                font.horizontalAlign                       = Tsukino::BuiltIn::ECS::HorizontalAlign::Left;
                font.verticalAlign                         = Tsukino::BuiltIn::ECS::VerticalAlign::Middle;
                font.sortOrder                             = CombatAndroid::UI::kGameLogText;    // パネル・バーより手前に描く
                // fontHandle未設定 → builtinAssets->fonts.defaultFont（Default.dfont）が使われるため
                // 日本語をそのまま渡してよい

                return textEntity;
            };

            for(int i = 0; i < CombatAndroid::ECS::kGameLogPoolSize; ++i) {
                // 4つとも作り切ってからGameLogComponentを付ける。先に付けてしまうと、
                // 残りの生成中に得た参照が生きているかどうかを気にする必要が出る
                Tsukino::ECS::Entity panelEntity  = makeGameLogSprite(CombatAndroid::UI::kGameLogPanel);
                Tsukino::ECS::Entity accentEntity = makeGameLogSprite(CombatAndroid::UI::kGameLogAccent);
                Tsukino::ECS::Entity labelEntity  = makeGameLogText();
                Tsukino::ECS::Entity textEntity   = makeGameLogText();

                CombatAndroid::ECS::GameLogComponent& gameLog = registry.AddComponent<CombatAndroid::ECS::GameLogComponent>(panelEntity);
                gameLog.accentEntity                          = accentEntity;
                gameLog.labelEntity                           = labelEntity;
                gameLog.textEntity                            = textEntity;
            }
        }

        //--------------------------------------------------------------
        // 被弾演出（点滅・画面フラッシュ）とGAME OVER表示。
        // 画面フラッシュは画面全体を覆う単色スプライトで、位置・サイズはPlayerDamageEffectSystemが
        // 毎フレーム画面サイズに合わせて書く（HUDバーと同じくWhitePixel.pngを使い回す）
        //--------------------------------------------------------------
        {
            Tsukino::Asset::AssetHandle whitePixelHandle =
                context->assetManager->Load(Tsukino::Core::Path("CombatAndroid/Assets/Textures/UI/WhitePixel.png"));

            Tsukino::ECS::Entity screenFlashEntity = m_scene.CreateEntity();

            Tsukino::BuiltIn::ECS::TransformComponent& flashTransform =
                registry.AddComponent<Tsukino::BuiltIn::ECS::TransformComponent>(screenFlashEntity);
            flashTransform.dirty = true;    // 実際の位置・スケールはPlayerDamageEffectSystemが毎フレーム書く

            Tsukino::BuiltIn::ECS::SpriteComponent& flashSprite = registry.AddComponent<Tsukino::BuiltIn::ECS::SpriteComponent>(screenFlashEntity);
            flashSprite.textureHandle = whitePixelHandle;
            flashSprite.tintColor     = hlslpp::float4(0.9f, 0.05f, 0.05f, 0.0f);    // 初期状態は透明
            flashSprite.sortOrder     = CombatAndroid::UI::kScreenDamageFlash;    // HUDより手前、GAME OVERテキストより奥

            CombatAndroid::ECS::PlayerDamageEffectComponent& damageEffect =
                registry.AddComponent<CombatAndroid::ECS::PlayerDamageEffectComponent>(playerEntity);
            damageEffect.screenFlashEntity = screenFlashEntity;

            //-------------------------------------------------------------
            // GAME OVER / リトライ案内テキスト。「Fキーで拾う」ラベルと同じく空文字で非表示にしておき、
            // GameOverSystemがHealthComponent::isDeadを検知した時点でtextを書き込む。
            // 位置は画面中央基準の固定ピクセル座標（開始時点のウィンドウサイズで決める。
            // リサイズには追従しない＝他の画面固定UIと同じ割り切り）
            //-------------------------------------------------------------
            float screenCenterX = context->window ? static_cast<float>(context->window->GetWidth()) * 0.5f : 850.0f;
            float screenHeight   = context->window ? static_cast<float>(context->window->GetHeight()) : 1000.0f;

            auto makeCenteredOverlayText = [&](float screenY, float fontScale) {
                Tsukino::ECS::Entity textEntity = m_scene.CreateEntity();

                Tsukino::BuiltIn::ECS::TransformComponent& textTransform =
                    registry.AddComponent<Tsukino::BuiltIn::ECS::TransformComponent>(textEntity);
                textTransform.position = hlslpp::float3(screenCenterX, screenY, 0.0f);
                textTransform.scale     = hlslpp::float3(fontScale, fontScale, 1.0f);
                textTransform.dirty     = true;

                Tsukino::BuiltIn::ECS::FontComponent& font = registry.AddComponent<Tsukino::BuiltIn::ECS::FontComponent>(textEntity);
                font.text              = L"";    // 空文字の間はFontRendererSystemが描画しない
                font.color              = hlslpp::float4(1.0f, 1.0f, 1.0f, 1.0f);
                font.outlineColor      = hlslpp::float4(0.0f, 0.0f, 0.0f, 1.0f);
                font.outlineWidth      = 3.0f;
                font.horizontalAlign  = Tsukino::BuiltIn::ECS::HorizontalAlign::Center;
                font.verticalAlign    = Tsukino::BuiltIn::ECS::VerticalAlign::Middle;
                font.sortOrder         = CombatAndroid::UI::kGameOverText;    // 画面フラッシュより手前に描く

                return textEntity;
            };

            CombatAndroid::ECS::GameOverComponent& gameOver = registry.AddComponent<CombatAndroid::ECS::GameOverComponent>(playerEntity);
            gameOver.titleTextEntity = makeCenteredOverlayText(screenHeight * 0.42f, 2.4f);
            gameOver.retryTextEntity = makeCenteredOverlayText(screenHeight * 0.55f, 1.1f);

            //-------------------------------------------------------------
            // レベルアップ時のスキル選択メニュー。
            // 「暗転板 + カード3枚（背景パネル・スキル名・説明文）+ 選択中の強調枠 + タイトル」を
            // 全て非表示（スケール0／空文字）で作っておき、SkillSelectSystemが表示のたびに
            // 位置・大きさ・色・文言を書き込む。
            //
            // ここで位置を焼き込まないのは、選択肢がカンストで3枚に満たない回があり、
            // 枚数によって縦の並びが変わるため（レイアウトの計算はSystem側に集約している）。
            // sortOrderは CombatAndroid/UI/UiSortOrder.hpp の kSkillSelect* 帯を使う
            //-------------------------------------------------------------
            auto makeSkillPanelSprite = [&](int sortOrder) {
                Tsukino::ECS::Entity panelEntity = m_scene.CreateEntity();

                Tsukino::BuiltIn::ECS::TransformComponent& panelTransform =
                    registry.AddComponent<Tsukino::BuiltIn::ECS::TransformComponent>(panelEntity);
                panelTransform.scale = hlslpp::float3(0.0f, 0.0f, 0.0f);    // スケール0の間はSpriteRenderSystemが描画しない
                panelTransform.dirty = true;

                Tsukino::BuiltIn::ECS::SpriteComponent& panelSprite =
                    registry.AddComponent<Tsukino::BuiltIn::ECS::SpriteComponent>(panelEntity);
                // カードの背景はSkillSelectSystemがスキルテーブルのパスから差し替える。
                // ここではハンドル未設定のまま描画されないよう、暫定でWhitePixelを入れておく
                panelSprite.textureHandle = whitePixelHandle;
                panelSprite.sortOrder     = sortOrder;

                return panelEntity;
            };

            auto makeSkillText = [&](Tsukino::BuiltIn::ECS::HorizontalAlign horizontalAlign, float outlineWidth) {
                Tsukino::ECS::Entity textEntity = m_scene.CreateEntity();

                Tsukino::BuiltIn::ECS::TransformComponent& textTransform =
                    registry.AddComponent<Tsukino::BuiltIn::ECS::TransformComponent>(textEntity);
                textTransform.dirty = true;    // 実際の位置・フォントサイズはSkillSelectSystemが書く

                Tsukino::BuiltIn::ECS::FontComponent& font = registry.AddComponent<Tsukino::BuiltIn::ECS::FontComponent>(textEntity);
                font.text            = L"";    // 空文字の間はFontRendererSystemが描画しない
                font.outlineColor    = hlslpp::float4(0.0f, 0.0f, 0.0f, 1.0f);
                font.outlineWidth    = outlineWidth;
                font.horizontalAlign = horizontalAlign;
                font.verticalAlign   = Tsukino::BuiltIn::ECS::VerticalAlign::Middle;
                font.sortOrder       = CombatAndroid::UI::kSkillSelectText;    // カード背景より手前

                return textEntity;
            };

            CombatAndroid::ECS::SkillSelectComponent& skillSelect =
                registry.AddComponent<CombatAndroid::ECS::SkillSelectComponent>(playerEntity);
            skillSelect.backdropEntity  = makeSkillPanelSprite(CombatAndroid::UI::kSkillSelectBackdrop);     // 画面全体の暗転
            skillSelect.highlightEntity = makeSkillPanelSprite(CombatAndroid::UI::kSkillSelectHighlight);    // 選択中カードの強調枠（カードの奥に敷いて縁に見せる）
            skillSelect.titleEntity     = makeSkillText(Tsukino::BuiltIn::ECS::HorizontalAlign::Center, 3.0f);

            for(CombatAndroid::ECS::SkillSelectCardEntities& card : skillSelect.cards) {
                card.panelEntity = makeSkillPanelSprite(CombatAndroid::UI::kSkillSelectCard);
                card.nameEntity  = makeSkillText(Tsukino::BuiltIn::ECS::HorizontalAlign::Left, 3.0f);
                card.descEntity  = makeSkillText(Tsukino::BuiltIn::ECS::HorizontalAlign::Left, 2.0f);
            }
        }

#ifdef _DEBUG
        //--------------------------------------------------------------
        // 武器の握り位置・角度を調整するデバッグHUD用エンティティ（F6で調整モードON時のみ表示）。
        // 上の「Fキーで拾う」ラベルと同じ作り。WeaponGripDebugSystemがtextを毎フレーム書き換える
        //--------------------------------------------------------------
        Tsukino::ECS::Entity weaponGripDebugHudEntity = m_scene.CreateEntity();

        Tsukino::BuiltIn::ECS::TransformComponent& gripHudTransform =
            registry.AddComponent<Tsukino::BuiltIn::ECS::TransformComponent>(weaponGripDebugHudEntity);
        gripHudTransform.position = hlslpp::float3(10.0f, 10.0f, 0.0f);    // 画面左上（生スクリーンピクセル座標）
        gripHudTransform.scale    = hlslpp::float3(1.0f, 1.0f, 1.0f);
        gripHudTransform.dirty    = true;

        Tsukino::BuiltIn::ECS::FontComponent& gripHudFont =
            registry.AddComponent<Tsukino::BuiltIn::ECS::FontComponent>(weaponGripDebugHudEntity);
        gripHudFont.text      = L"";    // 空文字の間はFontRendererSystemが描画しない
        gripHudFont.color     = hlslpp::float4(1.0f, 1.0f, 0.3f, 1.0f);
        gripHudFont.origin    = hlslpp::float2(0.0f, 0.0f);
        gripHudFont.sortOrder = CombatAndroid::UI::kDebugWeaponGripHud;    // 調査用HUDなので暗転板等より常に手前

        registry.AddComponent<CombatAndroid::ECS::WeaponGripDebugComponent>(weaponGripDebugHudEntity);

        //--------------------------------------------------------------
        // 所持武器のレベルを表示するデバッグHUD用エンティティ。トグルキーは持たず、
        // 存在する間は常に表示する（上の握り調整HUDと重ならないよう少し下から始める）。
        // WeaponLevelDebugSystemがtextを毎フレーム書き換える
        //--------------------------------------------------------------
        {
            Tsukino::ECS::Entity weaponLevelDebugHudEntity = m_scene.CreateEntity();

            Tsukino::BuiltIn::ECS::TransformComponent& levelHudTransform =
                registry.AddComponent<Tsukino::BuiltIn::ECS::TransformComponent>(weaponLevelDebugHudEntity);
            levelHudTransform.position = hlslpp::float3(10.0f, 230.0f, 0.0f);    // 画面左上（生スクリーンピクセル座標）
            levelHudTransform.scale    = hlslpp::float3(1.0f, 1.0f, 1.0f);
            levelHudTransform.dirty    = true;

            Tsukino::BuiltIn::ECS::FontComponent& levelHudFont =
                registry.AddComponent<Tsukino::BuiltIn::ECS::FontComponent>(weaponLevelDebugHudEntity);
            levelHudFont.text      = L"";    // 空文字の間はFontRendererSystemが描画しない
            levelHudFont.color     = hlslpp::float4(1.0f, 0.8f, 0.3f, 1.0f);
            levelHudFont.origin    = hlslpp::float2(0.0f, 0.0f);
            levelHudFont.sortOrder = CombatAndroid::UI::kDebugWeaponLevelHud;    // 調査用HUDなので暗転板等より常に手前

            registry.AddComponent<CombatAndroid::ECS::WeaponLevelDebugComponent>(weaponLevelDebugHudEntity);
        }
#endif

#ifdef TSUKINO_ENABLE_STRESS_TEST
        //--------------------------------------------------------------
        // 負荷試験のHUD用エンティティ。上の握り調整HUDと同じ作りで、
        // EnemyStressTestSystemがtextを毎フレーム書き換える。
        // 握り調整HUD（左上）と重ならないよう少し下から始める
        //--------------------------------------------------------------
        {
            Tsukino::ECS::Entity stressTestHudEntity = m_scene.CreateEntity();

            Tsukino::BuiltIn::ECS::TransformComponent& hudTransform =
                registry.AddComponent<Tsukino::BuiltIn::ECS::TransformComponent>(stressTestHudEntity);
            hudTransform.position = hlslpp::float3(10.0f, 120.0f, 0.0f);    // 画面左上（生スクリーンピクセル座標）
            hudTransform.scale    = hlslpp::float3(1.0f, 1.0f, 1.0f);
            hudTransform.dirty    = true;

            Tsukino::BuiltIn::ECS::FontComponent& hudFont = registry.AddComponent<Tsukino::BuiltIn::ECS::FontComponent>(stressTestHudEntity);
            hudFont.text                                  = L"";    // 空文字の間はFontRendererSystemが描画しない
            hudFont.color                                 = hlslpp::float4(0.4f, 1.0f, 0.6f, 1.0f);
            hudFont.origin                                = hlslpp::float2(0.0f, 0.0f);
            hudFont.sortOrder                             = CombatAndroid::UI::kDebugStressTestHud;    // 調査用HUDなので暗転板等より常に手前

            registry.AddComponent<CombatAndroid::ECS::EnemyStressTestComponent>(stressTestHudEntity);
        }
#endif

        //--------------------------------------------------------------
        // TPS（三人称視点）カメラエンティティの生成
        // プレイヤーの背後に追従するメインカメラ（isPrimary = true）
        //--------------------------------------------------------------
        {
            Tsukino::ECS::Entity tpsCameraEntity = m_scene.CreateEntity();

            Tsukino::BuiltIn::ECS::TransformComponent& tpsCamTransform = registry.AddComponent<Tsukino::BuiltIn::ECS::TransformComponent>(tpsCameraEntity);
            tpsCamTransform.position                                   = playerTransform.position + hlslpp::float3(0.0f, 200.0f, -400.0f);
            tpsCamTransform.dirty                                      = true;

            Tsukino::BuiltIn::ECS::CameraComponent& tpsCam = registry.AddComponent<Tsukino::BuiltIn::ECS::CameraComponent>(tpsCameraEntity);
            tpsCam.projectionType                          = Tsukino::BuiltIn::ECS::CameraComponent::ProjectionType::Perspective;
            tpsCam.fov                                     = 60.0f;
            tpsCam.nearZ                                   = 0.3f;
            tpsCam.farZ                                    = 2000.0f;
            tpsCam.useLookAt                               = true;
            tpsCam.lookAtTarget                            = playerTransform.position;
            tpsCam.isPrimary                               = true;

            CombatAndroid::ECS::TpsCameraComponent& tpsCameraComponent = registry.AddComponent<CombatAndroid::ECS::TpsCameraComponent>(tpsCameraEntity);
            tpsCameraComponent.target                               = playerEntity;

            //----------------------------------------------------------
            // モーションブラー（オブジェクト速度バッファ方式）
            // このコンポーネントを外せばモーションブラーごと無効になる。
            // strengthはAttackMotionBlurSystemが攻撃の進行度に応じて毎フレーム上書きする。
            //----------------------------------------------------------
            Tsukino::BuiltIn::ECS::MotionBlurComponent& motionBlur = registry.AddComponent<Tsukino::BuiltIn::ECS::MotionBlurComponent>(tpsCameraEntity);
            motionBlur.maxBlurRadius                               = 0.03f;
            motionBlur.sampleCount                                 = 8;
        }

        //--------------------------------------------------------------
        // デバッグカメラエンティティの生成 (デバッグビルドのみ)
        //--------------------------------------------------------------
#ifdef _DEBUG
        {
            Tsukino::ECS::Entity debugCamEntity = m_scene.CreateEntity();

            // 1ユニット≒1cm規約。身長約210のキャラクターを斜め上から見下ろす位置に置く
            Tsukino::BuiltIn::ECS::TransformComponent& t = registry.AddComponent<Tsukino::BuiltIn::ECS::TransformComponent>(debugCamEntity);
            t.position                                   = hlslpp::float3(0.0f, 180.0f, 300.0f);
            t.rotation                                   = hlslpp::quaternion(0.0f, 0.0f, 0.0f, 1.0f);
            t.dirty                                      = true;

            Tsukino::BuiltIn::ECS::CameraComponent& cam = registry.AddComponent<Tsukino::BuiltIn::ECS::CameraComponent>(debugCamEntity);
            cam.lookAtTarget                            = hlslpp::float3(0.0f, 100.0f, 0.0f);
            cam.nearZ                                   = 1.0f;
            cam.farZ                                    = 10000.0f;
            cam.isPrimary                               = false;

            Tsukino::BuiltIn::ECS::DebugCameraComponent& debug = registry.AddComponent<Tsukino::BuiltIn::ECS::DebugCameraComponent>(debugCamEntity);
            debug.moveSpeed                                    = 1.0f;

            registry.AddComponent<Tsukino::BuiltIn::ECS::DebugCameraTag>(debugCamEntity);
        }
#endif

        //--------------------------------------------------------------
        // ディレクショナルライトエンティティの生成
        //--------------------------------------------------------------
        {
            Tsukino::ECS::Entity                              lightEntity = m_scene.CreateEntity();
            Tsukino::BuiltIn::ECS::DirectionalLightComponent& light = registry.AddComponent<Tsukino::BuiltIn::ECS::DirectionalLightComponent>(lightEntity);
            light.direction                                         = hlslpp::float3(0.0f, -0.5f, -1.0f);
            light.color                                             = hlslpp::float3(1.0f, 1.0f, 1.0f);
            light.intensity                                         = 5.0f;
            light.castShadow                                        = true;
        }

        //--------------------------------------------------------------
        // 点光源・スポットライトの生成（ディファードの多光源デモ）
        //
        // ディレクショナルライトと違い TransformComponent が必須。
        // LightSystem の View が <TransformComponent, PointLight/SpotLight> のため、
        // Transform を付け忘れると収集されず、何も光らない。
        // また worldMatrix から位置を取るので dirty=true を立てて
        // TransformSystem に初回計算をさせること。
        //--------------------------------------------------------------
        {
            struct PointLightSpec {
                hlslpp::float3 position;
                hlslpp::float3 color;
                float          intensity;
                float          range;
            };

            // プレイヤーとゾンビが戦う地面付近を、暖色と寒色で挟むように配置する。
            //
            // intensityの単位に注意：減衰が物理的な逆二乗（intensity / (d^2 + 1)）なので、
            // 到達させたい放射輝度に「距離の二乗」を掛けた値が必要になる。
            // このシーンは1ユニット≒1cm相当（プレイヤー身長が約210ユニット）で
            // ライトまでの距離が150前後になるため d^2 ≒ 22500。
            // つまり radiance を 3 程度にしたければ intensity は 7万前後が必要。
            // ディレクショナルライト（intensity=5）と桁が違うのは距離減衰の有無によるもの。
            const PointLightSpec pointSpecs[] = {
                {hlslpp::float3(160.0f, 60.0f, 90.0f), hlslpp::float3(1.0f, 0.45f, 0.15f), 45000.0f, 700.0f},     // 焚き火（橙）
                {hlslpp::float3(-170.0f, 60.0f, 60.0f), hlslpp::float3(0.25f, 0.5f, 1.0f), 45000.0f, 700.0f},     // 月明かり寄り（青）
                {hlslpp::float3(0.0f, 90.0f, -190.0f), hlslpp::float3(0.6f, 1.0f, 0.4f), 40000.0f, 700.0f},       // 背後（緑）
                {hlslpp::float3(0.0f, 40.0f, 170.0f), hlslpp::float3(1.0f, 0.2f, 0.35f), 35000.0f, 600.0f},       // 手前（赤）
            };

            for(const auto& spec : pointSpecs) {
                Tsukino::ECS::Entity                       e         = m_scene.CreateEntity();
                Tsukino::BuiltIn::ECS::TransformComponent& transform = registry.AddComponent<Tsukino::BuiltIn::ECS::TransformComponent>(e);
                transform.position                                   = spec.position;
                transform.rotation                                   = hlslpp::quaternion(0.0f, 0.0f, 0.0f, 1.0f);
                transform.scale                                      = hlslpp::float3(1.0f, 1.0f, 1.0f);
                transform.dirty                                      = true;          // 初回計算のためフラグを立てる
                transform.parent                                     = entt::null;    // 親なし

                Tsukino::BuiltIn::ECS::PointLightComponent& light = registry.AddComponent<Tsukino::BuiltIn::ECS::PointLightComponent>(e);
                light.color                                      = spec.color;
                light.intensity                                  = spec.intensity;
                light.range                                      = spec.range;
                light.enabled                                    = true;
            }

            //----------------------------------------------------------
            // スポットライト：真上からプレイヤー付近を照らす
            // 向きはエンティティのローカル+Z。X軸に+90度回すと+Zが真下(-Y)を向く
            //----------------------------------------------------------
            {
                Tsukino::ECS::Entity                       e         = m_scene.CreateEntity();
                Tsukino::BuiltIn::ECS::TransformComponent& transform = registry.AddComponent<Tsukino::BuiltIn::ECS::TransformComponent>(e);
                transform.position                                   = hlslpp::float3(0.0f, 300.0f, 0.0f);
                transform.rotation                                   = hlslpp::quaternion::rotation_x(1.5708f);
                transform.scale                                      = hlslpp::float3(1.0f, 1.0f, 1.0f);
                transform.dirty                                      = true;
                transform.parent                                     = entt::null;

                Tsukino::BuiltIn::ECS::SpotLightComponent& light = registry.AddComponent<Tsukino::BuiltIn::ECS::SpotLightComponent>(e);
                light.color                                     = hlslpp::float3(1.0f, 0.95f, 0.85f);
                light.intensity                                 = 120000.0f;    // 距離300から照らすため d^2=90000 を見込む
                light.range                                     = 900.0f;
                light.innerConeDeg                              = 18.0f;
                light.outerConeDeg                              = 32.0f;
                light.enabled                                   = true;
            }
        }

        //--------------------------------------------------------------
        // スカイアトモスフィアエンティティの生成
        //--------------------------------------------------------------
        {
            Tsukino::ECS::Entity skyEntity = m_scene.CreateEntity();
            registry.AddComponent<Tsukino::BuiltIn::ECS::SkyAtmosphereComponent>(skyEntity);
        }

        //--------------------------------------------------------------
        // フォグエンティティの生成
        //
        // FogComponentのデフォルトは「1ユニット = 1m」想定なので、
        // このシーンのスケール（TPSカメラがプレイヤーの400ユニット後方、
        // 200ユニット上）に合わせて距離系のパラメータを入れ直す。
        //--------------------------------------------------------------
        {
            Tsukino::ECS::Entity fogEntity = m_scene.CreateEntity();
            auto&                fog       = registry.AddComponent<Tsukino::BuiltIn::ECS::FogComponent>(fogEntity);

            // 距離フォグ：戦闘範囲（〜500）は素通しで、そこから奥を徐々に霞ませる。
            // density / heightDensityはEnemySpawnDirectorSystemの湧き半径（900〜1300）が
            // 確実に隠れるよう、既定値（0.00030 / 0.00050）から引き上げてある
            fog.color         = hlslpp::float3(0.55f, 0.60f, 0.65f);
            fog.density       = 0.00060f;
            fog.startDistance = 500.0f;
            fog.maxOpacity    = 1.0f;

            // 高さフォグ：地面（y = -5）付近に溜め、カメラの高さ（y ≒ 205）では薄くする
            fog.heightFogEnabled = true;
            fog.height           = 0.0f;
            fog.heightFalloff    = 0.004f;
            fog.heightDensity    = 0.00070f;

            // 太陽方向の前方散乱
            fog.sunColor        = hlslpp::float3(1.0f, 0.85f, 0.65f);
            fog.sunScatterPower = 8.0f;

            // ノイズ：700ユニット程度の塊がゆっくり流れる
            fog.noiseEnabled   = true;
            fog.noiseScale     = 0.0015f;
            fog.noiseIntensity = 0.50f;
            fog.windDirection  = hlslpp::float3(1.0f, 0.0f, 0.3f);
            fog.windSpeed      = 60.0f;
        }
    }

    //-------------------------------------------------------------
    //! @brief  シーンの更新
    //-------------------------------------------------------------
    void CombatAndroidScene::OnUpdate(Tsukino::EngineIntegration::EngineAPI& api, float deltaTime) {
        // ヒットストップはHitStopComponent/HitStopSystemによりエンティティ単位（プレイヤーと
        // ヒットに関与した敵だけ）で処理されるため、ここでシーン全体のdeltaTimeを縮小することはしない

        float scaledDeltaTime = deltaTime;

        //--------------------------------------------------------------
        // スキル選択メニュー表示中は時間を完全に止める。Sceneへ渡すdeltaTimeそのものを0にする
        // ことで、敵AI・アニメーション・湧きディレクター・EXP玉・生存時間まで一律に停止する
        // （ヒットストップと異なり、こちらは意図的な画面全体の停止）。
        //
        // ただしPhysicsSystemだけはdeltaTimeが0以下でも1/60秒ぶんステップしてしまうため、
        // これだけではCharacterVirtualが滑り続ける。移動入力の打ち消しと、
        // プレイヤー入力の遮断はSkillSelectSystem側で行っている
        //--------------------------------------------------------------
        if(CombatAndroid::ECS::IsSkillSelectActive(m_scene.GetRegistry()))
            scaledDeltaTime = 0.0f;

        m_scene.Update(scaledDeltaTime);
    }


    //-------------------------------------------------------------
    //! @brief  シーンの終了処理
    //-------------------------------------------------------------
    void CombatAndroidScene::OnExit() {
        // シーン終了時の解放処理などが必要な場合はここに記述します
    }

}    // namespace CombatAndroid
