//-------------------------------------------------------------
//! @file    TitleScene.cpp
//! @brief   タイトル画面のシーンの実装
//-------------------------------------------------------------
#include <CombatAndroid/Scene/TitleScene.hpp>

#include <CombatAndroid/ECS/Component/TitleMenuComponent.hpp>
#include <CombatAndroid/ECS/System/TitleMenuSystem.hpp>
#include <CombatAndroid/ECS/System/GameSoundSystem.hpp>
#include <CombatAndroid/ECS/Utility/Bgm.hpp>
#include <CombatAndroid/ECS/Utility/UiSprite.hpp>
#include <CombatAndroid/UI/UiSortOrder.hpp>

#include <Tsukino/BuiltIn/ECS/Component/CameraComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/TransformComponent.hpp>

#include <Tsukino/EngineIntegration/ECS/System/CameraSystem.hpp>
#include <Tsukino/EngineIntegration/ECS/System/FontRendererSystem.hpp>
#include <Tsukino/EngineIntegration/ECS/System/SpriteRendererSystem.hpp>
#include <Tsukino/EngineIntegration/ECS/System/TransformSystem.hpp>
#include <Tsukino/EngineIntegration/EngineContext.hpp>

#include <Tsukino/Core/Window.hpp>
#include <Tsukino/Renderer/Renderer.hpp>

#include <memory>

// 名前空間 : CombatAndroid
namespace CombatAndroid {
    //-------------------------------------------------------------
    //! @brief  シーン固有の初期化処理
    //-------------------------------------------------------------
    void TitleScene::OnInitialize(Tsukino::EngineIntegration::EngineAPI& /*api*/) {
        Tsukino::EngineIntegration::EngineContext* context = m_scene.GetRegistry().GetContext<Tsukino::EngineIntegration::EngineContext*>();
        Tsukino::ECS::Registry&                    registry = m_scene.GetRegistry();

        // 3Dの世界は描かないので、背景は暗い紺で塗る（実際に見えるのはTitleMenuSystemが敷く背景の板）
        context->renderer->SetClearColor(0.03f, 0.04f, 0.07f, 1.0f);

        // 戦闘シーンではTpsCameraSystemがカーソルを隠しているので、戻ってきたときのために出しておく
        if(context->window)
            context->window->SetCursorVisible(true);

        // タイトルのBGM。素材が置かれていなければ無音のまま進む（Assets/Audio/README.md）
        CombatAndroid::ECS::PlayBgm(*context, CombatAndroid::ECS::kTitleBgmPath);

        //--------------------------------------------------------------
        // システム。画面固定のスプライトと文字を描くのに必要な分だけ
        //--------------------------------------------------------------
        m_scene.AddSystem(std::make_shared<Tsukino::BuiltIn::ECS::TransformSystem>(), 0);
        m_scene.AddSystem(std::make_shared<CombatAndroid::ECS::TitleMenuSystem>(), 1);
        m_scene.AddSystem(std::make_shared<Tsukino::BuiltIn::ECS::TransformSystem>(), 2);    // メニューが書いた位置をworldMatrixへ反映する
        m_scene.AddSystem(std::make_shared<Tsukino::BuiltIn::ECS::CameraSystem>(), 3);
        m_scene.AddSystem(std::make_shared<Tsukino::BuiltIn::ECS::SpriteRenderSystem>(), 4);
        m_scene.AddSystem(std::make_shared<Tsukino::BuiltIn::ECS::FontRendererSystem>(), 5);
        {
            // メニューの操作音。これが無いとタイトルではPlaySoundが誰にも拾われず無音になり、
            // オプションで変えた効果音の音量もその場で確かめられない
            auto gameSoundSystem = std::make_shared<CombatAndroid::ECS::GameSoundSystem>();
            m_scene.AddSystem(gameSoundSystem, 6);
            gameSoundSystem->Initialize(m_scene.GetEventBus());
        }

        //--------------------------------------------------------------
        // 画面固定UI用の2Dカメラ（戦闘シーンと同じ設定）
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
        // メニューのUI一式。全て非表示で作り、TitleMenuSystemが最初のフレームで組む
        //--------------------------------------------------------------
        Tsukino::ECS::Entity                  titleEntity = m_scene.CreateEntity();
        CombatAndroid::ECS::TitleMenuComponent title;

        title.backdropEntity = CombatAndroid::ECS::CreateUiRectEntity(registry, *context, CombatAndroid::UI::kTitleBackdrop);
        title.titleEntity    = CombatAndroid::ECS::CreateUiTextEntity(registry, CombatAndroid::UI::kTitleText, CombatAndroid::ECS::UiTextAlign::Center);
        title.subtitleEntity = CombatAndroid::ECS::CreateUiTextEntity(registry, CombatAndroid::UI::kTitleText, CombatAndroid::ECS::UiTextAlign::Center);
        title.bestEntity     = CombatAndroid::ECS::CreateUiTextEntity(registry, CombatAndroid::UI::kTitleText, CombatAndroid::ECS::UiTextAlign::Center);
        title.menu           = CombatAndroid::ECS::CreateGameMenuWidget(registry, *context, CombatAndroid::UI::kTitleMenuBase);

        title.controlsPanelEntity  = CombatAndroid::ECS::CreateUiRectEntity(registry, *context, CombatAndroid::UI::kTitleControlsPanel);
        title.controlsHeaderEntity =
            CombatAndroid::ECS::CreateUiTextEntity(registry, CombatAndroid::UI::kTitleControlsText, CombatAndroid::ECS::UiTextAlign::Center);
        for(int i = 0; i < CombatAndroid::ECS::kTitleControlsLineCount; ++i) {
            title.controlsActionEntities[i] =
                CombatAndroid::ECS::CreateUiTextEntity(registry, CombatAndroid::UI::kTitleControlsText, CombatAndroid::ECS::UiTextAlign::Left);
            title.controlsKeyEntities[i] =
                CombatAndroid::ECS::CreateUiTextEntity(registry, CombatAndroid::UI::kTitleControlsText, CombatAndroid::ECS::UiTextAlign::Left);
        }
        title.controlsMenu = CombatAndroid::ECS::CreateGameMenuWidget(registry, *context, CombatAndroid::UI::kTitleControlsMenuBase);
        title.options      = CombatAndroid::ECS::CreateOptionsMenu(registry, *context, CombatAndroid::UI::kTitleOptionsBase);

        registry.AddComponent<CombatAndroid::ECS::TitleMenuComponent>(titleEntity, title);
    }

    //-------------------------------------------------------------
    //! @brief  シーンの更新
    //-------------------------------------------------------------
    void TitleScene::OnUpdate(Tsukino::EngineIntegration::EngineAPI& /*api*/, float deltaTime) {
        m_scene.Update(deltaTime);
    }

    //-------------------------------------------------------------
    //! @brief  シーンの終了処理
    //-------------------------------------------------------------
    void TitleScene::OnExit() {
        if(auto* context = m_scene.GetRegistry().GetContext<Tsukino::EngineIntegration::EngineContext*>())
            CombatAndroid::ECS::StopBgm(*context, CombatAndroid::ECS::kTitleBgmPath);
    }
}    // namespace CombatAndroid
