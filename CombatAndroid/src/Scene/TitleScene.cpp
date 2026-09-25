//-------------------------------------------------------------
//! @file    TitleScene.cpp
//! @brief   タイトル画面のシーンの実装
//-------------------------------------------------------------
#include <CombatAndroid/Scene/TitleScene.hpp>

#include <CombatAndroid/ECS/Component/GrassFieldComponent.hpp>
#include <CombatAndroid/ECS/Component/GroundFollowComponent.hpp>
#include <CombatAndroid/ECS/Component/TitleMenuComponent.hpp>
#include <CombatAndroid/ECS/Component/TitleStageComponent.hpp>
#include <CombatAndroid/ECS/System/GrassFieldSystem.hpp>
#include <CombatAndroid/ECS/System/GroundVisualSystem.hpp>
#include <CombatAndroid/ECS/System/TitleMenuSystem.hpp>
#include <CombatAndroid/ECS/System/ScreenFadeSystem.hpp>
#include <CombatAndroid/ECS/System/TitleStageSystem.hpp>
#include <CombatAndroid/ECS/System/GameSoundSystem.hpp>
#include <CombatAndroid/ECS/SystemPriority.hpp>
#include <CombatAndroid/ECS/Utility/Bgm.hpp>
#include <CombatAndroid/ECS/Utility/GamePrefab.hpp>
#include <CombatAndroid/ECS/Utility/ScreenFade.hpp>
#include <CombatAndroid/ECS/Utility/UiSprite.hpp>
#include <CombatAndroid/UI/UiSortOrder.hpp>

#include <Tsukino/BuiltIn/ECS/Component/AmbientParticleComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/DirectionalLightComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/FogComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/SkyAtmosphereComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/TransformComponent.hpp>

#include <Tsukino/EngineIntegration/ECS/System/AmbientParticleSystem.hpp>
#include <Tsukino/EngineIntegration/ECS/System/AudioSystem.hpp>
#include <Tsukino/EngineIntegration/ECS/System/CameraSystem.hpp>
#include <Tsukino/EngineIntegration/ECS/System/EffectSystem.hpp>
#include <Tsukino/EngineIntegration/ECS/System/FogSystem.hpp>
#include <Tsukino/EngineIntegration/ECS/System/FontRendererSystem.hpp>
#include <Tsukino/EngineIntegration/ECS/System/LightSystem.hpp>
#include <Tsukino/EngineIntegration/ECS/System/ModelSystem.hpp>
#include <Tsukino/EngineIntegration/ECS/System/SkyAtmosphereSystem.hpp>
#include <Tsukino/EngineIntegration/ECS/System/SpriteRendererSystem.hpp>
#include <Tsukino/EngineIntegration/ECS/System/TransformSystem.hpp>
#include <Tsukino/EngineIntegration/EngineContext.hpp>

#include <Tsukino/Engine/Asset/AssetManager.hpp>

#include <Tsukino/Core/Path.hpp>
#include <Tsukino/Core/Window.hpp>
#include <Tsukino/Renderer/Renderer.hpp>

#include <memory>
#include <string>

