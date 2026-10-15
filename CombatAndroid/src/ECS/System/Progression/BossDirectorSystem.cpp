//-------------------------------------------------------------
//! @file   BossDirectorSystem.cpp
//! @brief  BossDirectorSystemクラスの実装
//-------------------------------------------------------------
#include <CombatAndroid/ECS/System/Progression/BossDirectorSystem.hpp>
#include <CombatAndroid/ECS/Serialization/Common/SerializationHelper.hpp>
#include <CombatAndroid/ECS/Utility/Table/TableJson.hpp>
#include <Tsukino/Core/Math/Serialization/HlslppSerialization.hpp>
#include <CombatAndroid/ECS/Component/Combat/HealthComponent.hpp>
#include <CombatAndroid/ECS/Component/Enemy/BossComponent.hpp>
#include <CombatAndroid/ECS/Component/Enemy/EliteEnemyComponent.hpp>
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
            std::wstring displayName    = L"ボス";         //!< 告知とログに出す呼び名
            float        sizeScale      = 1.8f;
            float        healthScale    = 25.0f;
            float        attackScale    = 1.5f;
            float        moveSpeedScale = 1.0f;
            float        expScale       = 10.0f;
        };

        template <class Archive>
        void load(Archive& archive, BossEntryParams& params) {
            LoadField(archive, "baseType", params.baseType);
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
            BossEntryParams finalBoss = {"Paladin", L"ラスボス", 2.2f, 60.0f, 2.0f, 1.0f, 30.0f};

            //-------------------------------------------------------------
            // 出現のカットシーン（遠景 → 寄り → プレイヤーへ戻す）
            //-------------------------------------------------------------
            BossShotParams revealShot;
            BossShotParams closeShot = {420.0f, 60.0f, 260.0f, -0.05f, 50.0f, 1.6f, 1.2f, 0.5f, 0.0f};
            BossShotParams returnShot = {420.0f, 150.0f, 210.0f, 0.22f, 60.0f, 1.0f, 1.4f, 3.14159f, 0.0f};

            //-------------------------------------------------------------
            // 告知（画面中央の帯と文字）。時間は実時間
            //-------------------------------------------------------------
            float          announceSeconds   = 3.2f;     //!< 出している時間
            float          announceOffsetY   = -60.0f;   //!< 画面中心からの見出しのY
            float          subtitleGapY      = 70.0f;    //!< 見出しから呼び名までのY
            float          bandTailX         = 520.0f;   //!< 帯の右端（画面中心から）
            float          bandHeight        = 170.0f;
            float          bandRollDegrees   = -3.0f;
            hlslpp::float4 midBandColor      = hlslpp::float4(0.08f, 0.10f, 0.28f, 0.88f);
            hlslpp::float4 finalBandColor    = hlslpp::float4(0.30f, 0.04f, 0.10f, 0.88f);
            hlslpp::float4 titleColor        = hlslpp::float4(1.0f, 1.0f, 1.0f, 1.0f);
            hlslpp::float4 subtitleColor     = hlslpp::float4(0.80f, 0.85f, 1.0f, 1.0f);
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
            LoadField(archive, "announceSeconds", params.announceSeconds);
            LoadField(archive, "announceOffsetY", params.announceOffsetY);
            LoadField(archive, "subtitleGapY", params.subtitleGapY);
            LoadField(archive, "bandTailX", params.bandTailX);
            LoadField(archive, "bandHeight", params.bandHeight);
            LoadField(archive, "bandRollDegrees", params.bandRollDegrees);
            LoadField(archive, "midBandColor", params.midBandColor);
            LoadField(archive, "finalBandColor", params.finalBandColor);
            LoadField(archive, "titleColor", params.titleColor);
            LoadField(archive, "subtitleColor", params.subtitleColor);
        }

        //-------------------------------------------------------------
        //! @brief  チューニング値を得る関数（初回の呼び出しで1度だけ読む）
        //-------------------------------------------------------------
        const BossDirectorParams& GetParams() {
            static const BossDirectorParams s_params = LoadSystemParams<BossDirectorParams>("BossDirector");
            return s_params;
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
        //! @brief  秒数を「分:秒」の文字列にする（例：「1:05」）
        //-------------------------------------------------------------
        std::wstring FormatMinutesSeconds(float seconds) {
            const int    totalSeconds = std::max(static_cast<int>(std::ceil(seconds)), 0);
            std::wstring secondsText  = std::to_wstring(totalSeconds % 60);
            if(secondsText.size() < 2)
                secondsText.insert(0, L"0");
            return std::to_wstring(totalSeconds / 60) + L":" + secondsText;
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

            progress.announceElapsed  = 0.0f;
            progress.announceTitle    = stage == BossStage::Final ? L"ラスボス出現" : L"中ボス出現";
            progress.announceSubtitle = entry.displayName;
            progress.announceFinal    = stage == BossStage::Final;

            RequestNightfall(registry, bossPosition);

            PlaySound(registry, SoundId::DangerUp);
            if(auto* eventBus = registry.GetContext<Tsukino::ECS::EventBus*>())
                eventBus->Publish(GameLogEvent{GameLogCategory::BossAppeared, entry.displayName});

            return boss;
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
                UiDisappear(registry, progress.announceSubtitleEntity, true);
                progress.announceElapsed = -1.0f;
                return;
            }

            const float screenWidth   = ctx.window ? static_cast<float>(ctx.window->GetWidth()) : 1700.0f;
            const float screenHeight  = ctx.window ? static_cast<float>(ctx.window->GetHeight()) : 1000.0f;
            const float screenCenterX = screenWidth * 0.5f;
            const float titleY        = screenHeight * 0.5f + params.announceOffsetY;

            PlaceUiBandFromLeft(registry, ctx, progress.announceBandEntity, screenCenterX + params.bandTailX, titleY + params.subtitleGapY * 0.4f,
                                params.bandHeight, params.bandRollDegrees, progress.announceFinal ? params.finalBandColor : params.midBandColor);
            PlaceUiText(registry, progress.announceTitleEntity, screenCenterX, titleY, GetUiTextScale(UiTextSize::Display), progress.announceTitle,
                        params.titleColor);
            PlaceUiText(registry, progress.announceSubtitleEntity, screenCenterX, titleY + params.subtitleGapY, GetUiTextScale(UiTextSize::Heading),
                        progress.announceSubtitle, params.subtitleColor);

            // 出始めだけ動きを付ける（カットシーン中は世界が止まっているので実時間で動かす）
            if(firstFrame) {
                UiFollow(registry, progress.announceSubtitleEntity, progress.announceTitleEntity);
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
    //! @brief HUDに出す「次の目標」の文字を作る
    //-------------------------------------------------------------
    std::wstring GetBossObjectiveText(Tsukino::ECS::Registry& registry) {
        const BossProgressComponent* progress = FindProgress(registry);
        if(!progress)
            return {};

        const BossDirectorParams& params = GetParams();

        float elapsed = 0.0f;
        auto  view    = registry.View<BossProgressComponent, RunClockComponent>();
        for(entt::entity entity : view)
            elapsed = view.get<RunClockComponent>(entity).elapsedSeconds;

        switch(progress->phase) {
            case BossPhase::BeforeMid:
                return L"中ボスまで " + FormatMinutesSeconds(params.midBossSeconds - elapsed);
            case BossPhase::Interlude:
                return L"ラスボスまで " + FormatMinutesSeconds(params.finalBossDelaySeconds - (elapsed - progress->interludeStartSeconds));
            case BossPhase::MidActive:
                return L"中ボスを倒せ";
            case BossPhase::FinalActive:
                return L"ラスボスを倒せ";
            case BossPhase::FinalDefeated:
                return {};
        }
        return {};
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
                        bossHealth->isDead        = true;
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

                    // ラスボスを倒した後の待ちで倒されてGAME OVERにならないよう、無敵にしておく
                    if(isFinal)
                        view.get<PlayerComponent>(playerEntity).levelUpInvincibleTimer =
                            std::max(view.get<PlayerComponent>(playerEntity).levelUpInvincibleTimer, params.defeatDelaySeconds + 1.0f);
                    else
                        RequestDaybreak(registry);    // 中ボスを倒したら夜が明ける（ラスボスはそのままリザルトへ）
                }

                progress.phaseElapsed += deltaTime;
                if(progress.phaseElapsed < params.defeatDelaySeconds)
                    continue;

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
