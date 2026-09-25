//-------------------------------------------------------------
//! @file    CombatAndroidScene.cpp
//! @brief   CombatAndroidのメインゲームシーンの実装
//! @author  山﨑愛
//-------------------------------------------------------------
#include <CombatAndroid/Scene/CombatAndroidScene.hpp>

// 条件付きインクルードより先に読む必要がある（TSUKINO_ENABLE_STRESS_TEST等の定義元）
#include <Tsukino/Core/DebugTools/DebugFeatures.hpp>

#include <CombatAndroid/ECS/Component/PlayerComponent.hpp>
#include <CombatAndroid/ECS/Component/GroundFollowComponent.hpp>
#include <CombatAndroid/ECS/Component/HealthComponent.hpp>
#include <CombatAndroid/ECS/Component/WeaponComponent.hpp>
#include <CombatAndroid/ECS/Component/EnemyComponent.hpp>
#include <CombatAndroid/ECS/Component/TpsCameraComponent.hpp>
#include <CombatAndroid/ECS/Utility/WorldTimeContext.hpp>
#include <CombatAndroid/ECS/Component/PlayerAnimationSetComponent.hpp>
#include <CombatAndroid/ECS/Component/PickupComponent.hpp>
#include <CombatAndroid/ECS/Component/InputPromptHudComponent.hpp>
#include <CombatAndroid/ECS/Component/DamageNumberComponent.hpp>
#include <CombatAndroid/ECS/Component/BehaviorTreeComponent.hpp>
#include <CombatAndroid/ECS/Component/EnemyAnimationSetComponent.hpp>
#include <CombatAndroid/ECS/Component/EnemyAttackHitboxComponent.hpp>
#include <CombatAndroid/ECS/Component/ExpOrbComponent.hpp>
#include <CombatAndroid/ECS/Component/PlayerExperienceComponent.hpp>
#include <CombatAndroid/ECS/Component/PlayerHudComponent.hpp>
#include <CombatAndroid/ECS/Component/RunClockComponent.hpp>
#include <CombatAndroid/ECS/Component/PlayerDamageEffectComponent.hpp>
#include <CombatAndroid/ECS/Component/PauseMenuComponent.hpp>
#include <CombatAndroid/ECS/System/TutorialSystem.hpp>
#include <CombatAndroid/ECS/Component/RunResultComponent.hpp>
#include <CombatAndroid/ECS/Component/PlayerSkillComponent.hpp>
#include <CombatAndroid/ECS/Component/PlayerSkillHudComponent.hpp>
#include <CombatAndroid/ECS/Component/SkillSelectComponent.hpp>
#include <CombatAndroid/ECS/Component/GameLogComponent.hpp>
#include <CombatAndroid/UI/UiSortOrder.hpp>
#include <CombatAndroid/ECS/AI/ZombieBehavior.hpp>
#include <CombatAndroid/ECS/Utility/EnemySpawner.hpp>
#include <CombatAndroid/ECS/Utility/WeaponSpawner.hpp>
#include <CombatAndroid/ECS/Utility/AssetPreloader.hpp>
#include <CombatAndroid/ECS/Utility/Bgm.hpp>
#include <CombatAndroid/ECS/Utility/GamePrefab.hpp>

