//-------------------------------------------------------------
//! @file   BossDirectorSystem.cpp
//! @brief  BossDirectorSystemクラスの実装
//-------------------------------------------------------------
#include <CombatAndroid/ECS/System/Progression/BossDirectorSystem.hpp>
#include <CombatAndroid/ECS/Serialization/Common/SerializationHelper.hpp>
#include <CombatAndroid/ECS/Utility/Table/TableJson.hpp>
#include <Tsukino/Core/Math/Serialization/HlslppSerialization.hpp>
#include <CombatAndroid/ECS/Component/Combat/HealthComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/CollisionComponent.hpp>
#include <CombatAndroid/ECS/AI/BossBehavior.hpp>
#include <CombatAndroid/ECS/Component/Enemy/BehaviorTreeComponent.hpp>
#include <CombatAndroid/ECS/Component/Enemy/BossComponent.hpp>
#include <CombatAndroid/ECS/Component/Enemy/BossMoveSetComponent.hpp>
#include <CombatAndroid/ECS/Component/Enemy/SpawnedEnemyComponent.hpp>
#include <CombatAndroid/ECS/Component/Combat/EnemyProjectileComponent.hpp>
#include <CombatAndroid/ECS/Utility/Asset/GamePrefab.hpp>
#include <CombatAndroid/ECS/Utility/Table/BossMoveTable.hpp>
#include <Tsukino/Engine/Asset/AssetManager.hpp>
#include <Tsukino/Core/Path.hpp>
#include <CombatAndroid/ECS/Component/Enemy/EliteEnemyComponent.hpp>
#include <CombatAndroid/ECS/Component/Enemy/EnemyAnimationSetComponent.hpp>
#include <CombatAndroid/ECS/Component/Enemy/EnemyHeldWeaponComponent.hpp>
#include <CombatAndroid/ECS/Event/Player/PlayerFinisherEvent.hpp>
#include <CombatAndroid/ECS/Component/Enemy/EnemyComponent.hpp>
#include <CombatAndroid/ECS/Component/Player/PlayerComponent.hpp>
#include <CombatAndroid/ECS/Component/Progression/BossProgressComponent.hpp>
#include <CombatAndroid/ECS/Component/Progression/RunClockComponent.hpp>
#include <CombatAndroid/ECS/Component/World/TpsCameraComponent.hpp>
#include <CombatAndroid/ECS/Event/Audio/SoundEvent.hpp>
#include <CombatAndroid/ECS/Event/UI/GameLogEvent.hpp>
#include <CombatAndroid/ECS/System/Menu/CutsceneSystem.hpp>
#include <CombatAndroid/ECS/System/World/NightTransitionSystem.hpp>
#include <CombatAndroid/ECS/Utility/Spawn/EnemySpawner.hpp>
#include <CombatAndroid/ECS/Utility/Table/EnemyDifficultyTable.hpp>
#include <CombatAndroid/ECS/Utility/Table/EnemySpawnTable.hpp>
#include <CombatAndroid/ECS/Utility/Time/GameplayFreeze.hpp>
#include <CombatAndroid/ECS/Utility/Time/WorldTimeContext.hpp>
#include <CombatAndroid/ECS/Utility/UI/UiMotion.hpp>
#include <CombatAndroid/ECS/Utility/UI/UiSprite.hpp>
#include <CombatAndroid/ECS/Utility/UI/UiTextSize.hpp>

#include <Tsukino/BuiltIn/ECS/Component/TransformComponent.hpp>
#include <Tsukino/Core/ECS/Event/EventBus.hpp>
#include <Tsukino/Core/Log.hpp>
#include <Tsukino/Core/Window.hpp>
#include <Tsukino/EngineIntegration/EngineContext.hpp>

#ifdef _DEBUG
#include <Tsukino/Core/Input/InputSystem.hpp>
#endif

