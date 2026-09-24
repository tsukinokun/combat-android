//-------------------------------------------------------------
//! @file   RunResultSystem.cpp
//! @brief  RunResultSystemクラスの実装
//-------------------------------------------------------------
#include <CombatAndroid/ECS/System/RunResultSystem.hpp>
#include <CombatAndroid/ECS/Component/HealthComponent.hpp>
#include <CombatAndroid/ECS/Component/PlayerComponent.hpp>
#include <CombatAndroid/ECS/Component/PlayerExperienceComponent.hpp>
#include <CombatAndroid/ECS/Component/PlayerSkillComponent.hpp>
#include <CombatAndroid/ECS/Component/RunClockComponent.hpp>
#include <CombatAndroid/ECS/Component/RunResultComponent.hpp>
#include <CombatAndroid/ECS/Event/SoundEvent.hpp>
#include <CombatAndroid/ECS/Utility/GameplayFreeze.hpp>
#include <CombatAndroid/ECS/Utility/RunRecord.hpp>
#include <CombatAndroid/ECS/Utility/ScreenFade.hpp>
#include <CombatAndroid/ECS/Utility/UiSprite.hpp>
#include <CombatAndroid/ECS/Utility/WorldTimeContext.hpp>
#include <CombatAndroid/Scene/CombatAndroidScene.hpp>
#include <CombatAndroid/Scene/TitleScene.hpp>

#include <Tsukino/EngineIntegration/EngineContext.hpp>
#include <Tsukino/EngineIntegration/Scene/GameSceneManager.hpp>

#include <Tsukino/Core/Input/InputSystem.hpp>
#include <Tsukino/Core/Window.hpp>

#include <entt/entt.hpp>

