//-------------------------------------------------------------
//! @file   TutorialSystem.cpp
//! @brief  TutorialSystemクラスの実装
//-------------------------------------------------------------
#include <CombatAndroid/ECS/System/UI/TutorialSystem.hpp>
#include <CombatAndroid/ECS/Serialization/Common/SerializationHelper.hpp>
#include <CombatAndroid/ECS/Utility/Table/TableJson.hpp>
#include <Tsukino/Core/Math/Serialization/HlslppSerialization.hpp>
#include <CombatAndroid/ECS/Component/Player/PlayerAnimationSetComponent.hpp>
#include <CombatAndroid/ECS/Component/Player/PlayerComponent.hpp>
#include <CombatAndroid/ECS/Component/Progression/RunClockComponent.hpp>
#include <CombatAndroid/ECS/Component/UI/TutorialComponent.hpp>
#include <CombatAndroid/ECS/Component/Weapon/WeaponComponent.hpp>
#include <CombatAndroid/ECS/Utility/Asset/GamePrefab.hpp>
#include <CombatAndroid/ECS/Utility/Time/GameplayFreeze.hpp>
#include <CombatAndroid/ECS/Utility/UI/UiSprite.hpp>
#include <CombatAndroid/ECS/Utility/UI/UiMotion.hpp>
#include <CombatAndroid/UI/UiSortOrder.hpp>
#include <CombatAndroid/ECS/Utility/UI/UiTextSize.hpp>

#include <Tsukino/EngineIntegration/EngineContext.hpp>

#include <Tsukino/Core/Window.hpp>

#include <entt/entt.hpp>

