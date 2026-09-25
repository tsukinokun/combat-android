//-------------------------------------------------------------
//! @file    LoadingScene.cpp
//! @brief   起動直後、タイトルの前に挟むロード画面のシーンの実装
//-------------------------------------------------------------
#include <CombatAndroid/Scene/LoadingScene.hpp>
#include <CombatAndroid/Scene/TitleScene.hpp>

#include <CombatAndroid/ECS/Utility/AssetPreloader.hpp>
#include <CombatAndroid/ECS/System/ScreenFadeSystem.hpp>
#include <CombatAndroid/ECS/Utility/ScreenFade.hpp>
#include <CombatAndroid/ECS/Utility/UiSprite.hpp>
#include <CombatAndroid/UI/UiSortOrder.hpp>

#include <Tsukino/BuiltIn/ECS/Component/CameraComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/TransformComponent.hpp>

#include <Tsukino/EngineIntegration/ECS/System/CameraSystem.hpp>
#include <Tsukino/EngineIntegration/ECS/System/FontRendererSystem.hpp>
#include <Tsukino/EngineIntegration/ECS/System/SpriteRendererSystem.hpp>
#include <Tsukino/EngineIntegration/ECS/System/TransformSystem.hpp>
#include <Tsukino/EngineIntegration/EngineContext.hpp>
#include <Tsukino/EngineIntegration/Scene/GameSceneManager.hpp>

#include <Tsukino/Core/Window.hpp>
#include <Tsukino/Renderer/Renderer.hpp>

#include <objbase.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>

// 名前空間 : CombatAndroid
namespace CombatAndroid {
    namespace {
        //-------------------------------------------------------------
        // 見た目（画面中心からのピクセル）
        //-------------------------------------------------------------
        constexpr float kLabelOffsetY    = -40.0f;     //!< 「NOW LOADING」のY
        constexpr float kBarOffsetY      = 30.0f;      //!< 進捗バーのY
        constexpr float kPercentOffsetY  = 70.0f;      //!< 「42%」のY
        constexpr float kBarWidth        = 520.0f;
        constexpr float kBarHeight       = 10.0f;
        constexpr float kLabelFontScale  = 1.4f;
        constexpr float kPercentFontScale = 0.8f;

        //! 回る印：画面右下に、3枚の細い板を60度ずつずらして重ねた「✳」を回す。
        //! 1件の重い読み込み（初回のFBX変換など）で進捗バーがしばらく止まっても、
        //! これが回っていれば固まっていないと分かる
        constexpr float kSpinnerMarginX   = 110.0f;    //!< 画面右端からの距離
        constexpr float kSpinnerMarginY   = 90.0f;     //!< 画面下端からの距離
        constexpr float kSpinnerLength    = 40.0f;
        constexpr float kSpinnerThickness = 6.0f;
        constexpr float kSpinnerSpeed     = 3.0f;      //!< 回る速さ（rad/秒）
        constexpr float kPi               = 3.14159265f;

        //! 「NOW LOADING」の後ろの点が1つ増えるまでの秒数
        constexpr float kDotInterval = 0.35f;

        //! 進捗バーが実際の進捗へ追いつく速さ（大きいほど素早い）。1件ずつ飛ぶのを均して見せる
        constexpr float kBarFollowSpeed = 10.0f;

        //! 最低表示時間（秒）。キャッシュ済みで一瞬で読み終えたとき、画面が1フレームだけ
        //! 映ってちらつくのを避ける
        constexpr float kMinDisplaySeconds = 0.3f;

        const hlslpp::float4 kBackdropColor = hlslpp::float4(0.03f, 0.04f, 0.07f, 1.0f);    //!< タイトルと同じ紺
        const hlslpp::float4 kTextColor     = hlslpp::float4(1.0f, 1.0f, 1.0f, 1.0f);
        const hlslpp::float4 kSubTextColor  = hlslpp::float4(0.75f, 0.75f, 0.80f, 1.0f);
        const hlslpp::float4 kTrackColor    = hlslpp::float4(1.0f, 1.0f, 1.0f, 0.15f);
        const hlslpp::float4 kFillColor     = hlslpp::float4(1.0f, 0.85f, 0.45f, 1.0f);     //!< タイトルの副題と同じ金
        const hlslpp::float4 kSpinnerColor  = hlslpp::float4(1.0f, 0.85f, 0.45f, 0.9f);
    }    // namespace

    //-------------------------------------------------------------
    //! @brief  デストラクタ
    //-------------------------------------------------------------
    LoadingScene::~LoadingScene() {
        StopWorker();
    }

