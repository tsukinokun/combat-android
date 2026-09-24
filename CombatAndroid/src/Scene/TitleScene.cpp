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
#include <CombatAndroid/ECS/System/TitleStageSystem.hpp>
#include <CombatAndroid/ECS/System/GameSoundSystem.hpp>
#include <CombatAndroid/ECS/SystemPriority.hpp>
#include <CombatAndroid/ECS/Utility/Bgm.hpp>
#include <CombatAndroid/ECS/Utility/UiSprite.hpp>
#include <CombatAndroid/ECS/Utility/WeaponSpawner.hpp>
#include <CombatAndroid/UI/UiSortOrder.hpp>

#include <Tsukino/BuiltIn/ECS/Component/AmbientParticleComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/CameraComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/DirectionalLightComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/FogComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/ModelComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/RimGlowComponent.hpp>
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

// 名前空間 : CombatAndroid
namespace CombatAndroid {
    namespace {
        //-------------------------------------------------------------
        // 舞台の寸法（1ユニット≒1cm。地面の上面がy=0）。
        // カメラは左を向いた位置に構え、武器を画面の右半分へ寄せる
        // （左半分はタイトルとメニューの場所）
        //-------------------------------------------------------------
        const hlslpp::float3 kCameraPosition = hlslpp::float3(-250.0f, 205.0f, -330.0f);    //!< カメラの基準位置
        const hlslpp::float3 kCameraLookAt   = hlslpp::float3(-30.0f, 120.0f, 40.0f);       //!< 注視点（武器の左側）

        //! 武器を刺しておく場所（Yは沈める深さ）と、抜けきったあとの高さ
        struct TitleWeaponPlacement {
            hlslpp::float3 groundPosition;
            float          hoverHeight;
            float          burstTime;
        };

        const TitleWeaponPlacement kWeaponPlacements[CombatAndroid::ECS::kTitleStageWeaponCount] = {
            {hlslpp::float3(10.0f, -220.0f, 30.0f), 120.0f, 0.9f},      // ウォーハンマー（中央）
            {hlslpp::float3(155.0f, -220.0f, 70.0f), 150.0f, 1.5f},     // グレートソード（右奥）
            {hlslpp::float3(80.0f, -220.0f, -70.0f), 95.0f, 2.1f},     // バトルアックス（右手前）
        };
    }    // namespace

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

