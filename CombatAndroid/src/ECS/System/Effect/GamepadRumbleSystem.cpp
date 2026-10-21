//-------------------------------------------------------------
//! @file   GamepadRumbleSystem.cpp
//! @brief  GamepadRumbleSystemクラスの実装
//-------------------------------------------------------------
#include <CombatAndroid/ECS/System/Effect/GamepadRumbleSystem.hpp>
#include <CombatAndroid/ECS/Component/Player/PlayerComponent.hpp>
#include <CombatAndroid/ECS/Serialization/Common/SerializationHelper.hpp>
#include <CombatAndroid/ECS/Utility/Save/GameSettings.hpp>
#include <CombatAndroid/ECS/Utility/Table/TableJson.hpp>
#include <CombatAndroid/ECS/Utility/Time/GameplayFreeze.hpp>
#include <CombatAndroid/ECS/Utility/Time/WorldTimeContext.hpp>

#include <Tsukino/EngineIntegration/EngineContext.hpp>

#include <Tsukino/Core/Input/InputSystem.hpp>

#include <algorithm>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    namespace {
        //-------------------------------------------------------------
        //! @struct RumbleSetting
        //! @brief  振動1種類ぶんの強さと長さ
        //-------------------------------------------------------------
        struct RumbleSetting {
            float low      = 0.0f;    //!< 左側（低周波・重い）モーターの強さ（0〜1）
            float high     = 0.0f;    //!< 右側（高周波・軽い）モーターの強さ（0〜1）
            float duration = 0.0f;    //!< 震わせる秒（実時間）
        };

        template <class Archive>
        void load(Archive& archive, RumbleSetting& setting) {
            LoadField(archive, "low", setting.low);
            LoadField(archive, "high", setting.high);
            LoadField(archive, "duration", setting.duration);
        }

        //-------------------------------------------------------------
        //! @struct GamepadRumbleParams
        //! @brief  振動のチューニング値（Assets/Tables/Systems/GamepadRumble.json。ここの初期値はJSONにキーが無いときの既定値）
        //! @note   斬った手ごたえは軽いモーターで短く、受けた衝撃・大技は重いモーターで長めにする
        //-------------------------------------------------------------
        struct GamepadRumbleParams {
            RumbleSetting hit         = {0.10f, 0.35f, 0.06f};    //!< プレイヤーの攻撃が当たった
            RumbleSetting heavyStrike = {0.45f, 0.55f, 0.14f};    //!< 連撃の最終段のインパクト
            RumbleSetting finisher    = {0.80f, 0.70f, 0.30f};    //!< 大技のインパクト
            RumbleSetting counterKick = {0.60f, 0.50f, 0.18f};    //!< ジャスト回避からの反撃キック
            RumbleSetting damaged     = {0.75f, 0.30f, 0.22f};    //!< 被弾
            RumbleSetting justDodge   = {0.00f, 0.45f, 0.08f};    //!< ジャスト回避
        };

        template <class Archive>
        void load(Archive& archive, GamepadRumbleParams& params) {
            LoadField(archive, "hit", params.hit);
            LoadField(archive, "heavyStrike", params.heavyStrike);
            LoadField(archive, "finisher", params.finisher);
            LoadField(archive, "counterKick", params.counterKick);
            LoadField(archive, "damaged", params.damaged);
            LoadField(archive, "justDodge", params.justDodge);
        }

        //-------------------------------------------------------------
        //! @brief  チューニング値を得る関数（初回の呼び出しで読む。Debugでは保存し直すと読み直す）
        //-------------------------------------------------------------
        const GamepadRumbleParams& GetParams() {
            return GetSystemParams<GamepadRumbleParams>("GamepadRumble");
        }
    }    // namespace

    //-------------------------------------------------------------
    //! @brief 各通知の購読を開始する
    //-------------------------------------------------------------
    void GamepadRumbleSystem::Initialize(Tsukino::ECS::EventBus& eventBus) {
        (void)GetParams();    // 初回フレームでファイルを読まないよう先に読んでおく

        // ハンドラは積むだけにする（通知はView反復中に来ることがあるので、ここでレジストリを触らない）
        m_hitConnection = eventBus.Subscribe<WeaponHitEvent>(
            [this](const WeaponHitEvent& event) { Enqueue(RumbleKind::Hit, 0.0f, event.attacker); });
        m_heavyStrikeConnection = eventBus.Subscribe<PlayerHeavyStrikeEvent>(
            [this](const PlayerHeavyStrikeEvent& event) { Enqueue(RumbleKind::HeavyStrike, event.impactDelay, event.player); });
        m_finisherConnection = eventBus.Subscribe<PlayerFinisherEvent>(
            [this](const PlayerFinisherEvent& event) { Enqueue(RumbleKind::Finisher, event.impactDelay, event.player); });
        m_counterKickConnection = eventBus.Subscribe<PlayerCounterKickEvent>(
            [this](const PlayerCounterKickEvent& event) { Enqueue(RumbleKind::CounterKick, 0.0f, event.player); });
        m_damagedConnection = eventBus.Subscribe<PlayerDamagedEvent>(
            [this](const PlayerDamagedEvent& event) { Enqueue(RumbleKind::Damaged, 0.0f, event.player); });
        m_justDodgeConnection = eventBus.Subscribe<PlayerJustDodgeEvent>(
            [this](const PlayerJustDodgeEvent& event) { Enqueue(RumbleKind::JustDodge, 0.0f, event.player); });
    }

    //-------------------------------------------------------------
    //! @brief 振動の要求を積む
    //-------------------------------------------------------------
    void GamepadRumbleSystem::Enqueue(RumbleKind kind, float delay, entt::entity source) {
        m_pending.push_back(PendingRumble{kind, std::max(delay, 0.0f), source});
    }

    //-------------------------------------------------------------
    //! @brief システムの更新
    //-------------------------------------------------------------
    void GamepadRumbleSystem::Update(Tsukino::ECS::Registry& registry, float deltaTime) {
        auto* ctx = registry.GetContext<Tsukino::EngineIntegration::EngineContext*>();
        if(!ctx || !ctx->inputSystem)
            return;

        Tsukino::Input::InputSystem& input = *ctx->inputSystem;

        //-------------------------------------------------------------
        // 振動OFF・ポーズやメニューで止まっている間は、震えを止めて積んだ分も捨てる
        // （メニューを閉じた瞬間に、開く前の振動が遅れて来ないように）
        //-------------------------------------------------------------
        if(!GetGameSettings().vibrationEnabled || IsGameplayFrozen(registry)) {
            if(m_activeRemaining > 0.0f || !m_pending.empty())
                input.StopGamepadVibration();
            m_pending.clear();
            m_activeStrength  = 0.0f;
            m_activeRemaining = 0.0f;
            return;
        }

        // 大技のスロー中もインパクトの時刻はずれないよう、待ち時間は実時間で数える
        const float realDeltaTime =
            registry.HasContext<WorldTimeContext>() ? registry.GetContext<WorldTimeContext>().realDeltaTime : deltaTime;

        m_activeRemaining = std::max(m_activeRemaining - realDeltaTime, 0.0f);
        if(m_activeRemaining <= 0.0f)
            m_activeStrength = 0.0f;

        const GamepadRumbleParams& params = GetParams();

        for(auto it = m_pending.begin(); it != m_pending.end();) {
            it->delay -= realDeltaTime;
            if(it->delay > 0.0f) {
                ++it;
                continue;
            }

            // 震わせるのはプレイヤーが出した手ごたえだけ（敵の武器のヒットや、攻撃した側が消えた後は震わせない）
            const bool fromPlayer = it->source != entt::null && registry.HasComponent<PlayerComponent>(it->source);
            if(fromPlayer) {
                const RumbleSetting* setting = nullptr;
                switch(it->kind) {
                    case RumbleKind::Hit:
                        setting = &params.hit;
                        break;
                    case RumbleKind::HeavyStrike:
                        setting = &params.heavyStrike;
                        break;
                    case RumbleKind::Finisher:
                        setting = &params.finisher;
                        break;
                    case RumbleKind::CounterKick:
                        setting = &params.counterKick;
                        break;
                    case RumbleKind::Damaged:
                        setting = &params.damaged;
                        break;
                    case RumbleKind::JustDodge:
                        setting = &params.justDodge;
                        break;
                }

                // 震えている最中の、より弱い振動では上書きしない
                const float strength = setting ? std::max(setting->low, setting->high) : 0.0f;
                if(setting && setting->duration > 0.0f && strength >= m_activeStrength) {
                    input.SetGamepadVibration(setting->low, setting->high, setting->duration);
                    m_activeStrength  = strength;
                    m_activeRemaining = setting->duration;
                }
            }

            it = m_pending.erase(it);
        }
    }
}    // namespace CombatAndroid::ECS
