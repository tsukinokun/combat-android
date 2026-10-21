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
#include <CombatAndroid/ECS/Component/Weapon/PickupComponent.hpp>
#include <CombatAndroid/ECS/Component/Weapon/WeaponComponent.hpp>
#include <CombatAndroid/ECS/Utility/Asset/GamePrefab.hpp>
#include <CombatAndroid/ECS/Utility/Time/GameplayFreeze.hpp>
#include <CombatAndroid/ECS/Utility/UI/UiSprite.hpp>
#include <CombatAndroid/ECS/Utility/UI/UiMotion.hpp>
#include <CombatAndroid/UI/UiSortOrder.hpp>
#include <CombatAndroid/ECS/Utility/UI/UiTextSize.hpp>
#include <CombatAndroid/ECS/Utility/Input/GameInput.hpp>

#include <Tsukino/Core/Input/InputSystem.hpp>

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
            const wchar_t* text;           //!< 案内の文。{操作名} は最後に触った機器の表記へ置き換える（FormatInputText）
            GameAction     action;         //!< キー表示で案内する操作（Countなら下の固定の文字を使う）
            const wchar_t* keyLabel;       //!< 操作1つで表せないときのキーキャップの文字（キーボード・マウス）
            const wchar_t* padKeyLabel;    //!< 同じくパッドのときの文字
            bool           useMouse;       //!< キーボード・マウスのときにマウスの絵を出すか
            bool           useHold;        //!< 長押しゲージを添えるか

            //! キー表示があるか
            [[nodiscard]]
            bool HasPrompt() const {
                return action != GameAction::Count || keyLabel || useMouse;
            }
        };

        //-------------------------------------------------------------
        // 手順の表。TutorialStepの並びと揃えること。
        // 文言はタイトルの操作説明（TitleMenuSystem.cppのkControlActions/kControlKeys）と揃える
        //-------------------------------------------------------------
        const TutorialStepDef kStepDefs[] = {
            {L"{Move} で移動（{Dash} でダッシュ）", GameAction::Count, L"WASD", L"Lスティック", false, false},
            {L"{Attack}で攻撃（続けて押すと連撃）", GameAction::Attack, nullptr, nullptr, false, false},
            {L"{Dodge} で回避（転がっている間は無敵）", GameAction::Dodge, nullptr, nullptr, false, false},
            {L"迫りくる群れを生き延び、夜の主を討て！", GameAction::Count, nullptr, nullptr, false, false},
        };
        static_assert(std::size(kStepDefs) == static_cast<size_t>(TutorialStep::Count), "kStepDefsはTutorialStepの並びと揃えること");

        //! 「操作 n / 3」の分母（目標の表示は操作ではないので数えない）
        constexpr int kOperationStepCount = static_cast<int>(TutorialStep::Goal);

        //-------------------------------------------------------------
        // 場面の案内の表。TutorialHintの並びと揃えること。
        // 開始時は武器が落ちていないので、武器まわりの操作はその場面が来たときに教える
        //-------------------------------------------------------------
        const TutorialStepDef kHintDefs[] = {
            {L"光っている武器に触れると拾える（同じ武器ならレベルアップ）", GameAction::Count, nullptr, nullptr, false, false},
            {L"{WeaponSwitch}で武器を切り替える", GameAction::Count, nullptr, L"LB/RB", true, false},
            {L"バトルアックスは{Attack}長押しで溜め攻撃（ピンクで最大威力）", GameAction::Attack, nullptr, nullptr, false, true},
        };
        static_assert(std::size(kHintDefs) == static_cast<size_t>(TutorialHint::Count), "kHintDefsはTutorialHintの並びと揃えること");

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
        //! @brief  チューニング値を得る関数（初回の呼び出しで読む。Debugでは保存し直すと読み直す）
        //-------------------------------------------------------------
        const TutorialParams& GetParams() {
            return GetSystemParams<TutorialParams>("Tutorial");
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
        float FitPanelWidth(const std::wstring& text) {
            const TutorialParams& params = GetParams();
            const float textWidth = EstimateTextWidth(text + kDoneSuffix, GetUiTextScale(UiTextSize::Body));
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
        //! @brief  場面の案内を隠す
        //-------------------------------------------------------------
        void HideHint(Tsukino::ECS::Registry& registry, TutorialComponent& tutorial) {
            HideUiSprite(registry, tutorial.hintPanel);
            HideUiText(registry, tutorial.hintText);
            for(const InputPromptWidget& prompt : tutorial.hintPrompts)
                HideInputPrompt(registry, prompt);

            tutorial.hintShown  = false;
            tutorial.hintFading = false;
        }

        //-------------------------------------------------------------
        //! @brief  案内を全部隠す
        //-------------------------------------------------------------
        void HideAll(Tsukino::ECS::Registry& registry, TutorialComponent& tutorial) {
            HideSteps(registry, tutorial);
            HideHint(registry, tutorial);
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
        //! @brief  手持ちの武器レベルの合計（同じ種類を拾うとレベルが上がるので、拾ったかの判定に使う）
        //-------------------------------------------------------------
        [[nodiscard]]
        int SumWeaponLevels(Tsukino::ECS::Registry& registry, const PlayerComponent& player) {
            int sum = 0;
            for(Tsukino::ECS::Entity weaponEntity : player.weaponInventory) {
                if(const auto* weapon = registry.try_get<WeaponComponent>(weaponEntity))
                    sum += weapon->level;
            }
            return sum;
        }

        //-------------------------------------------------------------
        //! @brief  拾える武器が地面にあるか（落下中の武器はまだPickupComponentを持たない）
        //-------------------------------------------------------------
        [[nodiscard]]
        bool HasPickupOnGround(Tsukino::ECS::Registry& registry) {
            auto view = registry.View<PickupComponent>();
            return view.begin() != view.end();
        }

        //-------------------------------------------------------------
        //! @brief  場面の案内の場面が来たか
        //-------------------------------------------------------------
        [[nodiscard]]
        bool IsHintTriggered(Tsukino::ECS::Registry& registry, TutorialHint hint, const PlayerComponent& player) {
            switch(hint) {
            case TutorialHint::Pickup:
                return HasPickupOnGround(registry);
            case TutorialHint::Switch:
                return player.weaponInventory.size() >= 2;
            case TutorialHint::Charge:
                return HasChargeWeapon(registry, player);
            default:
                return false;
            }
        }

        //-------------------------------------------------------------
        //! @brief  場面の案内をこなしたか（出した時点からの変化で見る）
        //-------------------------------------------------------------
        [[nodiscard]]
        bool IsHintDone(Tsukino::ECS::Registry& registry, const TutorialComponent& tutorial, TutorialHint hint, const PlayerComponent& player,
                        const PlayerAnimationSetComponent& anim) {
            switch(hint) {
            case TutorialHint::Pickup:
                return static_cast<int>(player.weaponInventory.size()) > tutorial.hintInventorySize
                       || SumWeaponLevels(registry, player) > tutorial.hintWeaponLevelSum;
            case TutorialHint::Switch:
                return player.selectedWeaponIndex != tutorial.hintSelectedWeapon;
            case TutorialHint::Charge:
                return anim.currentState == PlayerAnimState::Charge && ResolveChargeStage(anim.chargeTimer, player) >= kChargeHintClearStage;
            default:
                return false;
            }
        }

        //-------------------------------------------------------------
        //! @brief  次の手順へ進める
        //-------------------------------------------------------------
        void AdvanceStep(TutorialComponent& tutorial) {
            tutorial.step      = static_cast<TutorialStep>(static_cast<int>(tutorial.step) + 1);
            tutorial.stepTimer = 0.0f;
            tutorial.doneTimer = -1.0f;
            tutorial.moveTime  = 0.0f;
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
                    AdvanceStep(tutorial);
            } else if(IsStepDone(tutorial, player, anim, deltaTime)) {
                tutorial.doneTimer = 0.0f;
                UiPunch(registry, tutorial.panelEntity, 0.6f, true);    // できた手ごたえ
            } else if(tutorial.stepTimer >= params.stepTimeout) {
                AdvanceStep(tutorial);
            }

            //-------------------------------------------------------------
            // 表示
            //-------------------------------------------------------------
            const int              stepIndex = static_cast<int>(tutorial.step);
            const TutorialStepDef& def       = kStepDefs[stepIndex];
            const bool             isGoal    = tutorial.step == TutorialStep::Goal;

            // 最後の目標は操作の案内と違って読み終えるまで待たないので、板ごと控えめにゆらゆらさせて目を引く
            if(isGoal)
                UiIdle(registry, tutorial.panelEntity, 0.5f);
            else
                UiStopIdle(registry, tutorial.panelEntity);
            const bool             isDone    = tutorial.doneTimer >= 0.0f;

            const std::wstring stepText   = ctx.inputSystem ? FormatInputText(*ctx.inputSystem, def.text) : std::wstring(def.text);
            const float        panelWidth = FitPanelWidth(stepText);
            const float panelLeft  = panelCenterX - panelWidth * 0.5f;
            StretchSprite(registry, ctx, tutorial.panelEntity, panelCenterX, panelCenterY, panelWidth, params.panelHeight, params.panelColor);

            // 手順が変わった（または隠れていた状態から戻った）ら、板を下から滑り込ませる。文字とキー表示は板に追従する
            if(tutorial.shownStep != stepIndex) {
                tutorial.shownStep = stepIndex;
                UiAppearFromBelow(registry, tutorial.panelEntity, 0.0f, true);
            }

            std::wstring text = stepText;
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
            if(!def.HasPrompt())
                continue;    // キー表示の無い手順（目標）は作らない

            InputPromptDesc desc;
            desc.action        = def.action;
            desc.keyLabel      = def.keyLabel ? def.keyLabel : L"";
            desc.padKeyLabel   = def.padKeyLabel ? def.padKeyLabel : L"";
            desc.useMouse      = def.useMouse;
            desc.useHoldRing   = def.useHold;
            desc.sortOrderBase = UI::kTutorialPromptBase;
            tutorial.prompts[static_cast<size_t>(i)] = CreateInputPromptWidget(registry, context, desc);
        }

        // 場面の案内（拾う・切り替える・溜める）。板と文は共用、キー表示は案内ごと
        tutorial.hintPanel = CreateUiRectEntity(registry, context, UI::kTutorialPanel);
        tutorial.hintText  = CreateUiTextEntity(registry, UI::kTutorialText, UiTextAlign::Left);
        for(int i = 0; i < static_cast<int>(TutorialHint::Count); ++i) {
            const TutorialStepDef& def = kHintDefs[i];
            if(!def.HasPrompt())
                continue;    // キー表示の無い案内（拾う）は作らない

            InputPromptDesc desc;
            desc.action        = def.action;
            desc.keyLabel      = def.keyLabel ? def.keyLabel : L"";
            desc.padKeyLabel   = def.padKeyLabel ? def.padKeyLabel : L"";
            desc.useMouse      = def.useMouse;
            desc.useHoldRing   = def.useHold;
            desc.sortOrderBase = UI::kTutorialPromptBase;
            tutorial.hintPrompts[static_cast<size_t>(i)] = CreateInputPromptWidget(registry, context, desc);
        }

        // 文字とキー表示は板の動き（滑り込み・弾み・フェード）に追従させる
        UiFollow(registry, tutorial.textEntity, tutorial.panelEntity);
        UiFollow(registry, tutorial.counterEntity, tutorial.panelEntity);
        for(const InputPromptWidget& prompt : tutorial.prompts)
            UiFollowPrompt(registry, prompt, tutorial.panelEntity);
        UiFollow(registry, tutorial.hintText, tutorial.hintPanel);
        for(const InputPromptWidget& prompt : tutorial.hintPrompts)
            UiFollowPrompt(registry, prompt, tutorial.hintPanel);

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
            if(tutorial.finished
               && std::all_of(tutorial.hintStates.begin(), tutorial.hintStates.end(), [](TutorialHintState state) { return state == TutorialHintState::Done; }))
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
            // 場面の案内。手順の流れとは別に、その場面が来た時点で出し（拾える武器が現れた・2本目を持った・
            // バトルアックスを持った）、できたら「OK」を少し見せて消す。時間切れは無い。
            // 一度に出すのは1つだけで、同時に場面が来たらTutorialHintの並びで順に出す。
            // 手順の案内が出ている間は、その上に積む
            //-------------------------------------------------------------
            for(int i = 0; i < static_cast<int>(TutorialHint::Count); ++i) {
                TutorialHintState& state = tutorial.hintStates[static_cast<size_t>(i)];
                if(state == TutorialHintState::Waiting && IsHintTriggered(registry, static_cast<TutorialHint>(i), *player))
                    state = TutorialHintState::Pending;
            }

            // 拾える武器が消えたら、拾う案内はいったん下げて、次に現れたらまた出す
            if(tutorial.activeHint == static_cast<int>(TutorialHint::Pickup) && tutorial.hintDoneTimer < 0.0f && !HasPickupOnGround(registry)) {
                tutorial.hintStates[static_cast<size_t>(TutorialHint::Pickup)] = TutorialHintState::Waiting;
                tutorial.activeHint                                            = -1;
                HideHint(registry, tutorial);
            }

            if(tutorial.activeHint < 0) {
                for(int i = 0; i < static_cast<int>(TutorialHint::Count); ++i) {
                    if(tutorial.hintStates[static_cast<size_t>(i)] != TutorialHintState::Pending)
                        continue;
                    tutorial.activeHint                         = i;
                    tutorial.hintStates[static_cast<size_t>(i)] = TutorialHintState::Showing;
                    tutorial.hintDoneTimer                      = -1.0f;
                    tutorial.hintInventorySize                  = static_cast<int>(player->weaponInventory.size());
                    tutorial.hintWeaponLevelSum                 = SumWeaponLevels(registry, *player);
                    tutorial.hintSelectedWeapon                 = player->selectedWeaponIndex;
                    break;
                }
            }

            if(tutorial.activeHint < 0) {
                HideHint(registry, tutorial);
                continue;
            }

            const TutorialHint     hint = static_cast<TutorialHint>(tutorial.activeHint);
            const TutorialStepDef& def  = kHintDefs[tutorial.activeHint];
            if(tutorial.hintDoneTimer >= 0.0f) {
                tutorial.hintDoneTimer += deltaTime;
                if(tutorial.hintDoneTimer >= params.doneHold) {
                    // 「OK!」を見せ終えたら、フェードで消してから隠す。次の案内は板が消えきってから出す
                    if(!tutorial.hintFading) {
                        tutorial.hintFading = true;
                        UiDisappear(registry, tutorial.hintPanel, true);
                    } else if(!UiIsVisibilityAnimating(registry, tutorial.hintPanel)) {
                        tutorial.hintStates[static_cast<size_t>(tutorial.activeHint)] = TutorialHintState::Done;
                        tutorial.activeHint                                           = -1;
                        HideHint(registry, tutorial);
                        UiResetVisibility(registry, tutorial.hintPanel);
                        continue;
                    }
                }
            } else if(IsHintDone(registry, tutorial, hint, *player, *anim)) {
                tutorial.hintDoneTimer = 0.0f;
                UiPunch(registry, tutorial.hintPanel, 0.8f, true);    // できた手ごたえ
            }

            const bool  hintDone = tutorial.hintDoneTimer >= 0.0f;
            const float hintY    = tutorial.finished ? panelCenterY : panelCenterY + params.stackOffsetY;

            const std::wstring hintBaseText = ctx->inputSystem ? FormatInputText(*ctx->inputSystem, def.text) : std::wstring(def.text);
            const float        hintWidth    = FitPanelWidth(hintBaseText);
            const float panelLeft = panelCenterX - hintWidth * 0.5f;
            StretchSprite(registry, *ctx, tutorial.hintPanel, panelCenterX, hintY, hintWidth, params.panelHeight, params.panelColor);

            //-------------------------------------------------------------
            // 出した瞬間は下から滑り込ませる。手順の案内が終わって積む位置が下がったときは、
            // 前の位置から滑らせて移す（いきなり飛ばない）
            //-------------------------------------------------------------
            if(!tutorial.hintShown) {
                tutorial.hintShown = true;
                UiAppearFromBelow(registry, tutorial.hintPanel, 0.0f, true);
            } else if(hintY != tutorial.hintLastY) {
                UiSlideFrom(registry, tutorial.hintPanel, 0.0f, tutorial.hintLastY - hintY, true);
            }
            tutorial.hintLastY = hintY;

            std::wstring hintText = hintBaseText;
            if(hintDone)
                hintText += kDoneSuffix;
            PlaceUiText(registry, tutorial.hintText, panelLeft + params.textInsetX, hintY, GetUiTextScale(UiTextSize::Body), hintText,
                        hintDone ? params.doneColor : params.textColor);

            for(int i = 0; i < static_cast<int>(TutorialHint::Count); ++i) {
                const InputPromptWidget& prompt = tutorial.hintPrompts[static_cast<size_t>(i)];
                if(i != tutorial.activeHint) {
                    HideInputPrompt(registry, prompt);
                    continue;
                }
                InputPromptStyle style;
                if(hint == TutorialHint::Charge && anim->currentState == PlayerAnimState::Charge)
                    style.holdProgress = 1.0f;    // 溜めている間はゲージを満たして見せる
                ShowInputPromptAtScreen(registry, *ctx, prompt, panelLeft + params.promptInsetX, hintY, style);
            }
        }
    }
}    // namespace CombatAndroid::ECS