#include <Tsukino/Engine/ECS/Prefab/PrefabFactory.hpp>
#include <CombatAndroid/ECS/Utility/GameplayFreeze.hpp>
#include <CombatAndroid/ECS/Utility/ScreenFade.hpp>
#include <CombatAndroid/ECS/Utility/UiSprite.hpp>
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
// context->renderer を触るため（以前はシステム系ヘッダ経由で間接的に入っていた）
#include <Tsukino/Renderer/Renderer.hpp>


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
#include <Tsukino/BuiltIn/ECS/Component/SkyAtmosphereComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/FogComponent.hpp>
#include <CombatAndroid/ECS/Component/GrassFieldComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/AmbientParticleComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/MotionBlurComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/SpringBoneComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/DebugCameraComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/DebugCameraTag.hpp>
#include <Tsukino/BuiltIn/ECS/Component/EffectComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/RimGlowComponent.hpp>

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
        // 実行順の理由は CombatAndroid/ECS/SystemPriority.hpp、
        // 登録そのものは CombatAndroidSceneSystems.cpp に置いている
        //--------------------------------------------------------------
        RegisterSystems(context, eventBus);


        //--------------------------------------------------------------
        // アセットのロード
        //--------------------------------------------------------------

        // 武器・敵・スキル・システム単体の各種アセットをここでまとめて読み込んでおく。
        // AssetManager::Load はパスでキャッシュされるため、以降の「使う瞬間に遅延ロードする」
        // 既存コードはキャッシュヒットになり、戦闘中や初回表示のタイミングで
        // 重いインポート処理が走らなくなる（詳細はAssetPreloader.hppのコメント参照）
        CombatAndroid::ECS::PreloadAssets(*context);

        // 戦闘中のBGM。素材が置かれていなければ無音のまま進む（Assets/Audio/README.md）
        CombatAndroid::ECS::PlayBgm(*context, CombatAndroid::ECS::kBattleBgmPath);


        Tsukino::ECS::Registry& registry = m_scene.GetRegistry();

        //--------------------------------------------------------------
        // 環境（地面・光・空・フォグ・環境パーティクル・草）。値は Assets/Prefabs/Environment/ 以下のPrefab JSON（README参照）
        //--------------------------------------------------------------
        CombatAndroid::ECS::InstantiateEnvironment(registry, *context, "Combat");

        //--------------------------------------------------------------
        // プレイヤーエンティティ生成（Prefab: Player）。
        // Transform（position＝カプセル底面＝足元）・CharacterController・センサー用のRigidbody/Collision・
        // Player・Health・Model・RimGlow・AnimationPlayer・PlayerAnimationSet（クリップと連撃の各段）・SpringBone は
        // Assets/Prefabs/Player/ のJSONにある。PlayerExperience / PlayerSkill / AnimationController / SkeletonOutput は
        // 既定値のままアタッチするだけ。実体が要る結線（最初のクリップ・武器）だけここで行う
        //--------------------------------------------------------------
        Tsukino::ECS::Entity playerEntity = context->prefabFactory->Instantiate("CombatAndroid/Assets/Prefabs/Player/Prefab.json", registry);

        Tsukino::BuiltIn::ECS::TransformComponent&       playerTransform = registry.GetComponent<Tsukino::BuiltIn::ECS::TransformComponent>(playerEntity);
        CombatAndroid::ECS::PlayerComponent&             player          = registry.GetComponent<CombatAndroid::ECS::PlayerComponent>(playerEntity);
        CombatAndroid::ECS::PlayerAnimationSetComponent& animSet         = registry.GetComponent<CombatAndroid::ECS::PlayerAnimationSetComponent>(playerEntity);

        // 最初に再生するクリップは待機（以後はPlayerAnimationSystemが管理する）
        registry.GetComponent<Tsukino::BuiltIn::ECS::AnimationPlayerComponent>(playerEntity).current_clip_id = animSet.idleClip;

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
        // その場に留まる。PickupSystemが範囲内・最近傍の1本だけを強調し、Fキーで
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
        // 敵はここでは置かない。実行中の湧き（サバイバー化）は EnemySpawnDirectorSystem が担当し、
        // プレイヤーから離れた位置にだけ湧かせる（開始直後に近くへ敵がいる体験を避けるため）
        //--------------------------------------------------------------

        //--------------------------------------------------------------
        // 画面固定UI用の2Dカメラ（Prefab: UiCamera2D）
        //--------------------------------------------------------------
        CombatAndroid::ECS::InstantiateUiCamera2D(registry, *context);

        //--------------------------------------------------------------
        // 操作を促すUI（キーキャップ・マウス・矢印・長押しゲージ）一式の生成。
        // ダメージ数値やHPバーと同じく毎フレーム生成せず、ここで作った束を
        // InputPromptSystemが表示/非表示と値の更新だけで使い回す。
        // 実際に付くコンポーネントはプレイヤーエンティティの生成後（下のAddComponent）
        //--------------------------------------------------------------
        CombatAndroid::ECS::InputPromptHudComponent inputPromptHud;
        {
            // 拾う：[F] ＋ 拾い上げる向きの矢印 ＋ 対象名。対象の頭上へワールド追従
            CombatAndroid::ECS::InputPromptDesc pickupDesc;
            pickupDesc.keyLabel      = L"F";
            pickupDesc.chevron       = CombatAndroid::ECS::PromptChevron::Up;
            pickupDesc.useCaption    = true;
            pickupDesc.worldAnchored = true;
            pickupDesc.sortOrderBase = CombatAndroid::UI::kInputPromptBase;
            inputPromptHud.pickupPrompt = CombatAndroid::ECS::CreateInputPromptWidget(registry, *context, pickupDesc);

            // 溜め攻撃：マウスの絵を長押しゲージが囲む。プレイヤーの頭上へワールド追従
            CombatAndroid::ECS::InputPromptDesc chargeDesc;
            chargeDesc.useMouse      = true;
            chargeDesc.useHoldRing   = true;
            chargeDesc.worldAnchored = true;
            chargeDesc.sortOrderBase = CombatAndroid::UI::kInputPromptBase;
            inputPromptHud.chargePrompt = CombatAndroid::ECS::CreateInputPromptWidget(registry, *context, chargeDesc);

            //--------------------------------------------------------------
            // 以下3つはスキル選択メニューの上に重ねるので画面固定＋モーダル用の層を使う
            //--------------------------------------------------------------
            CombatAndroid::ECS::InputPromptDesc skillUpDesc;
            skillUpDesc.keyLabel      = L"W";
            skillUpDesc.chevron       = CombatAndroid::ECS::PromptChevron::Up;
            skillUpDesc.sortOrderBase = CombatAndroid::UI::kModalInputPromptBase;
            inputPromptHud.skillUpPrompt = CombatAndroid::ECS::CreateInputPromptWidget(registry, *context, skillUpDesc);

            CombatAndroid::ECS::InputPromptDesc skillDownDesc;
            skillDownDesc.keyLabel      = L"S";
            skillDownDesc.chevron       = CombatAndroid::ECS::PromptChevron::Down;
            skillDownDesc.sortOrderBase = CombatAndroid::UI::kModalInputPromptBase;
            inputPromptHud.skillDownPrompt = CombatAndroid::ECS::CreateInputPromptWidget(registry, *context, skillDownDesc);

            CombatAndroid::ECS::InputPromptDesc skillConfirmDesc;
            skillConfirmDesc.keyLabel      = L"F";
            skillConfirmDesc.chevron       = CombatAndroid::ECS::PromptChevron::Right;
            skillConfirmDesc.sortOrderBase = CombatAndroid::UI::kModalInputPromptBase;
            inputPromptHud.skillConfirmPrompt = CombatAndroid::ECS::CreateInputPromptWidget(registry, *context, skillConfirmDesc);
        }

        // InputPromptSystemはPlayerComponentと同じエンティティに付いた束を引く
        registry.AddComponent<CombatAndroid::ECS::InputPromptHudComponent>(playerEntity, inputPromptHud);

        //--------------------------------------------------------------
        // ダメージ数値用エンティティのプール。「Fキーで拾う」ラベルと同じく毎フレーム生成せず
        // 固定数を使い回す。WeaponHitEventはCombatSystemのview.eachの内側からPublishされるため、
        // そのハンドラでエンティティを生成するとEnTTのイテレータが壊れる。
        // DamageNumberSystemはここで作ったスロットの空きを探して再利用する
        //--------------------------------------------------------------
        //
        // Prefab（UI/DamageNumber）：スケール0（未使用スロットは非表示）・空文字の文字＋WorldAnchor
        // （以後DamageNumberSystemがfixedWorldPositionを使う）
        //--------------------------------------------------------------
        for(int i = 0; i < CombatAndroid::ECS::kDamageNumberPoolSize; ++i)
            (void)CombatAndroid::ECS::InstantiatePrefab(registry, *context, "UI/DamageNumber");

        //--------------------------------------------------------------
        // EXP玉用エンティティのプール。ダメージ数値と同じく毎フレーム生成せず固定数を使い回す。
        // EnemyDiedEventはビヘイビアツリーのアクション（View反復中）からPublishされるため、
        // ExpOrbSystemはここで作ったスロットの空きを探して再利用する。
        //
        // Prefab（UI/ExpOrb）：3Dワールド上を落下・飛行する演出のため、WorldAnchorComponent（画面固定UI用）は使わず、
        // SpriteComponent.space=Worldでpositionを直接3D座標として扱い、主カメラを向く
        // ビルボードとして深度テストされる形で描画する（敵の後ろに回ったら正しく隠れる）。加算合成で発光して見せる
        //--------------------------------------------------------------
        for(int i = 0; i < CombatAndroid::ECS::kExpOrbPoolSize; ++i)
            (void)CombatAndroid::ECS::InstantiatePrefab(registry, *context, "UI/ExpOrb");

        //--------------------------------------------------------------
        // 画面左上のプレイヤーHP/EXPバー。WorldAnchorComponentは使わず固定ピクセル座標に置き、
        // PlayerHudSystemが毎フレームHealthComponent/PlayerExperienceComponentの値へ合わせて更新する
        //--------------------------------------------------------------
        {
            // Prefab（UI/HudBar）：白い1ピクセル。実際の位置・スケール・色はPlayerHudSystemが毎フレーム書く。
            // 描画順（背景／残量／スキルアイコン）だけ個体ごとに変える
            auto makeBarSprite = [&](int sortOrder) {
                Tsukino::ECS::Entity barEntity = CombatAndroid::ECS::InstantiatePrefab(registry, *context, "UI/HudBar");
                registry.GetComponent<Tsukino::BuiltIn::ECS::SpriteComponent>(barEntity).sortOrder = sortOrder;
                return barEntity;
            };

            // Prefab（UI/HudText）：白文字・黒縁取り・バーより手前
            auto makeHudText = [&]() { return CombatAndroid::ECS::InstantiatePrefab(registry, *context, "UI/HudText"); };

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

            // Prefab（UI/HudTopText）：中央・上揃えの白文字
            Tsukino::ECS::Entity survivalTimeEntity = CombatAndroid::ECS::InstantiatePrefab(registry, *context, "UI/HudTopText");

            Tsukino::BuiltIn::ECS::TransformComponent& survivalTimeTransform =
                registry.GetComponent<Tsukino::BuiltIn::ECS::TransformComponent>(survivalTimeEntity);
            survivalTimeTransform.position = hlslpp::float3(survivalTimeScreenCenterX, 24.0f, 0.0f);
            survivalTimeTransform.dirty    = true;

            hud.survivalTimeTextEntity = survivalTimeEntity;

            //-------------------------------------------------------------
            // 生存時間のすぐ下に置く危険度テキスト。時間経過で敵が強くなることを
            // 見せておかないと、難易度上昇が原因不明の理不尽になる。
            // 横並びにしないのは、"12:34" の描画幅を知らないと重なるため
            //-------------------------------------------------------------
            //! 生存時間テキストの上端からの縦オフセット（ピクセル）。フォントの行高ぶん下げる
            constexpr float kDangerRankTextOffsetY = 34.0f;

            Tsukino::ECS::Entity dangerRankEntity = CombatAndroid::ECS::InstantiatePrefab(registry, *context, "UI/HudTopText");

            Tsukino::BuiltIn::ECS::TransformComponent& dangerRankTransform =
                registry.GetComponent<Tsukino::BuiltIn::ECS::TransformComponent>(dangerRankEntity);
            dangerRankTransform.position = hlslpp::float3(survivalTimeScreenCenterX, 24.0f + kDangerRankTextOffsetY, 0.0f);
            dangerRankTransform.dirty    = true;

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
        //
        // Prefab：UI/GameLogPanel（パネル＋GameLogComponent）・UI/GameLogAccent（種別色バー）・
        // UI/GameLogText（左揃えの文字。パネル・バーより手前）
        //--------------------------------------------------------------
        for(int i = 0; i < CombatAndroid::ECS::kGameLogPoolSize; ++i) {
            // 4つとも作り切ってから結ぶ。生成の途中で得た参照は格納先が動いて無効になり得る
            Tsukino::ECS::Entity panelEntity  = CombatAndroid::ECS::InstantiatePrefab(registry, *context, "UI/GameLogPanel");
            Tsukino::ECS::Entity accentEntity = CombatAndroid::ECS::InstantiatePrefab(registry, *context, "UI/GameLogAccent");
            Tsukino::ECS::Entity labelEntity  = CombatAndroid::ECS::InstantiatePrefab(registry, *context, "UI/GameLogText");
            Tsukino::ECS::Entity textEntity   = CombatAndroid::ECS::InstantiatePrefab(registry, *context, "UI/GameLogText");

            CombatAndroid::ECS::GameLogComponent& gameLog = registry.GetComponent<CombatAndroid::ECS::GameLogComponent>(panelEntity);
            gameLog.accentEntity                          = accentEntity;
            gameLog.labelEntity                           = labelEntity;
            gameLog.textEntity                            = textEntity;
        }

        //--------------------------------------------------------------
        // 被弾演出（点滅・画面フラッシュ）とGAME OVER表示。
        // 画面フラッシュは画面全体を覆う単色スプライト（Prefab: UI/ScreenFlash。初期状態は透明）で、
        // 位置・サイズはPlayerDamageEffectSystemが毎フレーム画面サイズに合わせて書く
        //--------------------------------------------------------------
        {
            Tsukino::ECS::Entity screenFlashEntity = CombatAndroid::ECS::InstantiatePrefab(registry, *context, "UI/ScreenFlash");

            CombatAndroid::ECS::PlayerDamageEffectComponent& damageEffect =
                registry.AddComponent<CombatAndroid::ECS::PlayerDamageEffectComponent>(playerEntity);
            damageEffect.screenFlashEntity = screenFlashEntity;

            //-------------------------------------------------------------
            // 走行の終わり（死亡・クリア）のリザルトと、Escで開くポーズメニュー。
            // どちらも全て非表示で作っておき、RunResultSystem / PauseMenuSystem が
            // 表示のたびにウィンドウサイズから位置を計算して文言を書き込む
            //-------------------------------------------------------------
            CombatAndroid::ECS::RunResultComponent& runResult = registry.AddComponent<CombatAndroid::ECS::RunResultComponent>(playerEntity);
            runResult.backdropEntity = CombatAndroid::ECS::CreateUiRectEntity(registry, *context, CombatAndroid::UI::kRunResultBackdrop);
            runResult.titleEntity =
                CombatAndroid::ECS::CreateUiTextEntity(registry, CombatAndroid::UI::kRunResultText, CombatAndroid::ECS::UiTextAlign::Center);
            for(CombatAndroid::ECS::RunResultStatRow& row : runResult.statRows) {
                row.labelEntity =
                    CombatAndroid::ECS::CreateUiTextEntity(registry, CombatAndroid::UI::kRunResultText, CombatAndroid::ECS::UiTextAlign::Left);
                row.valueEntity =
                    CombatAndroid::ECS::CreateUiTextEntity(registry, CombatAndroid::UI::kRunResultText, CombatAndroid::ECS::UiTextAlign::Left);
                row.recordEntity =
                    CombatAndroid::ECS::CreateUiTextEntity(registry, CombatAndroid::UI::kRunResultText, CombatAndroid::ECS::UiTextAlign::Left);
            }
            runResult.skillsEntity =
                CombatAndroid::ECS::CreateUiTextEntity(registry, CombatAndroid::UI::kRunResultText, CombatAndroid::ECS::UiTextAlign::Center);
            runResult.bestEntity =
                CombatAndroid::ECS::CreateUiTextEntity(registry, CombatAndroid::UI::kRunResultText, CombatAndroid::ECS::UiTextAlign::Center);
            runResult.menu = CombatAndroid::ECS::CreateGameMenuWidget(registry, *context, CombatAndroid::UI::kRunResultMenuBase);

            CombatAndroid::ECS::PauseMenuComponent& pauseMenu = registry.AddComponent<CombatAndroid::ECS::PauseMenuComponent>(playerEntity);
            pauseMenu.backdropEntity = CombatAndroid::ECS::CreateUiRectEntity(registry, *context, CombatAndroid::UI::kPauseBackdrop);
            pauseMenu.titleEntity =
                CombatAndroid::ECS::CreateUiTextEntity(registry, CombatAndroid::UI::kPauseText, CombatAndroid::ECS::UiTextAlign::Center);
            pauseMenu.menu = CombatAndroid::ECS::CreateGameMenuWidget(registry, *context, CombatAndroid::UI::kPauseMenuBase);
            pauseMenu.options = CombatAndroid::ECS::CreateOptionsMenu(registry, *context, CombatAndroid::UI::kPauseOptionsBase);

            // 場面の切り替わりを繋ぐ黒。ロード画面の暗転を受けてここから明ける
            CombatAndroid::ECS::CreateScreenFade(registry, *context);

            // 操作の案内。タイトルの「はじめる」から来たときだけ出す（リトライでは出さない）
            if(m_showTutorial)
                CombatAndroid::ECS::CreateTutorial(registry, *context);

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
            // Prefab: UI/SkillPanel（スケール0＝非表示。背景は暫定でWhitePixelを入れてあり、
            // カードの背景はSkillSelectSystemがスキルテーブルのパスから差し替える）。描画順だけ個体ごとに上書きする
            auto makeSkillPanelSprite = [&](int sortOrder) {
                Tsukino::ECS::Entity panelEntity = CombatAndroid::ECS::InstantiatePrefab(registry, *context, "UI/SkillPanel");
                registry.GetComponent<Tsukino::BuiltIn::ECS::SpriteComponent>(panelEntity).sortOrder = sortOrder;
                return panelEntity;
            };

            // Prefab: UI/SkillText（空文字＝非表示。位置・フォントサイズはSkillSelectSystemが書く）。揃えと縁取りだけ上書きする
            auto makeSkillText = [&](Tsukino::BuiltIn::ECS::HorizontalAlign horizontalAlign, float outlineWidth) {
                Tsukino::ECS::Entity textEntity = CombatAndroid::ECS::InstantiatePrefab(registry, *context, "UI/SkillText");

                Tsukino::BuiltIn::ECS::FontComponent& font = registry.GetComponent<Tsukino::BuiltIn::ECS::FontComponent>(textEntity);
                font.outlineWidth    = outlineWidth;
                font.horizontalAlign = horizontalAlign;

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
        // Prefab: Debug/GripHud（画面左上の黄色いテキスト）。WeaponGripDebugSystemがtextを毎フレーム書き換える
        //--------------------------------------------------------------
        (void)CombatAndroid::ECS::InstantiatePrefab(registry, *context, "Debug/GripHud");

        //--------------------------------------------------------------
        // 所持武器のレベルを表示するデバッグHUD用エンティティ。トグルキーは持たず、
        // 存在する間は常に表示する（Prefab: Debug/LevelHud。握り調整HUDと重ならないよう少し下から始める）。
        // WeaponLevelDebugSystemがtextを毎フレーム書き換える
        //--------------------------------------------------------------
        (void)CombatAndroid::ECS::InstantiatePrefab(registry, *context, "Debug/LevelHud");
#endif

#ifdef TSUKINO_ENABLE_STRESS_TEST
        //--------------------------------------------------------------
        // 負荷試験のHUD用エンティティ（Prefab: Debug/StressHud）。上の握り調整HUDと同じ作りで、
        // EnemyStressTestSystemがtextを毎フレーム書き換える。
        // 握り調整HUD（左上）と重ならないよう少し下から始める
        //--------------------------------------------------------------
        (void)CombatAndroid::ECS::InstantiatePrefab(registry, *context, "Debug/StressHud");
#endif

        //--------------------------------------------------------------
        // TPS（三人称視点）カメラエンティティの生成
        // プレイヤーの背後に追従するメインカメラ（isPrimary = true）
        //--------------------------------------------------------------
        // Prefab: TpsCamera（Transform・Camera・TpsCamera・MotionBlur）。
        // MotionBlurComponentを外せばモーションブラーごと無効になる。strengthはAttackMotionBlurSystemが
        // 攻撃の進行度に応じて毎フレーム上書きする。追従対象だけは実体が要るのでここで結ぶ
        {
            Tsukino::ECS::Entity tpsCameraEntity = context->prefabFactory->Instantiate("CombatAndroid/Assets/Prefabs/TpsCamera/Prefab.json", registry);
            registry.GetComponent<CombatAndroid::ECS::TpsCameraComponent>(tpsCameraEntity).target = playerEntity;
        }

        //--------------------------------------------------------------
        // デバッグカメラエンティティの生成 (デバッグビルドのみ)
        //--------------------------------------------------------------
#ifdef _DEBUG
        // Prefab: Debug/Camera。1ユニット≒1cm規約で、身長約210のキャラクターを斜め上から見下ろす位置に置いてある
        (void)CombatAndroid::ECS::InstantiatePrefab(registry, *context, "Debug/Camera");
#endif
    }

    //-------------------------------------------------------------
    //! @brief  シーンの更新
    //-------------------------------------------------------------
    void CombatAndroidScene::OnUpdate(Tsukino::EngineIntegration::EngineAPI& api, float deltaTime) {
        // ヒットストップはHitStopComponent/HitStopSystemによりエンティティ単位（プレイヤーと
        // ヒットに関与した敵だけ）で処理されるため、ここでは縮小しない。

        //--------------------------------------------------------------
        // 大技のインパクトで世界の時間を遅くする（こちらは画面全体）。
        // Sceneへ渡すdeltaTimeそのものに倍率を掛けるので、アニメーション・敵AI・物理・
        // エフェクト・草・霧まで一律に遅くなる。スローの進行自体は実時間で進める
        //--------------------------------------------------------------
        float scaledDeltaTime = deltaTime * m_slowMotion.Advance(deltaTime);

        //--------------------------------------------------------------
        // メニュー（スキル選択・ポーズ・リザルト）の表示中は時間を完全に止める。Sceneへ渡す
        // deltaTimeそのものを0にすることで、敵AI・アニメーション・湧きディレクター・EXP玉・
        // 生存時間まで一律に停止する（ヒットストップと異なり、こちらは意図的な画面全体の停止）。
        //
        // ただしPhysicsSystemだけはdeltaTimeが0以下でも1/60秒ぶんステップしてしまうため、
        // これだけではCharacterVirtualが滑り続ける。移動入力の打ち消しと、
        // プレイヤー入力の遮断は各メニューのSystem側で行っている（GameplayFreeze.hpp）
        //--------------------------------------------------------------
        if(CombatAndroid::ECS::IsGameplayFrozen(m_scene.GetRegistry()))
            scaledDeltaTime = 0.0f;

        // スローに引きずられたくない演出（カメラの寄り）が実時間を読めるよう、Sceneを更新する前に置く
        m_scene.GetRegistry().SetContext<CombatAndroid::ECS::WorldTimeContext>(CombatAndroid::ECS::WorldTimeContext{
            deltaTime, (deltaTime > 0.0f) ? scaledDeltaTime / deltaTime : 1.0f});

        m_scene.Update(scaledDeltaTime);
    }


    //-------------------------------------------------------------
    //! @brief  シーンの終了処理
    //-------------------------------------------------------------
    void CombatAndroidScene::OnExit() {
        // BGMは明示的に止める。止めないとタイトルへ戻っても戦闘曲が鳴り続ける
        if(auto* context = m_scene.GetRegistry().GetContext<Tsukino::EngineIntegration::EngineContext*>())
            CombatAndroid::ECS::StopBgm(*context, CombatAndroid::ECS::kBattleBgmPath);
    }

}    // namespace CombatAndroid