    //-------------------------------------------------------------
    //! @brief  シーン固有の初期化処理
    //-------------------------------------------------------------
    void LoadingScene::OnInitialize(Tsukino::EngineIntegration::EngineAPI& /*api*/) {
        Tsukino::EngineIntegration::EngineContext* context = m_scene.GetRegistry().GetContext<Tsukino::EngineIntegration::EngineContext*>();
        Tsukino::ECS::Registry&                    registry = m_scene.GetRegistry();

        context->renderer->SetClearColor(0.03f, 0.04f, 0.07f, 1.0f);

        //--------------------------------------------------------------
        // システム。画面固定のスプライトと文字を描くのに必要な分だけ（タイトルと同じ構成）
        //--------------------------------------------------------------
        m_scene.AddSystem(std::make_shared<Tsukino::BuiltIn::ECS::TransformSystem>(), 0);
        m_scene.AddSystem(std::make_shared<Tsukino::BuiltIn::ECS::CameraSystem>(), 1);
        m_scene.AddSystem(std::make_shared<Tsukino::BuiltIn::ECS::SpriteRenderSystem>(), 2);
        m_scene.AddSystem(std::make_shared<Tsukino::BuiltIn::ECS::FontRendererSystem>(), 3);
        m_scene.AddSystem(std::make_shared<CombatAndroid::ECS::ScreenFadeSystem>(), 4);

        //--------------------------------------------------------------
        // 画面固定UI用の2Dカメラ（タイトル・戦闘シーンと同じ設定）
        //--------------------------------------------------------------
        {
            Tsukino::ECS::Entity cameraEntity = m_scene.CreateEntity();

            Tsukino::BuiltIn::ECS::TransformComponent& cameraTransform =
                registry.AddComponent<Tsukino::BuiltIn::ECS::TransformComponent>(cameraEntity);
            cameraTransform.position = hlslpp::float3(0.0f, 0.0f, -1.0f);

            Tsukino::BuiltIn::ECS::CameraComponent& camera = registry.AddComponent<Tsukino::BuiltIn::ECS::CameraComponent>(cameraEntity);
            camera.projectionType                          = Tsukino::BuiltIn::ECS::CameraComponent::ProjectionType::Orthographic;
            camera.orthoSize                               = 1000.0f;
            camera.isPrimary                               = false;
        }

        //--------------------------------------------------------------
        // 画面の部品。位置と大きさは毎フレームRefreshUiが書く
        //--------------------------------------------------------------
        m_backdropEntity = CombatAndroid::ECS::CreateUiRectEntity(registry, *context, CombatAndroid::UI::kLoadingBackdrop);
        m_barTrackEntity = CombatAndroid::ECS::CreateUiRectEntity(registry, *context, CombatAndroid::UI::kLoadingParts);
        m_barFillEntity  = CombatAndroid::ECS::CreateUiRectEntity(registry, *context, CombatAndroid::UI::kLoadingParts + 1);
        for(Tsukino::ECS::Entity& spinner : m_spinnerEntities)
            spinner = CombatAndroid::ECS::CreateUiRectEntity(registry, *context, CombatAndroid::UI::kLoadingParts);
        m_labelEntity   = CombatAndroid::ECS::CreateUiTextEntity(registry, CombatAndroid::UI::kLoadingText, CombatAndroid::ECS::UiTextAlign::Center);
        m_percentEntity = CombatAndroid::ECS::CreateUiTextEntity(registry, CombatAndroid::UI::kLoadingText, CombatAndroid::ECS::UiTextAlign::Center);

        // 場面の切り替わりを繋ぐ黒。起動直後もここから明ける
        CombatAndroid::ECS::CreateScreenFade(registry, *context);

        RefreshUi();

        //--------------------------------------------------------------
        // 読み込みの裏スレッド。AssetManagerはスレッド安全なので、本体スレッドが
        // この画面を描きながら（Getしながら）裏でLoadしてよい。
        // テクスチャの変換はWIC（COM）を使うため、このスレッドでもCOMを初期化する
        //--------------------------------------------------------------
        m_steps = CombatAndroid::ECS::BuildPreloadSteps(*context);
        m_worker = std::thread([this] {
            const HRESULT comResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);

            for(const std::function<void()>& step : m_steps) {
                if(m_cancel.load())
                    break;
                step();
                m_completedSteps.fetch_add(1);
            }

            if(SUCCEEDED(comResult))
                CoUninitialize();
            m_finished.store(true);
        });
    }

    //-------------------------------------------------------------
    //! @brief  シーンの更新
    //-------------------------------------------------------------
    void LoadingScene::OnUpdate(Tsukino::EngineIntegration::EngineAPI& /*api*/, float deltaTime) {
        m_elapsed += deltaTime;

        // 進捗バーは実際の進捗へ滑らかに追いつかせる（読み終えたら満タンにしてから切り替える）
        const float target = m_steps.empty() ? 1.0f : static_cast<float>(m_completedSteps.load()) / static_cast<float>(m_steps.size());
        m_displayedProgress += (target - m_displayedProgress) * std::min(1.0f, kBarFollowSpeed * deltaTime);

        RefreshUi();
        m_scene.Update(deltaTime);

        //--------------------------------------------------------------
        // 読み終えたらタイトルシーンへ。ChangeSceneは予約だけで、実際の切り替えは次フレーム頭
        //--------------------------------------------------------------
        if(m_changeRequested || !m_finished.load() || m_elapsed < kMinDisplaySeconds)
            return;

        StopWorker();    // 終わっているので待たずに合流できる

        // 黒く覆ってから切り替える（ScreenFadeSystemが暗転しきった時点でChangeSceneを呼ぶ）
        CombatAndroid::ECS::RequestSceneChangeWithFade(m_scene.GetRegistry(), []() { return std::make_unique<CombatAndroid::TitleScene>(); });

        m_changeRequested = true;
    }

    //-------------------------------------------------------------
    //! @brief  シーンの終了処理
    //-------------------------------------------------------------
    void LoadingScene::OnExit() {
        StopWorker();
    }

    //-------------------------------------------------------------
    //! @brief  読み込みの裏スレッドを止めて合流する
    //-------------------------------------------------------------
    void LoadingScene::StopWorker() {
        m_cancel.store(true);
        if(m_worker.joinable())
            m_worker.join();
    }

    //-------------------------------------------------------------
    //! @brief  画面を今の状態で組み直す
    //-------------------------------------------------------------
    void LoadingScene::RefreshUi() {
        Tsukino::ECS::Registry& registry = m_scene.GetRegistry();
        auto*                   context  = registry.GetContext<Tsukino::EngineIntegration::EngineContext*>();
        if(!context)
            return;

        const float screenWidth  = context->window ? static_cast<float>(context->window->GetWidth()) : 1700.0f;
        const float screenHeight = context->window ? static_cast<float>(context->window->GetHeight()) : 1000.0f;
        const float centerX      = screenWidth * 0.5f;
        const float centerY      = screenHeight * 0.5f;

        CombatAndroid::ECS::StretchSprite(registry, *context, m_backdropEntity, centerX, centerY, screenWidth, screenHeight, kBackdropColor);

        // 「NOW LOADING」の後ろの点を 0〜3 個で繰り返す
        const int    dotCount = static_cast<int>(m_elapsed / kDotInterval) % 4;
        std::wstring label    = L"NOW LOADING";
        label.append(static_cast<size_t>(dotCount), L'.');
        label.append(static_cast<size_t>(3 - dotCount), L' ');    // 点の数で文字列の幅が変わって中央がぶれないよう空白で埋める
        CombatAndroid::ECS::PlaceUiText(registry, m_labelEntity, centerX, centerY + kLabelOffsetY, kLabelFontScale, label, kTextColor);

        // 進捗バー（溝の左端から伸ばす）
        const float progress  = std::clamp(m_displayedProgress, 0.0f, 1.0f);
        const float barLeft   = centerX - kBarWidth * 0.5f;
        const float fillWidth = kBarWidth * progress;
        CombatAndroid::ECS::StretchSprite(registry, *context, m_barTrackEntity, centerX, centerY + kBarOffsetY, kBarWidth, kBarHeight, kTrackColor);
        CombatAndroid::ECS::StretchSprite(registry, *context, m_barFillEntity, barLeft + fillWidth * 0.5f, centerY + kBarOffsetY, fillWidth,
                                          kBarHeight, kFillColor);

        CombatAndroid::ECS::PlaceUiText(registry, m_percentEntity, centerX, centerY + kPercentOffsetY, kPercentFontScale,
                                        std::to_wstring(static_cast<int>(progress * 100.0f + 0.5f)) + L"%", kSubTextColor);

        // 回る印
        const float spinnerX = screenWidth - kSpinnerMarginX;
        const float spinnerY = screenHeight - kSpinnerMarginY;
        for(int i = 0; i < 3; ++i) {
            const float angle = m_elapsed * kSpinnerSpeed + static_cast<float>(i) * kPi / 3.0f;
            CombatAndroid::ECS::StretchSpriteRotated(registry, *context, m_spinnerEntities[i], spinnerX, spinnerY, kSpinnerThickness,
                                                     kSpinnerLength, angle, kSpinnerColor);
        }
    }
}    // namespace CombatAndroid