// 名前空間 : CombatAndroid
namespace CombatAndroid {
    //-------------------------------------------------------------
    //! @brief  シーン固有の初期化処理
    //-------------------------------------------------------------
    void TitleScene::OnInitialize(Tsukino::EngineIntegration::EngineAPI& /*api*/) {
        Tsukino::EngineIntegration::EngineContext* context = m_scene.GetRegistry().GetContext<Tsukino::EngineIntegration::EngineContext*>();
        Tsukino::ECS::Registry&                    registry = m_scene.GetRegistry();

        // 空（SkyAtmosphere）が全面を覆うので、塗り潰しの色が見えるのは描画が間に合わない最初の1枚だけ
        context->renderer->SetClearColor(0.03f, 0.04f, 0.07f, 1.0f);

        // 戦闘シーンではTpsCameraSystemがカーソルを隠しているので、戻ってきたときのために出しておく
        if(context->window)
            context->window->SetCursorVisible(true);

        // タイトルのBGM。素材が置かれていなければ無音のまま進む（Assets/Audio/README.md）
        CombatAndroid::ECS::PlayBgm(*context, CombatAndroid::ECS::kTitleBgmPath);

        //--------------------------------------------------------------
        // システム。戦闘シーン（CombatAndroidSceneSystems.cpp）の並びに合わせる。
        // 物理・敵・プレイヤーは出さないので、その分だけ登録しない
        //--------------------------------------------------------------
        using CombatAndroid::ECS::SystemPriority;

        m_scene.AddSystem(std::make_shared<Tsukino::BuiltIn::ECS::TransformSystem>(), (int)SystemPriority::Transform);
        m_scene.AddSystem(std::make_shared<CombatAndroid::ECS::TitleStageSystem>(), (int)SystemPriority::Gameplay);
        m_scene.AddSystem(std::make_shared<CombatAndroid::ECS::TitleMenuSystem>(), (int)SystemPriority::PlayerHud);
        m_scene.AddSystem(std::make_shared<CombatAndroid::ECS::ScreenFadeSystem>(), (int)SystemPriority::PlayerHud);

        // 武器とカメラが書いた位置をworldMatrixへ反映してから、カメラ行列を作る
        m_scene.AddSystem(std::make_shared<Tsukino::BuiltIn::ECS::TransformSystem>(), (int)SystemPriority::TransformLate);
        m_scene.AddSystem(std::make_shared<Tsukino::BuiltIn::ECS::CameraSystem>(), (int)SystemPriority::Camera);

        // メニューが書いたUIの位置をworldMatrixへ反映する（FontRendererSystemがそこから読む）
        m_scene.AddSystem(std::make_shared<Tsukino::BuiltIn::ECS::TransformSystem>(), (int)SystemPriority::TransformUI);

        m_scene.AddSystem(std::make_shared<Tsukino::BuiltIn::ECS::SpriteRenderSystem>(), (int)SystemPriority::Render);
        m_scene.AddSystem(std::make_shared<Tsukino::BuiltIn::ECS::ModelSystem>(), (int)SystemPriority::Render);
        m_scene.AddSystem(std::make_shared<CombatAndroid::ECS::GroundVisualSystem>(), (int)SystemPriority::Render);
        m_scene.AddSystem(std::make_shared<Tsukino::BuiltIn::ECS::FontRendererSystem>(), (int)SystemPriority::Font);

        {
            // 土煙。TitleStageSystemがEngineContext::effectSystem経由で再生を頼む
            auto effectSystem = std::make_shared<Tsukino::BuiltIn::ECS::EffectSystem>();
            m_scene.AddSystem(effectSystem, (int)SystemPriority::Render);
            effectSystem->Initialize(m_scene.GetRegistry(), m_scene.GetEventBus());
            context->effectSystem = effectSystem.get();
        }

        m_scene.AddSystem(std::make_shared<Tsukino::BuiltIn::ECS::AudioSystem>(), (int)SystemPriority::Audio);
        {
            // メニューの操作音と、武器が抜ける音。これが無いとタイトルではPlaySoundが誰にも拾われず無音になり、
            // オプションで変えた効果音の音量もその場で確かめられない
            auto gameSoundSystem = std::make_shared<CombatAndroid::ECS::GameSoundSystem>();
            m_scene.AddSystem(gameSoundSystem, (int)SystemPriority::Audio);
            gameSoundSystem->Initialize(m_scene.GetEventBus());
        }

        // 夕日・空・霧・漂う塵・草。戦闘シーンと同じ並び
        m_scene.AddSystem(std::make_shared<Tsukino::BuiltIn::ECS::LightSystem>(), (int)SystemPriority::Light);
        m_scene.AddSystem(std::make_shared<Tsukino::BuiltIn::ECS::SkyAtmosphereSystem>(), (int)SystemPriority::SkyAtmosphere);
        m_scene.AddSystem(std::make_shared<Tsukino::BuiltIn::ECS::FogSystem>(), (int)SystemPriority::Fog);
        m_scene.AddSystem(std::make_shared<Tsukino::BuiltIn::ECS::AmbientParticleSystem>(), (int)SystemPriority::AmbientParticle);
        m_scene.AddSystem(std::make_shared<CombatAndroid::ECS::GrassFieldSystem>(), (int)SystemPriority::GrassField);

        BuildStage(registry, *context);

        // 場面の切り替わりを繋ぐ黒。起動直後もここから明ける
        CombatAndroid::ECS::CreateScreenFade(registry, *context);

        //--------------------------------------------------------------
        // 画面固定UI用の2Dカメラ（Prefab: UiCamera2D）
        //--------------------------------------------------------------
        CombatAndroid::ECS::InstantiateUiCamera2D(registry, *context);

        //--------------------------------------------------------------
        // メニューのUI一式。全て非表示で作り、TitleMenuSystemが最初のフレームで組む
        //--------------------------------------------------------------
        CombatAndroid::ECS::TitleMenuComponent title;

        title.backdropEntity = CombatAndroid::ECS::CreateUiRectEntity(registry, *context, CombatAndroid::UI::kTitleBackdrop);
        for(Tsukino::ECS::Entity& fadeEntity : title.backdropFadeEntities)
            fadeEntity = CombatAndroid::ECS::CreateUiRectEntity(registry, *context, CombatAndroid::UI::kTitleBackdrop);
        title.titleEntity    = CombatAndroid::ECS::CreateUiTextEntity(registry, CombatAndroid::UI::kTitleText, CombatAndroid::ECS::UiTextAlign::Center);
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

        // Prefab: Title/Menu（アタッチだけ）。中身は上で作ったエンティティの束
        registry.GetComponent<CombatAndroid::ECS::TitleMenuComponent>(CombatAndroid::ECS::InstantiatePrefab(registry, *context, "Title/Menu")) = title;
    }