#include <algorithm>
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
            {L"落ちている武器に近づくと拾える", nullptr, false, false},
            {L"マウスホイールで武器を切り替える", nullptr, true, false},
            {L"迫りくる群れを生き延び、夜の主を討て！", nullptr, false, false},
        };
        static_assert(std::size(kStepDefs) == static_cast<size_t>(TutorialStep::Count), "kStepDefsはTutorialStepの並びと揃えること");

        //! 「操作 n / 5」の分母（目標の表示は操作ではないので数えない）
        constexpr int kOperationStepCount = static_cast<int>(TutorialStep::Goal);

        //! バトルアックスを拾ったときに出す、溜め攻撃の案内（手順の流れとは別。番号は付けない）
        const TutorialStepDef kChargeHintDef = {L"バトルアックスは左クリック長押しで溜め攻撃（ピンクで最大威力）", nullptr, true, true};

        //! 溜め攻撃の案内を消す溜め段階（3＝最大。リムライトとゲージがピンク〜紫になる）
        constexpr int kChargeHintClearStage = 3;

        //-------------------------------------------------------------
        //! @struct TutorialParams
        //! @brief  見た目と挙動のチューニング値（Assets/Tables/Systems/Tutorial.json。ここの初期値はJSONにキーが無いときの既定値）
        //-------------------------------------------------------------
        struct TutorialParams {
            //-------------------------------------------------------------
            // 進み方のチューニング値（秒）
            //-------------------------------------------------------------
            float startDelay = 1.0f;     //!< 走行が始まってから最初の案内を出すまで
            float stepTimeout = 12.0f;    //!< できなくてもこの秒数で次へ進む（案内で足止めしない）
            float doneHold = 0.7f;     //!< できたときに「OK」を出しておく時間
            float moveRequired = 0.6f;     //!< 移動の手順を「できた」とみなす歩いた合計時間
            float goalDuration = 3.5f;     //!< 最後の目標を出しておく時間

            //-------------------------------------------------------------
            // レイアウト（画面下中央。HUDとスキル選択の間の層に置く）
            //-------------------------------------------------------------
            float panelWidth = 860.0f;
            float panelHeight = 84.0f;
            float panelBottomMargin = 170.0f;    //!< 画面下端から板の中心まで
            float promptInsetX = 70.0f;     //!< 板の左端からキー表示の中心まで
            float textInsetX = 150.0f;    //!< 板の左端から文の左端まで
            float textPaddingRight = 40.0f;    //!< 文の右端から板の右端まで空ける幅（文が長いときは板をこれが空くまで広げる）
            float counterOffsetY = -60.0f;    //!< 板の中心から見た「操作 n / 5」のY
            float stackOffsetY = -130.0f;     //!< 手順の案内が出ているとき、溜め攻撃の案内をその板の中心からこれだけずらして積む

            hlslpp::float4 panelColor = hlslpp::float4(0.02f, 0.02f, 0.03f, 0.72f);    //!< 取得ログと同じ黒い半透明
            hlslpp::float4 textColor = hlslpp::float4(1.0f, 1.0f, 1.0f, 1.0f);
            hlslpp::float4 doneColor = hlslpp::float4(0.55f, 1.0f, 0.55f, 1.0f);     //!< できたときの緑
            hlslpp::float4 goalColor = hlslpp::float4(1.0f, 0.85f, 0.45f, 1.0f);     //!< タイトルの副題と同じ金
            hlslpp::float4 counterColor = hlslpp::float4(0.75f, 0.75f, 0.80f, 1.0f);
        };

        template <class Archive>
        void load(Archive& archive, TutorialParams& params) {
            LoadField(archive, "startDelay", params.startDelay);
            LoadField(archive, "stepTimeout", params.stepTimeout);
            LoadField(archive, "doneHold", params.doneHold);
            LoadField(archive, "moveRequired", params.moveRequired);
            LoadField(archive, "goalDuration", params.goalDuration);
            LoadField(archive, "panelWidth", params.panelWidth);
            LoadField(archive, "panelHeight", params.panelHeight);
            LoadField(archive, "panelBottomMargin", params.panelBottomMargin);
            LoadField(archive, "promptInsetX", params.promptInsetX);
            LoadField(archive, "textInsetX", params.textInsetX);
            LoadField(archive, "textPaddingRight", params.textPaddingRight);
            LoadField(archive, "counterOffsetY", params.counterOffsetY);
            LoadField(archive, "stackOffsetY", params.stackOffsetY);
            LoadField(archive, "panelColor", params.panelColor);
            LoadField(archive, "textColor", params.textColor);
            LoadField(archive, "doneColor", params.doneColor);
            LoadField(archive, "goalColor", params.goalColor);
            LoadField(archive, "counterColor", params.counterColor);
        }

        //-------------------------------------------------------------
        //! @brief  チューニング値を得る関数（初回の呼び出しで1度だけ読む）
        //-------------------------------------------------------------
        const TutorialParams& GetParams() {
            static const TutorialParams s_params = LoadSystemParams<TutorialParams>("Tutorial");
            return s_params;
        }

        //! 1文字の幅を見積もる基準（.dfont の Size。UiTextSize.cpp の基準と同じ）
        constexpr float kFontBasePixels = 32.0f;

        //! 「OK!」を足した文。板の幅はこちらで測り、OKが付いた瞬間に板が伸び縮みしないようにする
        constexpr const wchar_t* kDoneSuffix = L"　OK!";

        //-------------------------------------------------------------
        //! @brief  文の画面上の幅を見積もる（全角は1文字ぶん、ASCIIは半分）
        //! @note   フォントから実測する仕組みは無いので、板からはみ出さない程度の目安として使う
        //-------------------------------------------------------------
        [[nodiscard]]
        float EstimateTextWidth(const std::wstring& text, float scale) {
            float ems = 0.0f;
            for(wchar_t c : text)
                ems += (c < 0x80) ? 0.5f : 1.0f;
            return ems * kFontBasePixels * scale;
        }

        //-------------------------------------------------------------
        //! @brief  文が収まる板の幅（既定の幅より狭くはしない）
        //-------------------------------------------------------------
        [[nodiscard]]
        float FitPanelWidth(const wchar_t* text) {
            const TutorialParams& params = GetParams();
            const float textWidth = EstimateTextWidth(std::wstring(text) + kDoneSuffix, GetUiTextScale(UiTextSize::Body));
            return std::max(params.panelWidth, params.textInsetX + textWidth + params.textPaddingRight);
        }

        //-------------------------------------------------------------
        //! @brief  手順の案内を隠す
        //-------------------------------------------------------------
        void HideSteps(Tsukino::ECS::Registry& registry, TutorialComponent& tutorial) {
            HideUiSprite(registry, tutorial.panelEntity);
            HideUiText(registry, tutorial.textEntity);
            HideUiText(registry, tutorial.counterEntity);
            for(const InputPromptWidget& prompt : tutorial.prompts)
                HideInputPrompt(registry, prompt);

            // 次に出すときは、また滑り込ませる（フェードの途中で隠したなら、出し直してからフェードし直す）
            tutorial.shownStep      = -1;
            tutorial.stepsFadingOut = false;
        }

        //-------------------------------------------------------------
        //! @brief  溜め攻撃の案内を隠す
        //-------------------------------------------------------------
        void HideChargeHint(Tsukino::ECS::Registry& registry, TutorialComponent& tutorial) {
            HideUiSprite(registry, tutorial.chargeHintPanel);
            HideUiText(registry, tutorial.chargeHintText);
            HideInputPrompt(registry, tutorial.chargeHintPrompt);

            tutorial.chargeHintShown  = false;
            tutorial.chargeHintFading = false;
        }

        //-------------------------------------------------------------
        //! @brief  案内を全部隠す
        //-------------------------------------------------------------
        void HideAll(Tsukino::ECS::Registry& registry, TutorialComponent& tutorial) {
            HideSteps(registry, tutorial);
            HideChargeHint(registry, tutorial);
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
            const TutorialParams& params = GetParams();

            switch(tutorial.step) {
            case TutorialStep::Move:
                if(anim.currentState == PlayerAnimState::Run || anim.currentState == PlayerAnimState::FastRun)
                    tutorial.moveTime += deltaTime;
                return tutorial.moveTime >= params.moveRequired;
            case TutorialStep::Attack:
                return anim.currentState == PlayerAnimState::Attack1 || anim.currentState == PlayerAnimState::Attack2
                       || anim.currentState == PlayerAnimState::Attack3;
            case TutorialStep::Dodge:
                return anim.currentState == PlayerAnimState::Dodge;
            case TutorialStep::Pickup:
                return static_cast<int>(player.weaponInventory.size()) > tutorial.inventorySizeAtStep;
            case TutorialStep::Switch:
                return player.selectedWeaponIndex != tutorial.selectedWeaponAtStep;
            default:
                return false;
            }
        }

        //-------------------------------------------------------------
        //! @brief  手順の案内を進めて表示する。全部終わっていれば隠すだけ
        //-------------------------------------------------------------
        void UpdateSteps(Tsukino::ECS::Registry& registry, Tsukino::EngineIntegration::EngineContext& ctx, TutorialComponent& tutorial,
                         const PlayerComponent& player, const PlayerAnimationSetComponent& anim, float deltaTime, float panelCenterX,
                         float panelCenterY) {
            const TutorialParams& params = GetParams();

            if(tutorial.finished) {
                HideSteps(registry, tutorial);
                return;
            }

            //-------------------------------------------------------------
            // 進行。できたら「OK」を少し見せてから次へ、できなくても時間切れで次へ
            //-------------------------------------------------------------
            tutorial.stepTimer += deltaTime;

            if(tutorial.step == TutorialStep::Goal) {
                if(tutorial.stepTimer >= params.goalDuration) {
                    // 目標を出し終えたら、板ごとフェードで消し、消えきってから隠す
                    if(!tutorial.stepsFadingOut) {
                        tutorial.stepsFadingOut = true;
                        UiDisappear(registry, tutorial.panelEntity, true);
                    } else if(!UiIsVisibilityAnimating(registry, tutorial.panelEntity)) {
                        tutorial.finished = true;
                        HideSteps(registry, tutorial);
                        UiResetVisibility(registry, tutorial.panelEntity);
                        return;
                    }
                }
            } else if(tutorial.doneTimer >= 0.0f) {
                tutorial.doneTimer += deltaTime;
                if(tutorial.doneTimer >= params.doneHold)
                    AdvanceStep(registry, tutorial, player);
            } else if(IsStepDone(tutorial, player, anim, deltaTime)) {
                tutorial.doneTimer = 0.0f;
                UiPunch(registry, tutorial.panelEntity, 0.6f, true);    // できた手ごたえ
            } else if(tutorial.stepTimer >= params.stepTimeout) {
                AdvanceStep(registry, tutorial, player);
            }

            //-------------------------------------------------------------
            // 表示
            //-------------------------------------------------------------
            const int              stepIndex = static_cast<int>(tutorial.step);
            const TutorialStepDef& def       = kStepDefs[stepIndex];
            const bool             isGoal    = tutorial.step == TutorialStep::Goal;
            const bool             isDone    = tutorial.doneTimer >= 0.0f;

            const float panelWidth = FitPanelWidth(def.text);
            const float panelLeft  = panelCenterX - panelWidth * 0.5f;
            StretchSprite(registry, ctx, tutorial.panelEntity, panelCenterX, panelCenterY, panelWidth, params.panelHeight, params.panelColor);

            // 手順が変わった（または隠れていた状態から戻った）ら、板を下から滑り込ませる。文字とキー表示は板に追従する
            if(tutorial.shownStep != stepIndex) {
                tutorial.shownStep = stepIndex;
                UiAppearFromBelow(registry, tutorial.panelEntity, 0.0f, true);
            }

            std::wstring text = def.text;
            if(isDone)
                text += kDoneSuffix;

            const hlslpp::float4 textColor = isGoal ? params.goalColor : (isDone ? params.doneColor : params.textColor);
            if(isGoal) {
                // 目標はキー表示が無いので、板の中央に置く
                HideUiText(registry, tutorial.textEntity);
                PlaceUiText(registry, tutorial.counterEntity, panelCenterX, panelCenterY, GetUiTextScale(UiTextSize::Body), text, textColor);
            } else {
                PlaceUiText(registry, tutorial.textEntity, panelLeft + params.textInsetX, panelCenterY, GetUiTextScale(UiTextSize::Body), text, textColor);
                PlaceUiText(registry, tutorial.counterEntity, panelCenterX, panelCenterY + params.counterOffsetY, GetUiTextScale(UiTextSize::Small),
                            L"操作 " + std::to_wstring(stepIndex + 1) + L" / " + std::to_wstring(kOperationStepCount), params.counterColor);
            }

            for(int i = 0; i < static_cast<int>(TutorialStep::Count); ++i) {
                const InputPromptWidget& prompt = tutorial.prompts[static_cast<size_t>(i)];
                if(i != stepIndex) {
                    HideInputPrompt(registry, prompt);
                    continue;
                }
                ShowInputPromptAtScreen(registry, ctx, prompt, panelLeft + params.promptInsetX, panelCenterY, InputPromptStyle{});
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

        // 溜め攻撃の案内（拾ったときに出す）
        tutorial.chargeHintPanel = CreateUiRectEntity(registry, context, UI::kTutorialPanel);
        tutorial.chargeHintText  = CreateUiTextEntity(registry, UI::kTutorialText, UiTextAlign::Left);
        {
            InputPromptDesc desc;
            desc.keyLabel                = L"";
            desc.useMouse                = kChargeHintDef.useMouse;
            desc.useHoldRing             = kChargeHintDef.useHold;
            desc.sortOrderBase           = UI::kTutorialPromptBase;
            tutorial.chargeHintPrompt    = CreateInputPromptWidget(registry, context, desc);
        }

        // 文字とキー表示は板の動き（滑り込み・弾み・フェード）に追従させる
        UiFollow(registry, tutorial.textEntity, tutorial.panelEntity);
        UiFollow(registry, tutorial.counterEntity, tutorial.panelEntity);
        for(const InputPromptWidget& prompt : tutorial.prompts)
            UiFollowPrompt(registry, prompt, tutorial.panelEntity);
        UiFollow(registry, tutorial.chargeHintText, tutorial.chargeHintPanel);
        UiFollowPrompt(registry, tutorial.chargeHintPrompt, tutorial.chargeHintPanel);

        // Prefab（System/Tutorial）は実行時状態だけの束。組み立てた中身を入れる
        registry.GetComponent<TutorialComponent>(InstantiatePrefab(registry, context, "System/Tutorial")) = tutorial;
    }

    //-------------------------------------------------------------
    //! @brief システムの更新
    //-------------------------------------------------------------
    void TutorialSystem::Update(Tsukino::ECS::Registry& registry, float deltaTime) {
        const TutorialParams& params = GetParams();

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
            if(tutorial.finished && tutorial.chargeHintState == ChargeHintState::Done)
                continue;

            //-------------------------------------------------------------
            // メニュー中は止めて隠す（暗転板の下に案内が透けないように）。
            // 走行が始まってすぐは、画面が落ち着くまで少し待つ
            //-------------------------------------------------------------
            if(!player || !anim || !clock || IsGameplayFrozen(registry) || clock->elapsedSeconds < params.startDelay) {
                HideAll(registry, tutorial);
                continue;
            }

            const float screenWidth  = ctx->window ? static_cast<float>(ctx->window->GetWidth()) : 1700.0f;
            const float screenHeight = ctx->window ? static_cast<float>(ctx->window->GetHeight()) : 1000.0f;
            const float panelCenterX = screenWidth * 0.5f;
            const float panelCenterY = screenHeight - params.panelBottomMargin;

            UpdateSteps(registry, *ctx, tutorial, *player, *anim, deltaTime, panelCenterX, panelCenterY);

            //-------------------------------------------------------------
            // 溜め攻撃の案内。手順の流れとは別に、溜め攻撃できる武器を拾った時点で出し、
            // 最大段階（ピンク）まで溜めたら「OK」を少し見せて消す。時間切れは無い。
            // 手順の案内が出ている間は、その上に積む
            //-------------------------------------------------------------
            if(tutorial.chargeHintState == ChargeHintState::Waiting && HasChargeWeapon(registry, *player))
                tutorial.chargeHintState = ChargeHintState::Showing;

            if(tutorial.chargeHintState != ChargeHintState::Showing) {
                HideChargeHint(registry, tutorial);
                continue;
            }

            const bool isCharging = anim->currentState == PlayerAnimState::Charge;
            if(tutorial.chargeHintDoneTimer >= 0.0f) {
                tutorial.chargeHintDoneTimer += deltaTime;
                if(tutorial.chargeHintDoneTimer >= params.doneHold) {
                    // 「OK!」を見せ終えたら、フェードで消してから隠す
                    if(!tutorial.chargeHintFading) {
                        tutorial.chargeHintFading = true;
                        UiDisappear(registry, tutorial.chargeHintPanel, true);
                    } else if(!UiIsVisibilityAnimating(registry, tutorial.chargeHintPanel)) {
                        tutorial.chargeHintState = ChargeHintState::Done;
                        HideChargeHint(registry, tutorial);
                        UiResetVisibility(registry, tutorial.chargeHintPanel);
                        continue;
                    }
                }
            } else if(isCharging && ResolveChargeStage(anim->chargeTimer, *player) >= kChargeHintClearStage) {
                tutorial.chargeHintDoneTimer = 0.0f;
                UiPunch(registry, tutorial.chargeHintPanel, 0.8f, true);    // ピンクまで溜めた手ごたえ
            }

            const bool  hintDone = tutorial.chargeHintDoneTimer >= 0.0f;
            const float hintY    = tutorial.finished ? panelCenterY : panelCenterY + params.stackOffsetY;

            const float hintWidth = FitPanelWidth(kChargeHintDef.text);
            const float panelLeft = panelCenterX - hintWidth * 0.5f;
            StretchSprite(registry, *ctx, tutorial.chargeHintPanel, panelCenterX, hintY, hintWidth, params.panelHeight, params.panelColor);

            //-------------------------------------------------------------
            // 出した瞬間は下から滑り込ませる。手順の案内が終わって積む位置が下がったときは、
            // 前の位置から滑らせて移す（いきなり飛ばない）
            //-------------------------------------------------------------
            if(!tutorial.chargeHintShown) {
                tutorial.chargeHintShown = true;
                UiAppearFromBelow(registry, tutorial.chargeHintPanel, 0.0f, true);
            } else if(hintY != tutorial.chargeHintLastY) {
                UiSlideFrom(registry, tutorial.chargeHintPanel, 0.0f, tutorial.chargeHintLastY - hintY, true);
            }
            tutorial.chargeHintLastY = hintY;

            std::wstring hintText = kChargeHintDef.text;
            if(hintDone)
                hintText += kDoneSuffix;
            PlaceUiText(registry, tutorial.chargeHintText, panelLeft + params.textInsetX, hintY, GetUiTextScale(UiTextSize::Body), hintText,
                        hintDone ? params.doneColor : params.textColor);

            InputPromptStyle style;
            if(isCharging)
                style.holdProgress = 1.0f;    // 溜めている間はゲージを満たして見せる
            ShowInputPromptAtScreen(registry, *ctx, tutorial.chargeHintPrompt, panelLeft + params.promptInsetX, hintY, style);
        }
    }
}    // namespace CombatAndroid::ECS