#include <entt/entt.hpp>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    namespace {
        //-------------------------------------------------------------
        //! @struct BossEntryParams
        //! @brief  ボス1体ぶんの中身（既存の敵を大きく硬くしたもの）
        //-------------------------------------------------------------
        struct BossEntryParams {
            std::string  baseType       = "BigZombie";    //!< 元にする敵（EnemyTypeIdの綴り）
            std::string  prefab;                          //!< 使うPrefab（Assets/Prefabs/ からの相対名）。空なら元にする敵のPrefab
            std::string  behavior;                        //!< 技の表の名前（Assets/Tables/Bosses/<behavior>.json）。空なら雑魚と同じ行動
            std::wstring displayName    = L"巨躯の屍";     //!< 告知とログに出す呼び名（「ボス」のような種別名は出さない）
            float        sizeScale      = 1.8f;
            float        healthScale    = 25.0f;
            float        attackScale    = 1.5f;
            float        moveSpeedScale = 1.0f;
            float        expScale       = 10.0f;
        };

        template <class Archive>
        void load(Archive& archive, BossEntryParams& params) {
            LoadField(archive, "baseType", params.baseType);
            LoadField(archive, "prefab", params.prefab);
            LoadField(archive, "behavior", params.behavior);
            LoadWideField(archive, "displayName", params.displayName);
            LoadField(archive, "sizeScale", params.sizeScale);
            LoadField(archive, "healthScale", params.healthScale);
            LoadField(archive, "attackScale", params.attackScale);
            LoadField(archive, "moveSpeedScale", params.moveSpeedScale);
            LoadField(archive, "expScale", params.expScale);
        }

        //-------------------------------------------------------------
        //! @struct BossShotParams
        //! @brief  出現のカットシーン1ショットぶん（向きはプレイヤー側からボスを見る向きに決まる）
        //-------------------------------------------------------------
        struct BossShotParams {
            float distance       = 900.0f;
            float height         = 120.0f;
            float lookHeight     = 250.0f;
            float pitch          = 0.12f;
            float fov            = 55.0f;
            float duration       = 2.0f;
            float blendFrequency = 0.8f;
            float yawOffset      = 0.0f;    //!< プレイヤー側から見た向きからのずらし（ラジアン）
            float yawDriftSpeed  = 0.0f;
        };

        template <class Archive>
        void load(Archive& archive, BossShotParams& params) {
            LoadField(archive, "distance", params.distance);
            LoadField(archive, "height", params.height);
            LoadField(archive, "lookHeight", params.lookHeight);
            LoadField(archive, "pitch", params.pitch);
            LoadField(archive, "fov", params.fov);
            LoadField(archive, "duration", params.duration);
            LoadField(archive, "blendFrequency", params.blendFrequency);
            LoadField(archive, "yawOffset", params.yawOffset);
            LoadField(archive, "yawDriftSpeed", params.yawDriftSpeed);
        }

        //-------------------------------------------------------------
        //! @struct BossDirectorParams
        //! @brief  進行・演出の調整値（Assets/Tables/Systems/BossDirector.json。ここの初期値はJSONにキーが無いときの既定値）
        //-------------------------------------------------------------
        struct BossDirectorParams {
            float midBossSeconds        = 180.0f;    //!< 走行開始から中ボスが出るまで（秒）
            float finalBossDelaySeconds = 120.0f;    //!< 中ボスを倒してからラスボスが出るまで（秒）
            float defeatDelaySeconds    = 2.0f;      //!< ボスのHPが0になってから次へ進むまで（倒れるモーションを見せる）
            float spawnDistance         = 900.0f;    //!< プレイヤーの前方、ボスを置く距離（霧の手前）
            float detectRange           = 100000.0f; //!< ボスがプレイヤーを追い続ける距離（どこまでも追ってくる）

            BossEntryParams midBoss;
            BossEntryParams finalBoss = {"Paladin", "", "", L"夜の聖騎士", 2.2f, 60.0f, 2.0f, 1.0f, 30.0f};

            //-------------------------------------------------------------
            // 出現のカットシーン（遠景 → 寄り → プレイヤーへ戻す）
            //-------------------------------------------------------------
            BossShotParams revealShot;
            BossShotParams closeShot = {420.0f, 60.0f, 260.0f, -0.05f, 50.0f, 1.6f, 1.2f, 0.5f, 0.0f};
            BossShotParams returnShot = {420.0f, 150.0f, 210.0f, 0.22f, 60.0f, 1.0f, 1.4f, 3.14159f, 0.0f};

            // クリアのムービー（ラスボスを倒した後）：崩れ落ちるボスの周りを回る → 引いて夜明けの空 → 朝日の中のプレイヤー
            BossShotParams endingCollapseShot = {420.0f, 30.0f, 120.0f, 0.05f, 50.0f, 4.0f, 1.0f, 0.6f, 0.35f};
            BossShotParams endingSkyShot      = {1400.0f, 250.0f, 700.0f, 0.0f, 60.0f, 3.0f, 0.6f, 0.0f, 0.05f};
            BossShotParams endingPlayerShot   = {320.0f, 40.0f, 175.0f, 0.03f, 50.0f, 2.5f, 1.0f, 3.14159f, 0.1f};

            //-------------------------------------------------------------
            // 第二形態への変身（崩れ落ちる → 浮き上がって大きくなる → プレイヤーへ戻す）。
            // どれも上から見下ろす。浮き上がりは目線もボスと一緒に上がる（目線は対象の高さが基準）
            //-------------------------------------------------------------
            BossShotParams transformCollapseShot = {650.0f, 420.0f, 120.0f, 0.38f, 50.0f, 1.1f, 1.2f, 0.45f, 0.0f};
            BossShotParams transformRiseShot     = {1000.0f, 600.0f, 380.0f, 0.28f, 55.0f, 2.2f, 0.9f, -0.35f, 0.18f};
            BossShotParams transformReturnShot   = {520.0f, 300.0f, 200.0f, 0.3f, 60.0f, 1.0f, 1.4f, 0.0f, 0.0f};    //!< ボスがすぐ横にいるので、ボスと反対側からプレイヤーへ戻す

            //-------------------------------------------------------------
            // 告知（画面中央の帯と文字）。時間は実時間
            //-------------------------------------------------------------
            float          announceSeconds   = 3.2f;     //!< 出している時間
            float          announceOffsetY   = -60.0f;   //!< 画面中心からの呼び名のY
            float          bandTailX         = 520.0f;   //!< 帯の右端（画面中心から）
            float          bandHeight        = 170.0f;
            float          bandRollDegrees   = -3.0f;
            hlslpp::float4 midBandColor      = hlslpp::float4(0.08f, 0.10f, 0.28f, 0.88f);
            hlslpp::float4 finalBandColor    = hlslpp::float4(0.30f, 0.04f, 0.10f, 0.88f);
            hlslpp::float4 titleColor        = hlslpp::float4(1.0f, 1.0f, 1.0f, 1.0f);
        };

        template <class Archive>
        void load(Archive& archive, BossDirectorParams& params) {
            LoadField(archive, "midBossSeconds", params.midBossSeconds);
            LoadField(archive, "finalBossDelaySeconds", params.finalBossDelaySeconds);
            LoadField(archive, "defeatDelaySeconds", params.defeatDelaySeconds);
            LoadField(archive, "spawnDistance", params.spawnDistance);
            LoadField(archive, "detectRange", params.detectRange);
            LoadField(archive, "midBoss", params.midBoss);
            LoadField(archive, "finalBoss", params.finalBoss);
            LoadField(archive, "revealShot", params.revealShot);
            LoadField(archive, "closeShot", params.closeShot);
            LoadField(archive, "returnShot", params.returnShot);
            LoadField(archive, "endingCollapseShot", params.endingCollapseShot);
            LoadField(archive, "endingSkyShot", params.endingSkyShot);
            LoadField(archive, "endingPlayerShot", params.endingPlayerShot);
            LoadField(archive, "transformCollapseShot", params.transformCollapseShot);
            LoadField(archive, "transformRiseShot", params.transformRiseShot);
            LoadField(archive, "transformReturnShot", params.transformReturnShot);
            LoadField(archive, "announceSeconds", params.announceSeconds);
            LoadField(archive, "announceOffsetY", params.announceOffsetY);
            LoadField(archive, "bandTailX", params.bandTailX);
            LoadField(archive, "bandHeight", params.bandHeight);
            LoadField(archive, "bandRollDegrees", params.bandRollDegrees);
            LoadField(archive, "midBandColor", params.midBandColor);
            LoadField(archive, "finalBandColor", params.finalBandColor);
            LoadField(archive, "titleColor", params.titleColor);
        }

        //-------------------------------------------------------------
        //! @brief  チューニング値を得る関数（初回の呼び出しで読む。Debugでは保存し直すと読み直す）
        //-------------------------------------------------------------
        const BossDirectorParams& GetParams() {
            return GetSystemParams<BossDirectorParams>("BossDirector");
        }

        //-------------------------------------------------------------
        //! @brief  唯一の進行状態を探す（プレイヤーエンティティに付いている）
        //-------------------------------------------------------------
        BossProgressComponent* FindProgress(Tsukino::ECS::Registry& registry) {
            auto view = registry.View<BossProgressComponent>();
            for(entt::entity entity : view)
                return &view.get<BossProgressComponent>(entity);
            return nullptr;
        }

        //-------------------------------------------------------------
        //! @brief  元にする敵の生成関数を名前から引く
        //! @return 見つからなければ先頭（SmallZombie）
        //-------------------------------------------------------------
        const EnemySpawnTableEntry& FindBaseEntry(const std::string& baseType) {
            const auto table = GetEnemySpawnTable();
            for(const EnemySpawnTableEntry& entry : table) {
                if(baseType == GetEnemyTypeKey(entry.id))
                    return entry;
            }
            Tsukino::Core::Log::Error("BossDirector.json: unknown baseType '" + baseType + "'");
            return table.front();
        }

        //-------------------------------------------------------------
        //! @brief  ボスを置く位置を決める。カメラから見てプレイヤーの向こう側（＝画面の奥）
        //-------------------------------------------------------------
        hlslpp::float3 ResolveBossPosition(Tsukino::ECS::Registry& registry, const hlslpp::float3& playerPosition, float distance) {
            hlslpp::float3 forward(0.0f, 0.0f, 1.0f);

            auto cameraView = registry.View<TpsCameraComponent, Tsukino::BuiltIn::ECS::TransformComponent>();
            for(entt::entity entity : cameraView) {
                const hlslpp::float3 cameraPosition = cameraView.get<Tsukino::BuiltIn::ECS::TransformComponent>(entity).position;
                const float          dx             = static_cast<float>(playerPosition.x) - static_cast<float>(cameraPosition.x);
                const float          dz             = static_cast<float>(playerPosition.z) - static_cast<float>(cameraPosition.z);
                const float          length         = std::sqrt(dx * dx + dz * dz);
                if(length > 1.0f)
                    forward = hlslpp::float3(dx / length, 0.0f, dz / length);
                break;
            }

            return playerPosition + forward * distance;
        }

        //-------------------------------------------------------------
        //! @brief  ショット1つぶんを作る。向きはボスからプレイヤーへの向き（プレイヤーの側からボスを見る）が基準
        //-------------------------------------------------------------
        CutsceneShot MakeShot(const BossShotParams& params, Tsukino::ECS::Entity target, float baseYaw) {
            CutsceneShot shot;
            shot.targetEntity   = target;
            shot.distance       = params.distance;
            shot.height         = params.height;
            shot.lookHeight     = params.lookHeight;
            shot.yaw            = baseYaw + params.yawOffset;
            shot.pitch          = params.pitch;
            shot.fov            = params.fov;
            shot.duration       = params.duration;
            shot.blendFrequency = params.blendFrequency;
            shot.yawDriftSpeed  = params.yawDriftSpeed;
            return shot;
        }

        //-------------------------------------------------------------
        //! @brief  ラスボスを倒した後のクリアのムービーを始める（世界を止めないライブのカットシーン）
        //! @return 始められたか（スキル選択の予約などで始められなければfalse。そのときは従来どおり待ってからクリアにする）
        //! @note   残っている雑魚と敵の弾は、ムービーの前に消す（EXP玉は出さない）。
        //!         敵のAIはカットシーン中は止まるので、ボスの死亡モーションはここで直接始めておく
        //-------------------------------------------------------------
        bool StartEndingCutscene(Tsukino::ECS::Registry& registry, const BossDirectorParams& params, Tsukino::ECS::Entity playerEntity,
                                 Tsukino::ECS::Entity boss) {
            if(boss == entt::null || !registry.IsValid(boss) || !registry.HasComponent<Tsukino::BuiltIn::ECS::TransformComponent>(boss))
                return false;
            if(IsGameplayFrozen(registry))
                return false;    // PlayCutsceneは他の停止中は再生しない

            const hlslpp::float3 playerPosition = registry.GetComponent<Tsukino::BuiltIn::ECS::TransformComponent>(playerEntity).position;
            const hlslpp::float3 bossPosition   = registry.GetComponent<Tsukino::BuiltIn::ECS::TransformComponent>(boss).position;
            const float          baseYaw        = std::atan2(static_cast<float>(playerPosition.x) - static_cast<float>(bossPosition.x),
                                                             -(static_cast<float>(playerPosition.z) - static_cast<float>(bossPosition.z)));

            std::vector<CutsceneShot> shots;
            shots.push_back(MakeShot(params.endingCollapseShot, boss, baseYaw));
            CutsceneShot sky = MakeShot(params.endingSkyShot, playerEntity, baseYaw);
            sky.farZ         = 6000.0f;    // 引いた空の絵は地平線まで見せる
            shots.push_back(sky);
            shots.push_back(MakeShot(params.endingPlayerShot, playerEntity, baseYaw));
            PlayCutscene(registry, std::move(shots), true);

            //-------------------------------------------------------------
            // 後片付け：雑魚（ボス以外の敵）と敵の弾を消す。夜明けとともに消えた扱いで、EXP玉は出さない
            //-------------------------------------------------------------
            std::vector<Tsukino::ECS::Entity> doomed;
            {
                auto enemies = registry.View<EnemyComponent>();
                for(entt::entity entity : enemies) {
                    if(entity != boss)
                        doomed.push_back(entity);
                }
                auto projectiles = registry.View<EnemyProjectileComponent>();
                for(entt::entity entity : projectiles)
                    doomed.push_back(entity);
            }
            for(Tsukino::ECS::Entity entity : doomed) {
                if(const auto* health = registry.try_get<HealthComponent>(entity))
                    DestroyEnemyHpBar(registry, *health);
                if(const auto* held = registry.try_get<EnemyHeldWeaponComponent>(entity);
                   held && held->weaponEntity != entt::null && registry.IsValid(held->weaponEntity))
                    registry.QueueDestroy(held->weaponEntity);
                registry.QueueDestroy(entity);
            }

            // 敵のAIはムービー中は止まるので、倒れるモーションはここで始めておく（続きの消える演出はムービーの後）
            if(auto* animSet = registry.try_get<EnemyAnimationSetComponent>(boss))
                animSet->desiredState = EnemyAnimState::Death;

            // 倒した瞬間に大技と同じスローを掛ける
            if(auto* eventBus = registry.GetContext<Tsukino::ECS::EventBus*>())
                eventBus->Publish(PlayerFinisherEvent{playerEntity, 0.0f});
            return true;
        }

        //-------------------------------------------------------------
        //! @brief  ボスを1体湧かせ、出現の演出（カットシーン・告知・夜）を始める
        //! @return 湧かせたボス
        //-------------------------------------------------------------
        Tsukino::ECS::Entity SpawnBoss(Tsukino::ECS::Registry& registry, Tsukino::EngineIntegration::EngineContext& context,
                                       Tsukino::ECS::Entity playerEntity, BossProgressComponent& progress, BossStage stage, int dangerRank) {
            const BossDirectorParams& params = GetParams();
            const BossEntryParams&    entry  = stage == BossStage::Final ? params.finalBoss : params.midBoss;

            const hlslpp::float3 playerPosition = registry.GetComponent<Tsukino::BuiltIn::ECS::TransformComponent>(playerEntity).position;
            const hlslpp::float3 bossPosition   = ResolveBossPosition(registry, playerPosition, params.spawnDistance);

            //-------------------------------------------------------------
            // 中身は既存の敵。危険度の補正を掛けたうえでボスの倍率を積むので、
            // 中ボスが遅れて出るほど（F12で飛ばしても）その時点の雑魚より必ず強い
            //-------------------------------------------------------------
            const EnemySpawnTableEntry& base   = FindBaseEntry(entry.baseType);
            EnemySpawnConfig            config = base.makeConfig(context, bossPosition);
            if(!entry.prefab.empty())
                config.prefabName = entry.prefab.c_str();    // 調整値は関数内staticで寿命が走行より長いので、ポインタのまま渡してよい
            config.detectRange                 = params.detectRange;
            ApplyEnemyDifficulty(config, dangerRank);
            config.sizeScale *= entry.sizeScale;
            config.healthScale *= entry.healthScale;
            config.attackScale *= entry.attackScale;
            config.knockbackThresholdScale *= entry.healthScale;    // 一撃でひるまない硬さもHPと同じだけ伸ばす
            config.moveSpeedScale *= entry.moveSpeedScale;
            config.expScale *= entry.expScale;

            const Tsukino::ECS::Entity boss = SpawnBehaviorEnemy(registry, context, config);
            registry.AddComponent<BossComponent>(boss).stage = stage;

            // エリートの目印も付ける。紫の発光と、画面外にいるときの矢印（EliteIndicatorSystem）がそのまま効く
            registry.AddComponent<EliteEnemyComponent>(boss).baseType = base.id;

            //-------------------------------------------------------------
            // 技の表を持つボスは、雑魚のツリーを複数の技を持つボスのツリーへ差し替える。
            // 技ごとのクリップはここで読んでおき（技を出すたびに読まない）、のけぞらないようにする
            //-------------------------------------------------------------
            if(!entry.behavior.empty()) {
                const BossMoveSet& set = GetBossMoveSet(entry.behavior);

                BossMoveSetComponent& moveSet = registry.AddComponent<BossMoveSetComponent>(boss);
                moveSet.key                   = entry.behavior;
                moveSet.sizeScale             = config.sizeScale;
                moveSet.damageScale           = config.attackScale;
                moveSet.cooldowns.assign(set.moves.size(), 0.0f);
                for(const BossMoveDef& move : set.moves)
                    moveSet.clips.push_back(context.assetManager ? context.assetManager->Load(Tsukino::Core::Path(move.clip))
                                                                 : Tsukino::Asset::AssetHandle::Invalid());
                if(!set.breakDef.clip.empty() && context.assetManager)
                    moveSet.breakClip = context.assetManager->Load(Tsukino::Core::Path(set.breakDef.clip));

                EnemyComponent& enemy = registry.GetComponent<EnemyComponent>(boss);
                enemy.superArmor      = true;
                moveSet.baseMoveSpeed = enemy.moveSpeed;

                registry.GetComponent<BehaviorTreeComponent>(boss).root = BuildBossTree();
            }

            //-------------------------------------------------------------
            // 演出：カットシーン（世界を止めてボスを見せる）・告知・夜への切り替え（中心はボス）
            //-------------------------------------------------------------
            const float toPlayerX = static_cast<float>(playerPosition.x) - static_cast<float>(bossPosition.x);
            const float toPlayerZ = static_cast<float>(playerPosition.z) - static_cast<float>(bossPosition.z);
            const float baseYaw   = std::atan2(toPlayerX, -toPlayerZ);

            std::vector<CutsceneShot> shots;
            shots.push_back(MakeShot(params.revealShot, boss, baseYaw));
            shots.push_back(MakeShot(params.closeShot, boss, baseYaw));
            shots.push_back(MakeShot(params.returnShot, playerEntity, baseYaw));
            PlayCutscene(registry, std::move(shots));

            // 告知は呼び名だけを出す。「中ボス出現」のような種別名や残り時間はゲームの外の言葉に聞こえるので出さない
            progress.announceElapsed = 0.0f;
            progress.announceTitle   = entry.displayName;
            progress.announceFinal   = stage == BossStage::Final;

            RequestNightfall(registry, bossPosition);

            PlaySound(registry, SoundId::DangerUp);
            if(auto* eventBus = registry.GetContext<Tsukino::ECS::EventBus*>())
                eventBus->Publish(GameLogEvent{GameLogCategory::BossAppeared, entry.displayName});

            return boss;
        }

        //! 空中で倒れたボスが地面へ落ちる速さ（ユニット/秒）
        constexpr float kDeathFallSpeed = 600.0f;

        //-------------------------------------------------------------
        //! @brief  ボスの第二形態の後始末と演出（変身の告知・当たり判定の拡大・倒れた後の落下）
        //! @note   当たり判定のカプセルは付け直さないと大きさが変わらない。ビヘイビアツリーはViewの反復中に
        //!         動くので、付け外しはツリーではなくここ（反復の外）で行う
        //-------------------------------------------------------------
        void UpdateSecondForm(Tsukino::ECS::Registry& registry, const BossDirectorParams& params, BossProgressComponent& progress,
                              Tsukino::ECS::Entity playerEntity, float realDeltaTime) {
            if(progress.bossEntity == entt::null || !registry.IsValid(progress.bossEntity))
                return;
            auto* moveSet = registry.try_get<BossMoveSetComponent>(progress.bossEntity);
            if(!moveSet)
                return;

            // 変身が始まったら、上からのカメラで見せる。新しい呼び名の帯は、変身し終えたときに出す（下の当たり判定の拡大と同じ時）
            if(moveSet->phase == BossActionPhase::Transform && !moveSet->transformAnnounced) {
                moveSet->transformAnnounced = true;
                progress.secondFormName     = GetBossMoveSet(moveSet->key).secondForm.displayName;
                PlaySound(registry, SoundId::DangerUp);

                //-------------------------------------------------------------
                // 変身を上からのカメラで見せる。世界の時間を止めないライブのカットシーンにし、
                // EnemyBehaviorSystemはこの間も変身中のボスだけ進める（他の敵は止まる）
                //-------------------------------------------------------------
                const auto& bossTransform   = registry.GetComponent<Tsukino::BuiltIn::ECS::TransformComponent>(progress.bossEntity);
                const auto& playerTransform = registry.GetComponent<Tsukino::BuiltIn::ECS::TransformComponent>(playerEntity);
                const float toPlayerX       = static_cast<float>(playerTransform.position.x) - static_cast<float>(bossTransform.position.x);
                const float toPlayerZ       = static_cast<float>(playerTransform.position.z) - static_cast<float>(bossTransform.position.z);
                const float baseYaw         = std::atan2(toPlayerX, -toPlayerZ);

                std::vector<CutsceneShot> shots;
                shots.push_back(MakeShot(params.transformCollapseShot, progress.bossEntity, baseYaw));
                shots.push_back(MakeShot(params.transformRiseShot, progress.bossEntity, baseYaw));
                shots.push_back(MakeShot(params.transformReturnShot, playerEntity, baseYaw));
                PlayCutscene(registry, std::move(shots), true);
            }

            // 変身が終わったら、当たり判定のカプセルを体の大きさに合わせて付け直す（物理のボディが作り直される）
            if(moveSet->pendingColliderScale != 1.0f && registry.HasComponent<Tsukino::BuiltIn::ECS::CollisionComponent>(progress.bossEntity)) {
                Tsukino::BuiltIn::ECS::CollisionComponent collision = registry.GetComponent<Tsukino::BuiltIn::ECS::CollisionComponent>(progress.bossEntity);
                collision.extent         = collision.extent * moveSet->pendingColliderScale;
                collision.offsetPosition = collision.offsetPosition * moveSet->pendingColliderScale;
                collision.isInitialized  = false;
                collision.bodyID         = {};
                registry.RemoveComponent<Tsukino::BuiltIn::ECS::CollisionComponent>(progress.bossEntity);
                registry.AddComponent<Tsukino::BuiltIn::ECS::CollisionComponent>(progress.bossEntity, collision);
                moveSet->pendingColliderScale = 1.0f;

                // 新しい呼び名で告知の帯を出し直す（ボスHUDの名前もこれに変わる）
                if(!progress.secondFormName.empty()) {
                    progress.announceElapsed = 0.0f;
                    progress.announceTitle   = progress.secondFormName;
                    progress.announceFinal   = true;
                    if(auto* eventBus = registry.GetContext<Tsukino::ECS::EventBus*>())
                        eventBus->Publish(GameLogEvent{GameLogCategory::BossAppeared, progress.secondFormName});
                }
            }

            // 空中で倒れたら、地面まで落とす（倒れた後はビヘイビアツリーが高さを動かさない）
            const auto* health = registry.try_get<HealthComponent>(progress.bossEntity);
            if(health && health->isDead && moveSet->groundKnown && moveSet->altitude > 0.0f) {
                moveSet->altitude = std::max(moveSet->altitude - kDeathFallSpeed * realDeltaTime, 0.0f);
                auto& transform      = registry.GetComponent<Tsukino::BuiltIn::ECS::TransformComponent>(progress.bossEntity);
                transform.position.y = moveSet->groundY + moveSet->altitude;
                transform.dirty      = true;
            }
        }

        //-------------------------------------------------------------
        //! @brief  告知の帯と文字を置く（毎フレーム。画面サイズが変わっても中央に来る）
        //-------------------------------------------------------------
        void UpdateAnnouncement(Tsukino::ECS::Registry& registry, Tsukino::EngineIntegration::EngineContext& ctx, BossProgressComponent& progress,
                                float realDeltaTime) {
            if(progress.announceElapsed < 0.0f)
                return;

            const BossDirectorParams& params = GetParams();

            const bool firstFrame = progress.announceElapsed == 0.0f;
            progress.announceElapsed += realDeltaTime;

            //-------------------------------------------------------------
            // 出している時間を過ぎたら消す。消える動きを始めたら、次のフレームから置き直さない
            //-------------------------------------------------------------
            if(progress.announceElapsed >= params.announceSeconds) {
                UiDisappear(registry, progress.announceBandEntity, true);
                UiDisappear(registry, progress.announceTitleEntity, true);
                progress.announceElapsed = -1.0f;
                return;
            }

            const float screenWidth   = ctx.window ? static_cast<float>(ctx.window->GetWidth()) : 1700.0f;
            const float screenHeight  = ctx.window ? static_cast<float>(ctx.window->GetHeight()) : 1000.0f;
            const float screenCenterX = screenWidth * 0.5f;
            const float titleY        = screenHeight * 0.5f + params.announceOffsetY;

            PlaceUiBandFromLeft(registry, ctx, progress.announceBandEntity, screenCenterX + params.bandTailX, titleY, params.bandHeight, params.bandRollDegrees, progress.announceFinal ? params.finalBandColor : params.midBandColor);
            PlaceUiText(registry, progress.announceTitleEntity, screenCenterX, titleY, GetUiTextScale(UiTextSize::Display), progress.announceTitle,
                        params.titleColor);
            UiIdle(registry, progress.announceTitleEntity, 1.0f, true);    // 出ている間は止まったままにしない

            // 出始めだけ動きを付ける（カットシーン中は世界が止まっているので実時間で動かす）
            if(firstFrame) {
                UiAppear(registry, progress.announceBandEntity, UiAppearDesc{.slideX = -300.0f, .unscaledTime = true});
                UiAppear(registry, progress.announceTitleEntity,
                         UiAppearDesc{.scaleFrom = 1.8f, .duration = GetUiMotionParams().popDuration * 1.4f, .unscaledTime = true});
            }
        }
    }    // namespace

    //-------------------------------------------------------------
    //! @brief ボスと戦っている最中かを問い合わせる
    //-------------------------------------------------------------
    bool IsBossBattleActive(Tsukino::ECS::Registry& registry) {
        const BossProgressComponent* progress = FindProgress(registry);
        return progress && (progress->phase == BossPhase::MidActive || progress->phase == BossPhase::FinalActive);
    }

    //-------------------------------------------------------------
    //! @brief ラスボスを倒し終えたかを問い合わせる
    //-------------------------------------------------------------
    bool IsFinalBossDefeated(Tsukino::ECS::Registry& registry) {
        const BossProgressComponent* progress = FindProgress(registry);
        return progress && progress->phase == BossPhase::FinalDefeated;
    }

    //-------------------------------------------------------------
    //! @brief BossSummonEventの購読を始める
    //-------------------------------------------------------------
    void BossDirectorSystem::Initialize(Tsukino::ECS::EventBus& eventBus) {
        // ハンドラは積むだけ。ビヘイビアツリーの反復中に届くので、ここでは湧かせない
        m_summonConnection     = eventBus.Subscribe<BossSummonEvent>([this](const BossSummonEvent& summon) { m_pendingSummons.push_back(summon); });
        m_projectileConnection = eventBus.Subscribe<BossProjectileEvent>([this](const BossProjectileEvent& shot) { m_pendingProjectiles.push_back(shot); });
    }

    //-------------------------------------------------------------
    //! @brief 呼ばれた雑魚を湧かせる
    //! @note  湧き潰しと同じく危険度の補正を掛け、SpawnedEnemyComponentを付けて
    //!        既存の「霧から浮かび上がる」フェードインと、遠くへ取り残されたときの間引きに乗せる
    //-------------------------------------------------------------
    void BossDirectorSystem::SpawnSummons(Tsukino::ECS::Registry& registry, const BossSummonEvent& summon, int dangerRank) {
        auto* ctx = registry.GetContext<Tsukino::EngineIntegration::EngineContext*>();
        if(!ctx || summon.count <= 0)
            return;

        const EnemySpawnTableEntry& base = FindBaseEntry("SmallZombie");
        std::uniform_real_distribution<float> jitter(-0.4f, 0.4f);

        for(int i = 0; i < summon.count; ++i) {
            // 周りに等間隔で並べ、少しずつずらす（綺麗な円に並ぶと作り物に見える）
            const float angle = 6.2831853f * (static_cast<float>(i) + 0.5f + jitter(m_rng)) / static_cast<float>(summon.count);
            const hlslpp::float3 position = summon.center + hlslpp::float3(std::sin(angle), 0.0f, std::cos(angle)) * summon.radius;

            EnemySpawnConfig config = base.makeConfig(*ctx, position);
            if(!summon.prefab.empty())
                config.prefabName = summon.prefab.c_str();    // イベントは湧かせ終わるまで生きているので、ポインタのまま渡してよい
            config.detectRange = GetParams().detectRange;
            ApplyEnemyDifficulty(config, dangerRank);
            config.sizeScale *= summon.sizeScale;
            config.healthScale *= summon.healthScale;

            const Tsukino::ECS::Entity minion = SpawnBehaviorEnemy(registry, *ctx, config);
            registry.AddComponent<SpawnedEnemyComponent>(minion).spawnDistance = summon.radius;
        }
    }

    //-------------------------------------------------------------
    //! @brief ボスの弾を作る
    //! @note  見た目は仮（Prefab: Enemy/WebShot。EXP玉のビルボードを白くしたもの）
    //-------------------------------------------------------------
    void BossDirectorSystem::SpawnProjectile(Tsukino::ECS::Registry& registry, const BossProjectileEvent& shot) {
        auto* ctx = registry.GetContext<Tsukino::EngineIntegration::EngineContext*>();
        if(!ctx)
            return;

        const Tsukino::ECS::Entity entity = InstantiatePrefab(registry, *ctx, shot.prefab.empty() ? "Enemy/WebShot" : shot.prefab.c_str());
        if(entity == entt::null)
            return;

        if(auto* transform = registry.try_get<Tsukino::BuiltIn::ECS::TransformComponent>(entity)) {
            transform->position = shot.origin;
            transform->dirty    = true;
        }

        EnemyProjectileComponent& projectile = registry.AddComponent<EnemyProjectileComponent>(entity);
        projectile.owner                     = shot.owner;
        projectile.direction                 = shot.direction;
        projectile.speed                     = shot.speed;
        projectile.damage                    = shot.damage;
        projectile.radius                    = shot.radius;
        projectile.remaining                 = shot.range;
        projectile.slowSeconds               = shot.slowSeconds;
        projectile.slowScale                 = shot.slowScale;
    }

    //-------------------------------------------------------------
    //! @brief システムの更新
    //-------------------------------------------------------------
    void BossDirectorSystem::Update(Tsukino::ECS::Registry& registry, float deltaTime) {
        const BossDirectorParams& params = GetParams();

        auto* ctx = registry.GetContext<Tsukino::EngineIntegration::EngineContext*>();
        if(!ctx)
            return;

        const float realDeltaTime =
            registry.HasContext<WorldTimeContext>() ? registry.GetContext<WorldTimeContext>().realDeltaTime : deltaTime;

        auto view = registry.View<PlayerComponent, HealthComponent, RunClockComponent, BossProgressComponent>();
        for(entt::entity playerEntity : view) {
            const HealthComponent&   playerHealth = view.get<HealthComponent>(playerEntity);
            const RunClockComponent& clock        = view.get<RunClockComponent>(playerEntity);
            BossProgressComponent&   progress     = view.get<BossProgressComponent>(playerEntity);

            UpdateAnnouncement(registry, *ctx, progress, realDeltaTime);

            // ボスが呼んだ雑魚を湧かせる（倒れた後に届いた分は捨てる）
            if(!playerHealth.isDead) {
                for(const BossSummonEvent& summon : m_pendingSummons)
                    SpawnSummons(registry, summon, clock.dangerRank);
            }
            m_pendingSummons.clear();

            // ボスが撃った弾を作る（倒れた後に届いた分は捨てる）
            if(!playerHealth.isDead) {
                for(const BossProjectileEvent& shot : m_pendingProjectiles)
                    SpawnProjectile(registry, shot);
            }
            m_pendingProjectiles.clear();

            UpdateSecondForm(registry, params, progress, playerEntity, realDeltaTime);

            // 倒れた後は進めない（GAME OVERの裏でボスが湧かないように）
            if(playerHealth.isDead)
                continue;

            //-------------------------------------------------------------
            // 戦っているボスの様子を見る。HPが0になったら倒れるモーションの分だけ待ってから次へ進む。
            // （entityが消えていた＝何かの理由で消された場合も撃破として扱い、進行が詰まらないようにする）
            //-------------------------------------------------------------
            if(progress.phase == BossPhase::MidActive || progress.phase == BossPhase::FinalActive) {
#ifdef _DEBUG
                // Shift+F12：戦っているボスを倒す（撃破からクリアまでを確かめるためのデバッグ操作）。
                // 当たり判定（CombatHit.cpp）と同じくHPを0にしてisDeadを立てるので、倒れる演出はそのまま流れる
                if(ctx->inputSystem && ctx->inputSystem->IsKeyDown(Tsukino::Input::KeyCode::Shift)
                   && ctx->inputSystem->IsKeyPressed(Tsukino::Input::KeyCode::F12) && registry.IsValid(progress.bossEntity)) {
                    if(auto* bossHealth = registry.try_get<HealthComponent>(progress.bossEntity); bossHealth && !bossHealth->isDead) {
                        bossHealth->currentHealth = 0.0f;
                        // 第二形態が残っていれば1回目は変身させる（2回目で倒れる）
                        auto* bossMoveSet = registry.try_get<BossMoveSetComponent>(progress.bossEntity);
                        if(bossMoveSet && !bossMoveSet->secondFormUsed && !GetBossMoveSet(bossMoveSet->key).secondForm.key.empty()) {
                            if(bossMoveSet->phase != BossActionPhase::Transform)
                                bossMoveSet->transformPending = true;
                        } else {
                            bossHealth->isDead = true;
                        }
                    }
                }
#endif
                if(!progress.bossDown) {
                    const HealthComponent* bossHealth =
                        registry.IsValid(progress.bossEntity) ? registry.try_get<HealthComponent>(progress.bossEntity) : nullptr;
                    if(bossHealth && !bossHealth->isDead)
                        continue;

                    progress.bossDown     = true;
                    progress.phaseElapsed = 0.0f;

                    const bool              isFinal = progress.phase == BossPhase::FinalActive;
                    const BossEntryParams&  entry   = isFinal ? params.finalBoss : params.midBoss;
                    if(auto* eventBus = registry.GetContext<Tsukino::ECS::EventBus*>())
                        eventBus->Publish(GameLogEvent{GameLogCategory::BossDefeated, entry.displayName});

                    // ラスボスを倒した後の待ち（ムービー）で倒されてGAME OVERにならないよう、無敵にしておく
                    if(isFinal) {
                        view.get<PlayerComponent>(playerEntity).levelUpInvincibleTimer =
                            std::max(view.get<PlayerComponent>(playerEntity).levelUpInvincibleTimer, params.defeatDelaySeconds + 1.0f);
                        progress.endingPlaying  = StartEndingCutscene(registry, params, playerEntity, progress.bossEntity);
                        progress.endingDaybreak = false;
                    } else {
                        RequestDaybreak(registry);    // 中ボスを倒したら夜が明ける
                    }
                }

                //-------------------------------------------------------------
                // クリアのムービー中：2本目（空の絵）に入ったら夜を明けさせ、終わったら（スキップを含む）クリアへ進む
                //-------------------------------------------------------------
                if(progress.endingPlaying) {
                    if(IsCutsceneActive(registry)) {
                        if(!progress.endingDaybreak && GetCutsceneShotIndex(registry) >= 1) {
                            progress.endingDaybreak = true;
                            RequestDaybreak(registry);
                        }
                        continue;
                    }
                    if(!progress.endingDaybreak)
                        RequestDaybreak(registry);    // 空の絵の前にスキップされても、夜は明けさせる
                    progress.endingPlaying = false;
                } else {
                    progress.phaseElapsed += deltaTime;
                    if(progress.phaseElapsed < params.defeatDelaySeconds)
                        continue;
                }

                progress.phase                 = progress.phase == BossPhase::FinalActive ? BossPhase::FinalDefeated : BossPhase::Interlude;
                progress.phaseElapsed          = 0.0f;
                progress.interludeStartSeconds = clock.elapsedSeconds;
                progress.bossEntity   = entt::null;
                progress.bossDown     = false;
                continue;
            }

            //-------------------------------------------------------------
            // 次のボスを出すかを決める。どちらも走行の時計で数える（メニュー中は止まり、F12の早送りも効く）。
            // メニュー・カットシーン中は出さない（PlayCutsceneが他の停止中には再生されないため。時計も止まっているので遅れは出ない）
            //-------------------------------------------------------------
            const bool midDue = progress.phase == BossPhase::BeforeMid && clock.elapsedSeconds >= params.midBossSeconds;
            const bool finalDue =
                progress.phase == BossPhase::Interlude && clock.elapsedSeconds - progress.interludeStartSeconds >= params.finalBossDelaySeconds;
            if((!midDue && !finalDue) || IsGameplayFrozen(registry))
                continue;

            const BossStage stage = finalDue ? BossStage::Final : BossStage::Mid;
            progress.bossEntity   = SpawnBoss(registry, *ctx, playerEntity, progress, stage, clock.dangerRank);
            progress.phase        = finalDue ? BossPhase::FinalActive : BossPhase::MidActive;
            progress.phaseElapsed = 0.0f;
            progress.bossDown     = false;
        }
    }
}    // namespace CombatAndroid::ECS