    //-------------------------------------------------------------
    //! @brief  3Dの舞台（カメラ・夕日・地面・草・武器）を組む
    //-------------------------------------------------------------
    void TitleScene::BuildStage(Tsukino::ECS::Registry& registry, Tsukino::EngineIntegration::EngineContext& context) {
        //--------------------------------------------------------------
        // 舞台の演出パラメータ（Prefab: Title/Stage）。1ユニット≒1cmで、地面の上面がy=0。
        // カメラは左を向いた位置に構え、武器を画面の右半分へ寄せる（左半分はタイトルとメニューの場所）。
        // 武器を刺しておく場所（Yは沈める深さ）・抜けきったあとの高さ・抜けるタイミングもここが持つ
        //--------------------------------------------------------------
        Tsukino::ECS::Entity                    stageEntity = CombatAndroid::ECS::InstantiatePrefab(registry, context, "Title/Stage");
        CombatAndroid::ECS::TitleStageComponent stage       = registry.GetComponent<CombatAndroid::ECS::TitleStageComponent>(stageEntity);

        //--------------------------------------------------------------
        // 3Dのカメラ（Prefab: Title/Camera）。TitleStageSystemが基準位置から揺らす
        //--------------------------------------------------------------
        stage.cameraEntity = CombatAndroid::ECS::InstantiatePrefab(registry, context, "Title/Camera");

        //--------------------------------------------------------------
        // 夕日・空・霧・漂う塵・地面・草。値は Assets/Prefabs/Environment/Title/ 以下のPrefab JSON（README参照）
        //--------------------------------------------------------------
        CombatAndroid::ECS::InstantiateEnvironment(registry, context, "Title");

        //--------------------------------------------------------------
        // 見せる武器（Prefab: Title/Weapon0〜2 ＝ウォーハンマー・グレートソード・バトルアックス）。
        // 戦闘用の武器Prefabは当たり判定や拾得の部品まで持つので使わず、見た目とリムグローだけの素のPrefabを使う。
        // 抜けた瞬間から輪郭を光らせる（強さはTitleStageSystemが毎フレーム書く）
        //--------------------------------------------------------------
        for(int i = 0; i < CombatAndroid::ECS::kTitleStageWeaponCount; ++i)
            stage.weapons[i].entity = CombatAndroid::ECS::InstantiatePrefab(registry, context, "Title/Weapon" + std::to_string(i));

        // 生成でComponentの格納先が動き得るので、結び終えた値をまとめて書き戻す
        registry.GetComponent<CombatAndroid::ECS::TitleStageComponent>(stageEntity) = stage;
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
        if(auto* context = m_scene.GetRegistry().GetContext<Tsukino::EngineIntegration::EngineContext*>()) {
            CombatAndroid::ECS::StopBgm(*context, CombatAndroid::ECS::kTitleBgmPath);

            // このシーンと一緒に壊れるEffectSystemを指したままにしない
            // （次のシーンが自分のものを入れるまでの間、誰かが触ると落ちる）
            context->effectSystem = nullptr;
        }
    }
}    // namespace CombatAndroid