        registry.AddComponent<CombatAndroid::ECS::TitleMenuComponent>(titleEntity, title);
    }

    //-------------------------------------------------------------
    //! @brief  3Dの舞台（カメラ・夕日・地面・草・武器）を組む
    //-------------------------------------------------------------
    void TitleScene::BuildStage(Tsukino::ECS::Registry& registry, Tsukino::EngineIntegration::EngineContext& context) {
        CombatAndroid::ECS::TitleStageComponent stage;
        stage.cameraBasePosition = kCameraPosition;
        stage.cameraLookAt       = kCameraLookAt;

        //--------------------------------------------------------------
        // 3Dのカメラ。TitleStageSystemが位置を揺らすので、ここでは基準位置だけ入れる
        //--------------------------------------------------------------
        {
            Tsukino::ECS::Entity cameraEntity = m_scene.CreateEntity();

            Tsukino::BuiltIn::ECS::TransformComponent& cameraTransform =
                registry.AddComponent<Tsukino::BuiltIn::ECS::TransformComponent>(cameraEntity);
            cameraTransform.position = kCameraPosition;
            cameraTransform.dirty    = true;

            Tsukino::BuiltIn::ECS::CameraComponent& camera = registry.AddComponent<Tsukino::BuiltIn::ECS::CameraComponent>(cameraEntity);
            camera.projectionType                          = Tsukino::BuiltIn::ECS::CameraComponent::ProjectionType::Perspective;
            camera.fov                                     = 45.0f;
            camera.nearZ                                   = 1.0f;
            camera.farZ                                    = 3000.0f;    // 戦闘のTPSカメラ（2000）より少し遠くまで見せる
            camera.useLookAt                               = true;
            camera.lookAtTarget                            = kCameraLookAt;
            camera.isPrimary                               = true;

            stage.cameraEntity = cameraEntity;
        }

        //--------------------------------------------------------------
        // 夕日。低い角度から差す暖色の1灯で、武器と草に長い影を落とさせる
        //--------------------------------------------------------------
        {
            Tsukino::ECS::Entity lightEntity = m_scene.CreateEntity();

            Tsukino::BuiltIn::ECS::DirectionalLightComponent& light =
                registry.AddComponent<Tsukino::BuiltIn::ECS::DirectionalLightComponent>(lightEntity);
            // 太陽はカメラの側に置く（光の進む向きが奥＝+Z）。戦闘シーンと同じ向きにすると
            // 武器が逆光になり、金具の彫りが黒く潰れて見えるため、ここだけ正面から当てる
            light.direction  = hlslpp::float3(0.35f, -0.45f, 0.82f);
            light.color      = hlslpp::float3(1.0f, 1.0f, 1.0f);
            light.intensity  = 5.0f;
            light.castShadow = true;
        }

        //--------------------------------------------------------------
        // 空・霧・漂う塵。値は戦闘シーン（CombatAndroidScene.cpp）と揃える
        //--------------------------------------------------------------
        {
            Tsukino::ECS::Entity skyEntity = m_scene.CreateEntity();
            registry.AddComponent<Tsukino::BuiltIn::ECS::SkyAtmosphereComponent>(skyEntity);
        }

        {
            Tsukino::ECS::Entity fogEntity = m_scene.CreateEntity();
            auto&                fog       = registry.AddComponent<Tsukino::BuiltIn::ECS::FogComponent>(fogEntity);

            // 色は戦闘シーンと同じ。濃さだけ薄く・遠くから掛ける
            // （タイトルは遠くを隠す必要が無く、戦闘と同じ濃さだと武器まで白く霞む）
            fog.color         = hlslpp::float3(0.42f, 0.46f, 0.52f);
            fog.density       = 0.00050f;
            fog.startDistance = 900.0f;
            fog.maxOpacity    = 1.0f;

            fog.heightFogEnabled = true;
            fog.height           = 0.0f;
            fog.heightFalloff    = 0.004f;
            fog.heightDensity    = 0.00018f;

            fog.sunColor        = hlslpp::float3(1.0f, 0.85f, 0.65f);
            fog.sunScatterPower = 8.0f;

            fog.noiseEnabled   = true;
            fog.noiseScale     = 0.0015f;
            fog.noiseIntensity = 0.50f;
            fog.windDirection  = hlslpp::float3(1.0f, 0.0f, 0.3f);
            fog.windSpeed      = 60.0f;
        }

        {
            Tsukino::ECS::Entity particleEntity = m_scene.CreateEntity();
            auto&                particles      = registry.AddComponent<Tsukino::BuiltIn::ECS::AmbientParticleComponent>(particleEntity);

            particles.count      = 2000;    // カメラが動かないぶん、戦闘（3000）より減らしても密度は足りる
            particles.volumeSize = hlslpp::float3(2400.0f, 900.0f, 2400.0f);

            particles.color         = hlslpp::float3(1.0f, 0.32f, 0.06f);
            particles.intensity     = 1.0f;
            particles.minSize       = 0.8f;
            particles.maxSize       = 3.5f;
            particles.minBrightness = 0.15f;
            particles.maxBrightness = 1.6f;
            particles.twinkle       = 0.6f;

            particles.driftVelocity = hlslpp::float3(10.0f, 20.0f, 3.0f);
            particles.swayAmplitude = 14.0f;
            particles.swayFrequency = 0.8f;
            particles.minSpeedScale = 0.35f;
            particles.maxSpeedScale = 1.7f;

            particles.edgeFadeStart    = 0.65f;
            particles.nearFadeDistance = 60.0f;
        }

        //--------------------------------------------------------------
        // 地面。GroundVisualSystemはGroundFollowComponentを持つエンティティを探して
        // 土の板を描くので、当たり判定も剛体も要らない（誰も歩かないため）
        //--------------------------------------------------------------
        {
            Tsukino::ECS::Entity groundEntity = m_scene.CreateEntity();

            Tsukino::BuiltIn::ECS::TransformComponent& groundTransform =
                registry.AddComponent<Tsukino::BuiltIn::ECS::TransformComponent>(groundEntity);
            groundTransform.position = hlslpp::float3(0.0f, -5.0f, 0.0f);
            groundTransform.dirty    = true;

            CombatAndroid::ECS::GroundFollowComponent& groundFollow =
                registry.AddComponent<CombatAndroid::ECS::GroundFollowComponent>(groundEntity);
            groundFollow.groundHeight = groundTransform.position.y;
        }

        //--------------------------------------------------------------
        // 草。戦闘シーンと同じ見た目にしつつ、カメラが動かないぶん本数を半分にする
        //--------------------------------------------------------------
        {
            Tsukino::ECS::Entity grassEntity = m_scene.CreateEntity();
            auto&                grass       = registry.AddComponent<CombatAndroid::ECS::GrassFieldComponent>(grassEntity);

            grass.bladeCount    = 32000;
            grass.fieldSize     = 6000.0f;
            grass.farFieldSize  = 9000.0f;
            grass.farBladeCount = 32000;

            grass.lodBlendStart    = 2200.0f;
            grass.lodBlendEnd      = 2800.0f;
            grass.horizonFillStart = 1500.0f;
            grass.horizonFillEnd   = 2800.0f;

            grass.bladeWidth        = 5.5f;
            grass.heightVariance    = 0.35f;
            grass.groundHeight      = 0.0f;
            grass.distantWidthBoost = 2.5f;

            grass.species[0] = {34.0f, 1.00f, hlslpp::float3(0.07f, 0.30f, 0.05f), hlslpp::float3(0.30f, 0.66f, 0.13f)};
            grass.species[1] = {22.0f, 0.85f, hlslpp::float3(0.14f, 0.22f, 0.04f), hlslpp::float3(0.52f, 0.56f, 0.10f)};
            grass.species[2] = {46.0f, 1.15f, hlslpp::float3(0.03f, 0.17f, 0.04f), hlslpp::float3(0.12f, 0.46f, 0.14f)};

            grass.clumpRadiusMin    = 60.0f;
            grass.clumpRadiusMax    = 250.0f;
            grass.clumpSpawnChance  = 0.8f;
            grass.clumpShapeNoise   = 0.25f;
            grass.clumpEdgeSoftness = 0.35f;
            grass.fillerDensity     = 0.12f;
            grass.fillerHeightScale = 0.45f;

            grass.windDirection = hlslpp::float3(1.0f, 0.0f, 0.3f);
            grass.windStrength  = 0.4f;

            grass.gustWavelength = 650.0f;
            grass.gustSpeed      = 320.0f;
            grass.gustStrength   = 0.55f;

            grass.swaySpeed    = 3.0f;
            grass.swayStrength = 0.15f;

            grass.fadeStartRatio = 0.93f;
        }

        //--------------------------------------------------------------
        // 見せる武器。戦闘用のSpawnWeaponは当たり判定や拾得の部品まで付けるので使わず、
        // モデルのパスだけ武器の表（WeaponSpawner.cpp）から借りて素のエンティティを作る
        //--------------------------------------------------------------
        for(int i = 0; i < CombatAndroid::ECS::kTitleStageWeaponCount; ++i) {
            const CombatAndroid::ECS::WeaponSpawnDefinition& definition =
                CombatAndroid::ECS::GetWeaponSpawnDefinition(static_cast<CombatAndroid::ECS::WeaponId>(i));
            const TitleWeaponPlacement& placement = kWeaponPlacements[i];

            Tsukino::ECS::Entity weaponEntity = m_scene.CreateEntity();

            Tsukino::BuiltIn::ECS::TransformComponent& transform =
                registry.AddComponent<Tsukino::BuiltIn::ECS::TransformComponent>(weaponEntity);
            transform.position = placement.groundPosition;
            transform.dirty    = true;

            Tsukino::BuiltIn::ECS::ModelComponent& model = registry.AddComponent<Tsukino::BuiltIn::ECS::ModelComponent>(weaponEntity);
            model.modelHandle                            = context.assetManager->Load(Tsukino::Core::Path(definition.modelPath));
            model.visible                                = true;

            // 抜けた瞬間から輪郭を光らせる（強さはTitleStageSystemが毎フレーム書く）
            registry.AddComponent<Tsukino::BuiltIn::ECS::RimGlowComponent>(weaponEntity);

            CombatAndroid::ECS::TitleStageWeapon& weapon = stage.weapons[i];
            weapon.entity                                 = weaponEntity;
            weapon.groundPosition                         = placement.groundPosition;
            weapon.hoverHeight                            = placement.hoverHeight;
            weapon.burstTime                              = placement.burstTime;
            weapon.spinPhase                              = static_cast<float>(i) * 1.7f;
            weapon.bobPhase                               = static_cast<float>(i) * 0.9f;
        }

        registry.AddComponent<CombatAndroid::ECS::TitleStageComponent>(m_scene.CreateEntity(), stage);
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