#include <algorithm>
#include <array>
#include <memory>
#include <string>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    namespace {
        //-------------------------------------------------------------
        //! @enum   ResultMenuItem
        //! @brief  リザルトのメニュー項目（並び順どおり）
        //-------------------------------------------------------------
        enum class ResultMenuItem : int {
            Retry = 0,    //!< もう一度
            Title,        //!< タイトルへ戻る
            Count,
        };

        //! 項目の文字。ResultMenuItemの並びと揃えること
        const std::array<std::wstring, static_cast<size_t>(ResultMenuItem::Count)> kMenuLabels = {
            L"リトライ",
            L"タイトルへ",
        };

        //-------------------------------------------------------------
        // 走行が終わってからリザルトを出すまでの待ち（実時間）。
        // 死亡は倒れるモーションを見せたいので長め、クリアは止まった画面を一呼吸見せる程度
        //-------------------------------------------------------------
        constexpr float kDeadOverlayDelay  = 1.5f;
        constexpr float kClearOverlayDelay = 1.0f;

        //-------------------------------------------------------------
        // レイアウト（画面中心からのピクセル）
        //-------------------------------------------------------------
        constexpr float kTitleOffsetY    = -300.0f;
        constexpr float kFirstRowOffsetY = -170.0f;
        constexpr float kRowPitch        = 54.0f;
        constexpr float kLabelOffsetX    = -250.0f;    //!< 項目名の左端
        constexpr float kValueOffsetX    = 30.0f;      //!< 値の左端
        constexpr float kRecordOffsetX   = 210.0f;     //!< 「NEW RECORD」の左端
        constexpr float kSkillsOffsetY   = 60.0f;
        constexpr float kBestOffsetY     = 108.0f;
        constexpr float kMenuTopOffsetY  = 200.0f;

        constexpr float kTitleFontScale  = 2.6f;
        constexpr float kLabelFontScale  = 1.0f;
        constexpr float kValueFontScale  = 1.15f;
        constexpr float kRecordFontScale = 0.8f;
        constexpr float kSkillsFontScale = 0.85f;
        constexpr float kBestFontScale   = 0.8f;

        const hlslpp::float4 kBackdropColor   = hlslpp::float4(0.0f, 0.0f, 0.0f, 0.72f);
        const hlslpp::float4 kDeadTitleColor  = hlslpp::float4(1.0f, 0.35f, 0.30f, 1.0f);
        const hlslpp::float4 kClearTitleColor = hlslpp::float4(1.0f, 0.88f, 0.35f, 1.0f);
        const hlslpp::float4 kLabelColor      = hlslpp::float4(0.75f, 0.75f, 0.78f, 1.0f);
        const hlslpp::float4 kValueColor      = hlslpp::float4(1.0f, 1.0f, 1.0f, 1.0f);
        const hlslpp::float4 kRecordColor     = hlslpp::float4(1.0f, 0.88f, 0.30f, 1.0f);
        const hlslpp::float4 kSkillsColor     = hlslpp::float4(0.92f, 0.92f, 0.92f, 1.0f);
        const hlslpp::float4 kBestColor       = hlslpp::float4(0.70f, 0.70f, 0.74f, 1.0f);

        //-------------------------------------------------------------
        //! @brief  秒数を「分:秒」の文字列にする
        //! @param  seconds [in] 秒数
        //! @return 例：「9:05」
        //-------------------------------------------------------------
        [[nodiscard]]
        std::wstring FormatMinutesSeconds(float seconds) {
            const int totalSeconds = std::max(static_cast<int>(seconds), 0);

            std::wstring secondsText = std::to_wstring(totalSeconds % 60);
            if(secondsText.size() < 2)
                secondsText.insert(0, L"0");

            return std::to_wstring(totalSeconds / 60) + L":" + secondsText;
        }

        //-------------------------------------------------------------
        //! @brief  取ったスキルの一覧を1行にする
        //! @param  skills [in] プレイヤーの取得状況
        //! @return 例：「スキル　強欲 Lv2　憤怒 Lv1」。1つも取っていなければ「スキル　なし」
        //-------------------------------------------------------------
        [[nodiscard]]
        std::wstring FormatSkills(const PlayerSkillComponent& skills) {
            std::wstring text = L"スキル";
            bool         any  = false;

            for(size_t i = 0; i < skills.levels.size(); ++i) {
                const int level = skills.levels[i];
                if(level <= 0)
                    continue;

                text += L"　";
                text += GetSkillEntry(static_cast<SkillId>(i)).displayName;
                text += L" Lv" + std::to_wstring(level);
                any = true;
            }

            if(!any)
                text += L"　なし";

            return text;
        }

        //-------------------------------------------------------------
        //! @brief  成績1行を出す
        //! @param  registry  [in] ECSレジストリ
        //! @param  row       [in] 対象の行
        //! @param  centerX   [in] 画面中心のX
        //! @param  y         [in] 行のY
        //! @param  label     [in] 項目名
        //! @param  value     [in] 値
        //! @param  newRecord [in] 記録を更新したか（「NEW RECORD」を出すか）
        //-------------------------------------------------------------
        void PlaceStatRow(Tsukino::ECS::Registry& registry, const RunResultStatRow& row, float centerX, float y, const std::wstring& label,
                          const std::wstring& value, bool newRecord) {
            PlaceUiText(registry, row.labelEntity, centerX + kLabelOffsetX, y, kLabelFontScale, label, kLabelColor);
            PlaceUiText(registry, row.valueEntity, centerX + kValueOffsetX, y, kValueFontScale, value, kValueColor);

            if(newRecord)
                PlaceUiText(registry, row.recordEntity, centerX + kRecordOffsetX, y, kRecordFontScale, L"NEW RECORD", kRecordColor);
            else
                HideUiText(registry, row.recordEntity);
        }

        //-------------------------------------------------------------
        //! @brief  リザルト画面を今の画面サイズで組む
        //! @param  registry [in]     ECSレジストリ
        //! @param  ctx      [in]     エンジンコンテキスト
        //! @param  result   [in,out] 対象のリザルト。保存済みの表示内容から配置し、配置した画面サイズを書き戻す
        //! @note   配置は画面中心からのオフセットなので、ウィンドウサイズが変わるたびに呼び直す。
        //!         最大化や元に戻す操作の後も、暗転板が画面全体を覆い、文字が中央に収まる
        //-------------------------------------------------------------
        void LayoutResultScreen(Tsukino::ECS::Registry& registry, Tsukino::EngineIntegration::EngineContext& ctx, RunResultComponent& result) {
            const float screenWidth   = ctx.window ? static_cast<float>(ctx.window->GetWidth()) : 1700.0f;
            const float screenHeight  = ctx.window ? static_cast<float>(ctx.window->GetHeight()) : 1000.0f;
            const float screenCenterX = screenWidth * 0.5f;
            const float screenCenterY = screenHeight * 0.5f;

            result.layoutWidth  = screenWidth;
            result.layoutHeight = screenHeight;

            StretchSprite(registry, ctx, result.backdropEntity, screenCenterX, screenCenterY, screenWidth, screenHeight, kBackdropColor);

            PlaceUiText(registry, result.titleEntity, screenCenterX, screenCenterY + kTitleOffsetY, kTitleFontScale,
                        result.cleared ? L"CLEAR" : L"GAME OVER", result.cleared ? kClearTitleColor : kDeadTitleColor);

            const std::array<std::wstring, kRunResultStatRowCount> labels = {L"生存時間", L"撃破数", L"到達レベル", L"危険度"};
            for(int i = 0; i < kRunResultStatRowCount; ++i) {
                const float rowY = screenCenterY + kFirstRowOffsetY + kRowPitch * static_cast<float>(i);
                PlaceStatRow(registry, result.statRows[i], screenCenterX, rowY, labels[i], result.statValues[i], result.statNewRecords[i]);
            }

            if(!result.skillsText.empty())
                PlaceUiText(registry, result.skillsEntity, screenCenterX, screenCenterY + kSkillsOffsetY, kSkillsFontScale, result.skillsText,
                            kSkillsColor);

            PlaceUiText(registry, result.bestEntity, screenCenterX, screenCenterY + kBestOffsetY, kBestFontScale, result.bestText, kBestColor);

            ShowGameMenu(registry, ctx, result.menu, screenCenterX, screenCenterY + kMenuTopOffsetY, kMenuLabels, result.cursorIndex);
        }
    }    // namespace

    //-------------------------------------------------------------
    //! @brief 走行が終わったかを問い合わせる
    //-------------------------------------------------------------
    bool IsRunEnded(Tsukino::ECS::Registry& registry) {
        auto view = registry.View<RunResultComponent>();
        for(entt::entity entity : view)
            return view.get<RunResultComponent>(entity).outcome != RunOutcome::None;
        return false;
    }

    //-------------------------------------------------------------
    //! @brief リザルトで進行を止めているかを問い合わせる
    //-------------------------------------------------------------
    bool IsRunResultFreezing(Tsukino::ECS::Registry& registry) {
        auto view = registry.View<RunResultComponent>();
        for(entt::entity entity : view) {
            const RunResultComponent& result = view.get<RunResultComponent>(entity);
            return result.outcome == RunOutcome::Cleared || result.shown;
        }
        return false;
    }

    //-------------------------------------------------------------
    //! @brief EnemyDiedEventの購読を開始する
    //-------------------------------------------------------------
    void RunResultSystem::Initialize(Tsukino::ECS::EventBus& eventBus) {
        // ハンドラは数を積むだけ。コンポーネントへの反映はUpdateで行う
        //（EnemyDiedEventはビヘイビアツリーのView反復中にPublishされ、Registryも渡ってこないため）
        m_diedConnection = eventBus.Subscribe<EnemyDiedEvent>([this](const EnemyDiedEvent&) { ++m_pendingKills; });
    }

    //-------------------------------------------------------------
    //! @brief システムの更新
    //-------------------------------------------------------------
    void RunResultSystem::Update(Tsukino::ECS::Registry& registry, float deltaTime) {
        auto* ctx = registry.GetContext<Tsukino::EngineIntegration::EngineContext*>();
        if(!ctx)
            return;

        // 待ち時間は実時間で数える（クリア後はシーンがdeltaTimeを0にするため）
        const float realDeltaTime =
            registry.HasContext<WorldTimeContext>() ? registry.GetContext<WorldTimeContext>().realDeltaTime : deltaTime;

        const int newKills = m_pendingKills;
        m_pendingKills     = 0;

        auto view = registry.View<PlayerComponent, HealthComponent, RunClockComponent, RunResultComponent>();
        for(entt::entity entity : view) {
            PlayerComponent&       player = view.get<PlayerComponent>(entity);
            const HealthComponent& health = view.get<HealthComponent>(entity);
            RunClockComponent&     clock  = view.get<RunClockComponent>(entity);
            RunResultComponent&    result = view.get<RunResultComponent>(entity);

            //-------------------------------------------------------------
            // 走行中：撃破数を数え、終わり（死亡・クリア）を判定する。
            // 死亡を先に見るのは、クリアの瞬間と同じフレームに倒れた場合を死亡として扱うため
            //-------------------------------------------------------------
            if(result.outcome == RunOutcome::None) {
                result.killCount += newKills;

                if(health.isDead) {
                    result.outcome = RunOutcome::Dead;
                } else if(clock.elapsedSeconds >= kRunClearSeconds) {
                    result.outcome       = RunOutcome::Cleared;
                    clock.elapsedSeconds = kRunClearSeconds;    // 越えた端数はHUDにもリザルトにも出さない

                    // ここから世界を止める（IsRunResultFreezing）。止めた後に残った攻撃判定で
                    // 倒されないよう無敵にし、ヒットストップと移動入力も片付ける
                    player.isInvincible = true;
                    ClearAllHitStop(registry);
                    SuppressAllMoveInput(registry);
                } else {
                    continue;
                }

                result.endElapsed = 0.0f;
            }

            if(result.outcome == RunOutcome::Cleared)
                SuppressAllMoveInput(registry);

            result.endElapsed += realDeltaTime;

            //-------------------------------------------------------------
            // リザルトを出す。記録の反映と保存はこの1回だけ行う
            //-------------------------------------------------------------
            if(!result.shown) {
                const bool  cleared = result.outcome == RunOutcome::Cleared;
                const float delay   = cleared ? kClearOverlayDelay : kDeadOverlayDelay;
                if(result.endElapsed < delay)
                    continue;

                result.shown           = true;
                result.openedThisFrame = true;
                result.cursorIndex     = 0;

                PlaySound(registry, cleared ? SoundId::RunClear : SoundId::RunFailed);

                const int level = registry.HasComponent<PlayerExperienceComponent>(entity)
                                      ? registry.GetComponent<PlayerExperienceComponent>(entity).level
                                      : 1;

                RunRecord             record = LoadRunRecord();
                const RunRecordUpdate update = ApplyRunResult(record, clock.elapsedSeconds, result.killCount, level, cleared);
                SaveRunRecord(record);

                // 死亡時は動いていた世界をここで止めるので、ヒットストップと移動入力を片付ける
                ClearAllHitStop(registry);
                SuppressAllMoveInput(registry);

                //-------------------------------------------------------------
                // 表示内容を確定して保存し、画面を組む。
                // 配置はウィンドウサイズが変わるたびに組み直すので、内容はコンポーネントに残す
                //-------------------------------------------------------------
                result.cleared        = cleared;
                result.statValues     = {
                    FormatMinutesSeconds(clock.elapsedSeconds),
                    std::to_wstring(result.killCount),
                    L"Lv " + std::to_wstring(level),
                    std::to_wstring(clock.dangerRank),
                };
                result.statNewRecords = {update.survivalSeconds, update.kills, update.level, false};

                if(registry.HasComponent<PlayerSkillComponent>(entity))
                    result.skillsText = FormatSkills(registry.GetComponent<PlayerSkillComponent>(entity));
                else
                    result.skillsText.clear();

                result.bestText = L"ベスト　生存 " + FormatMinutesSeconds(record.bestSurvivalSeconds) + L"　撃破 "
                                  + std::to_wstring(record.bestKills) + L"　Lv " + std::to_wstring(record.bestLevel)
                                  + L"　クリア " + std::to_wstring(record.clearCount) + L"回";

                LayoutResultScreen(registry, *ctx, result);
                continue;    // 表示した直後のフレームでそのまま決定入力を拾わない
            }

            //-------------------------------------------------------------
            // ウィンドウサイズが変わっていたら組み直す（最大化・元に戻すなど）。
            // 表示した直後のフレームは上で組んだばかりなので、ここへは来ない
            //-------------------------------------------------------------
            const float currentWidth  = ctx->window ? static_cast<float>(ctx->window->GetWidth()) : 1700.0f;
            const float currentHeight = ctx->window ? static_cast<float>(ctx->window->GetHeight()) : 1000.0f;
            if(currentWidth != result.layoutWidth || currentHeight != result.layoutHeight)
                LayoutResultScreen(registry, *ctx, result);

            if(result.openedThisFrame) {
                result.openedThisFrame = false;
                continue;
            }

            if(!ctx->inputSystem)
                continue;

            //-------------------------------------------------------------
            // メニュー操作
            //-------------------------------------------------------------
            const Tsukino::Input::InputSystem& input = *ctx->inputSystem;

            const int step = ReadGameMenuStep(input);
            if(step != 0) {
                const int nextIndex = std::clamp(result.cursorIndex + step, 0, static_cast<int>(ResultMenuItem::Count) - 1);
                if(nextIndex != result.cursorIndex) {
                    result.cursorIndex = nextIndex;
                    PlaySound(registry, SoundId::MenuMove);

                    LayoutResultScreen(registry, *ctx, result);
                }
            }

            // 暗転が始まったら、もう選択は受け付けない（連打で二重に切り替えないため）
            if(!IsGameMenuConfirmPressed(input) || !ctx->gameSceneManager || IsScreenFadingOut(registry))
                continue;

            PlaySound(registry, SoundId::MenuConfirm);

            // 黒く覆ってから切り替える。実際のChangeSceneは暗転しきった時点で
            // ScreenFadeSystemが呼ぶ（リトライでは操作の案内を出さない）
            if(static_cast<ResultMenuItem>(result.cursorIndex) == ResultMenuItem::Title)
                RequestSceneChangeWithFade(registry, []() { return std::make_unique<CombatAndroid::TitleScene>(); });
            else
                RequestSceneChangeWithFade(registry, []() { return std::make_unique<CombatAndroid::CombatAndroidScene>(); });
        }
    }
}    // namespace CombatAndroid::ECS
