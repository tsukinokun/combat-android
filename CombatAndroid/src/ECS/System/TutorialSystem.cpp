//-------------------------------------------------------------
//! @file   TutorialSystem.cpp
//! @brief  TutorialSystemクラスの実装
//-------------------------------------------------------------
#include <CombatAndroid/ECS/System/TutorialSystem.hpp>
#include <CombatAndroid/ECS/Component/PlayerAnimationSetComponent.hpp>
#include <CombatAndroid/ECS/Component/PlayerComponent.hpp>
#include <CombatAndroid/ECS/Component/RunClockComponent.hpp>
#include <CombatAndroid/ECS/Component/TutorialComponent.hpp>
#include <CombatAndroid/ECS/Component/WeaponComponent.hpp>
#include <CombatAndroid/ECS/Utility/GamePrefab.hpp>
#include <CombatAndroid/ECS/Utility/GameplayFreeze.hpp>
#include <CombatAndroid/ECS/Utility/UiSprite.hpp>
#include <CombatAndroid/UI/UiSortOrder.hpp>

#include <Tsukino/EngineIntegration/EngineContext.hpp>

#include <Tsukino/Core/Window.hpp>

#include <entt/entt.hpp>

#include <string>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    namespace {
        //-------------------------------------------------------------
        //! @struct TutorialStepDef
        //! @brief  手順1つぶんの見せ方
        //-------------------------------------------------------------
        struct TutorialStepDef {
            const wchar_t* text;        //!< 案内の文
            const wchar_t* keyLabel;    //!< キーキャップの文字（マウス操作ならnullptr）
            bool           useMouse;    //!< マウスの絵を出すか
            bool           useHold;     //!< 長押しゲージを添えるか
        };

        //-------------------------------------------------------------
        // 手順の表。TutorialStepの並びと揃えること。
        // 文言とキーはタイトルの操作説明（TitleMenuSystem.cppのkControlActions/kControlKeys）と揃える
        //-------------------------------------------------------------
        const TutorialStepDef kStepDefs[] = {
            {L"W A S D で移動（Shift でダッシュ）", L"WASD", false, false},
            {L"左クリックで攻撃（続けて押すと連撃）", nullptr, true, false},
            {L"Space で回避（転がっている間は無敵）", L"SPACE", false, false},
            {L"落ちている武器に近づいて F で拾う", L"F", false, false},
            {L"マウスホイールで武器を切り替える", nullptr, true, false},
            {L"バトルアックスは左クリック長押しで溜め攻撃", nullptr, true, true},
            {L"迫りくる群れの中で、10分間生き延びろ！", nullptr, false, false},
        };
        static_assert(std::size(kStepDefs) == static_cast<size_t>(TutorialStep::Count), "kStepDefsはTutorialStepの並びと揃えること");

        //! 「操作 n / 6」の分母（目標の表示は操作ではないので数えない）
        constexpr int kOperationStepCount = static_cast<int>(TutorialStep::Goal);

        //-------------------------------------------------------------
        // 進み方のチューニング値（秒）
        //-------------------------------------------------------------
        constexpr float kStartDelay   = 1.0f;     //!< 走行が始まってから最初の案内を出すまで
        constexpr float kStepTimeout  = 12.0f;    //!< できなくてもこの秒数で次へ進む（案内で足止めしない）
        constexpr float kDoneHold     = 0.7f;     //!< できたときに「OK」を出しておく時間
        constexpr float kMoveRequired = 0.6f;     //!< 移動の手順を「できた」とみなす歩いた合計時間
        constexpr float kGoalDuration = 3.5f;     //!< 最後の目標を出しておく時間

        //-------------------------------------------------------------
        // レイアウト（画面下中央。HUDとスキル選択の間の層に置く）
        //-------------------------------------------------------------
        constexpr float kPanelWidth          = 860.0f;
        constexpr float kPanelHeight         = 84.0f;
        constexpr float kPanelBottomMargin   = 170.0f;    //!< 画面下端から板の中心まで
        constexpr float kPromptInsetX        = 70.0f;     //!< 板の左端からキー表示の中心まで
        constexpr float kTextInsetX          = 150.0f;    //!< 板の左端から文の左端まで
        constexpr float kCounterOffsetY      = -60.0f;    //!< 板の中心から見た「操作 n / 6」のY
        constexpr float kTextFontScale       = 1.0f;
        constexpr float kCounterFontScale    = 0.65f;

        const hlslpp::float4 kPanelColor   = hlslpp::float4(0.02f, 0.02f, 0.03f, 0.72f);    //!< 取得ログと同じ黒い半透明
        const hlslpp::float4 kTextColor    = hlslpp::float4(1.0f, 1.0f, 1.0f, 1.0f);
        const hlslpp::float4 kDoneColor    = hlslpp::float4(0.55f, 1.0f, 0.55f, 1.0f);     //!< できたときの緑
        const hlslpp::float4 kGoalColor    = hlslpp::float4(1.0f, 0.85f, 0.45f, 1.0f);     //!< タイトルの副題と同じ金
        const hlslpp::float4 kCounterColor = hlslpp::float4(0.75f, 0.75f, 0.80f, 1.0f);

        //-------------------------------------------------------------
        //! @brief  案内を全部隠す
        //-------------------------------------------------------------
        void HideAll(Tsukino::ECS::Registry& registry, const TutorialComponent& tutorial) {
            HideUiSprite(registry, tutorial.panelEntity);
            HideUiText(registry, tutorial.textEntity);
            HideUiText(registry, tutorial.counterEntity);
            for(const InputPromptWidget& prompt : tutorial.prompts)
                HideInputPrompt(registry, prompt);
        }

        //-------------------------------------------------------------
        //! @brief  手持ちにバトルアックス（溜め攻撃できる武器）があるか
        //-------------------------------------------------------------
        [[nodiscard]]
        bool HasChargeWeapon(Tsukino::ECS::Registry& registry, const PlayerComponent& player) {
            for(Tsukino::ECS::Entity weaponEntity : player.weaponInventory) {
                if(const auto* weapon = registry.try_get<WeaponComponent>(weaponEntity); weapon && weapon->chargeAttackEnabled)
                    return true;
            }
            return false;
        }

        //-------------------------------------------------------------
        //! @brief  次の手順へ進める。こなせない手順（持ち替える先が無い等）は飛ばす
        //-------------------------------------------------------------
        void AdvanceStep(Tsukino::ECS::Registry& registry, TutorialComponent& tutorial, const PlayerComponent& player) {
            int next = static_cast<int>(tutorial.step) + 1;

            while(next < static_cast<int>(TutorialStep::Goal)) {
                const TutorialStep candidate = static_cast<TutorialStep>(next);
                if(candidate == TutorialStep::Switch && player.weaponInventory.size() < 2) {
                    ++next;    // 1本しか持っていないので切り替えようが無い
                    continue;
                }
                if(candidate == TutorialStep::Charge && !HasChargeWeapon(registry, player)) {
                    ++next;    // 溜め攻撃できる武器を持っていない
                    continue;
                }
                break;
            }

            tutorial.step                 = static_cast<TutorialStep>(next);
            tutorial.stepTimer            = 0.0f;
            tutorial.doneTimer            = -1.0f;
            tutorial.moveTime             = 0.0f;
            tutorial.inventorySizeAtStep  = static_cast<int>(player.weaponInventory.size());
            tutorial.selectedWeaponAtStep = player.selectedWeaponIndex;
        }

        //-------------------------------------------------------------
        //! @brief  今の手順をこなしたか
        //-------------------------------------------------------------
        [[nodiscard]]
        bool IsStepDone(TutorialComponent& tutorial, const PlayerComponent& player, const PlayerAnimationSetComponent& anim, float deltaTime) {
            switch(tutorial.step) {
            case TutorialStep::Move:
                if(anim.currentState == PlayerAnimState::Run || anim.currentState == PlayerAnimState::FastRun)
                    tutorial.moveTime += deltaTime;
                return tutorial.moveTime >= kMoveRequired;
            case TutorialStep::Attack:
                return anim.currentState == PlayerAnimState::Attack1 || anim.currentState == PlayerAnimState::Attack2
                       || anim.currentState == PlayerAnimState::Attack3;
            case TutorialStep::Dodge:
                return anim.currentState == PlayerAnimState::Dodge;
            case TutorialStep::Pickup:
                return static_cast<int>(player.weaponInventory.size()) > tutorial.inventorySizeAtStep;
            case TutorialStep::Switch:
                return player.selectedWeaponIndex != tutorial.selectedWeaponAtStep;
            case TutorialStep::Charge:
                return anim.currentState == PlayerAnimState::Charge;
            default:
                return false;
            }
        }
    }    // namespace

    //-------------------------------------------------------------
    //! @brief 案内の状態とUI一式を作る
    //-------------------------------------------------------------
    void CreateTutorial(Tsukino::ECS::Registry& registry, Tsukino::EngineIntegration::EngineContext& context) {
        TutorialComponent tutorial;
        tutorial.panelEntity   = CreateUiRectEntity(registry, context, UI::kTutorialPanel);
        tutorial.textEntity    = CreateUiTextEntity(registry, UI::kTutorialText, UiTextAlign::Left);
        tutorial.counterEntity = CreateUiTextEntity(registry, UI::kTutorialText, UiTextAlign::Center);

        for(int i = 0; i < static_cast<int>(TutorialStep::Count); ++i) {
            const TutorialStepDef& def = kStepDefs[i];
            if(!def.keyLabel && !def.useMouse)
                continue;    // キー表示の無い手順（目標）は作らない

            InputPromptDesc desc;
            desc.keyLabel      = def.keyLabel ? def.keyLabel : L"";
            desc.useMouse      = def.useMouse;
            desc.useHoldRing   = def.useHold;
            desc.sortOrderBase = UI::kTutorialPromptBase;
            tutorial.prompts[static_cast<size_t>(i)] = CreateInputPromptWidget(registry, context, desc);
        }

        // Prefab（System/Tutorial）は実行時状態だけの束。組み立てた中身を入れる
        registry.GetComponent<TutorialComponent>(InstantiatePrefab(registry, context, "System/Tutorial")) = tutorial;
    }

    //-------------------------------------------------------------
    //! @brief システムの更新
    //-------------------------------------------------------------
    void TutorialSystem::Update(Tsukino::ECS::Registry& registry, float deltaTime) {
        auto* ctx = registry.GetContext<Tsukino::EngineIntegration::EngineContext*>();
        if(!ctx)
            return;

        //-------------------------------------------------------------
        // プレイヤー（単一プレイヤー前提）
        //-------------------------------------------------------------
        const PlayerComponent*             player = nullptr;
        const PlayerAnimationSetComponent* anim   = nullptr;
        const RunClockComponent*           clock  = nullptr;
        auto playerView = registry.View<PlayerComponent, PlayerAnimationSetComponent, RunClockComponent>();
        for(auto entity : playerView) {
            player = &playerView.get<PlayerComponent>(entity);
            anim   = &playerView.get<PlayerAnimationSetComponent>(entity);
            clock  = &playerView.get<RunClockComponent>(entity);
            break;
        }

        auto view = registry.View<TutorialComponent>();
        for(auto entity : view) {
            TutorialComponent& tutorial = view.get<TutorialComponent>(entity);
            if(tutorial.finished)
                continue;

            //-------------------------------------------------------------
            // メニュー中は止めて隠す（暗転板の下に案内が透けないように）。
            // 走行が始まってすぐは、画面が落ち着くまで少し待つ
            //-------------------------------------------------------------
            if(!player || !anim || !clock || IsGameplayFrozen(registry) || clock->elapsedSeconds < kStartDelay) {
                HideAll(registry, tutorial);
                continue;
            }

            //-------------------------------------------------------------
            // 進行。できたら「OK」を少し見せてから次へ、できなくても時間切れで次へ
            //-------------------------------------------------------------
            tutorial.stepTimer += deltaTime;

            if(tutorial.step == TutorialStep::Goal) {
                if(tutorial.stepTimer >= kGoalDuration) {
                    tutorial.finished = true;
                    HideAll(registry, tutorial);
                    continue;
                }
            } else if(tutorial.doneTimer >= 0.0f) {
                tutorial.doneTimer += deltaTime;
                if(tutorial.doneTimer >= kDoneHold)
                    AdvanceStep(registry, tutorial, *player);
            } else if(IsStepDone(tutorial, *player, *anim, deltaTime)) {
                tutorial.doneTimer = 0.0f;
            } else if(tutorial.stepTimer >= kStepTimeout) {
                AdvanceStep(registry, tutorial, *player);
            }

            //-------------------------------------------------------------
            // 表示
            //-------------------------------------------------------------
            const float screenWidth  = ctx->window ? static_cast<float>(ctx->window->GetWidth()) : 1700.0f;
            const float screenHeight = ctx->window ? static_cast<float>(ctx->window->GetHeight()) : 1000.0f;
            const float panelCenterX = screenWidth * 0.5f;
            const float panelCenterY = screenHeight - kPanelBottomMargin;
            const float panelLeft    = panelCenterX - kPanelWidth * 0.5f;

            const int                stepIndex = static_cast<int>(tutorial.step);
            const TutorialStepDef&   def       = kStepDefs[stepIndex];
            const bool               isGoal    = tutorial.step == TutorialStep::Goal;
            const bool               isDone    = tutorial.doneTimer >= 0.0f;

            StretchSprite(registry, *ctx, tutorial.panelEntity, panelCenterX, panelCenterY, kPanelWidth, kPanelHeight, kPanelColor);

            std::wstring text = def.text;
            if(isDone)
                text += L"　OK!";

            const hlslpp::float4 textColor = isGoal ? kGoalColor : (isDone ? kDoneColor : kTextColor);
            if(isGoal) {
                // 目標はキー表示が無いので、板の中央に置く
                HideUiText(registry, tutorial.textEntity);
                PlaceUiText(registry, tutorial.counterEntity, panelCenterX, panelCenterY, kTextFontScale, text, textColor);
            } else {
                PlaceUiText(registry, tutorial.textEntity, panelLeft + kTextInsetX, panelCenterY, kTextFontScale, text, textColor);
                PlaceUiText(registry, tutorial.counterEntity, panelCenterX, panelCenterY + kCounterOffsetY, kCounterFontScale,
                            L"操作 " + std::to_wstring(stepIndex + 1) + L" / " + std::to_wstring(kOperationStepCount), kCounterColor);
            }

            for(int i = 0; i < static_cast<int>(TutorialStep::Count); ++i) {
                const InputPromptWidget& prompt = tutorial.prompts[static_cast<size_t>(i)];
                if(i != stepIndex) {
                    HideInputPrompt(registry, prompt);
                    continue;
                }

                InputPromptStyle style;
                if(def.useHold && anim->currentState == PlayerAnimState::Charge)
                    style.holdProgress = 1.0f;    // 溜めている間はゲージを満たして見せる
                ShowInputPromptAtScreen(registry, *ctx, prompt, panelLeft + kPromptInsetX, panelCenterY, style);
            }
        }
    }
}    // namespace CombatAndroid::ECS
