// 一時ファイル：現行コードが作るエンティティをPrefab JSONへ書き出すための使い捨て。書き出したら削除する
#include <CombatAndroid/ECS/Component/DamageNumberComponent.hpp>
#include <CombatAndroid/ECS/Component/EnemyAnimationSetComponent.hpp>
#include <CombatAndroid/ECS/Component/ExpOrbComponent.hpp>
#include <CombatAndroid/ECS/Component/HealthComponent.hpp>
#include <CombatAndroid/ECS/Component/TitleStageComponent.hpp>
#include <CombatAndroid/ECS/Component/WeaponComponent.hpp>
#include <CombatAndroid/ECS/Serialization/EnemyDefinitionSerialization.hpp>
#include <CombatAndroid/ECS/Utility/EnemySpawner.hpp>
#include <CombatAndroid/ECS/Utility/UiSprite.hpp>
#include <CombatAndroid/ECS/Utility/WeaponSpawner.hpp>
#include <CombatAndroid/UI/UiSortOrder.hpp>

#include <Tsukino/BuiltIn/ECS/Component/CameraComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/DebugCameraComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/EffectComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/FontComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/ModelComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/RimGlowComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/SpriteComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/TransformComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/WorldAnchorComponent.hpp>
#include <Tsukino/EngineIntegration/EngineContext.hpp>
#include <Tsukino/Engine/ECS/Prefab/PrefabFactory.hpp>

#include <cereal/archives/json.hpp>
#include <cereal/types/map.hpp>
#include <cereal/types/string.hpp>

#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <map>
#include <string>

namespace CombatAndroid::ECS {
    namespace {
        namespace BI = Tsukino::BuiltIn::ECS;

        const std::string kRoot       = "CombatAndroid/Assets/Prefabs/";
        const char*       kWhitePixel = "CombatAndroid/Assets/Textures/UI/WhitePixel.png";

        void AddNulls(const std::string& dir, std::initializer_list<const char*> names) {
            std::map<std::string, std::string> list;
            {
                std::ifstream is(dir + "/Prefab.json");
                if(is.is_open()) {
                    cereal::JSONInputArchive archive(is);
                    archive(cereal::make_nvp("Components", list));
                }
            }
            for(const char* name : names)
                list[name] = "null";

            std::filesystem::create_directories(dir);
            std::ofstream             os(dir + "/Prefab.json");
            cereal::JSONOutputArchive archive(os);
            archive(cereal::make_nvp("Components", list));
        }

        void Capture(Tsukino::ECS::Registry& registry, Tsukino::EngineIntegration::EngineContext& context, Tsukino::ECS::Entity entity,
                     const std::string& name, std::initializer_list<const char*> extras = {}) {
            const std::string dir = kRoot + name;
            std::filesystem::remove(dir + "/Prefab.json");
            (void)context.prefabFactory->CaptureEntity(registry, entity, dir);
            if(extras.size() > 0)
                AddNulls(dir, extras);
        }

        void SetSpritePath(Tsukino::ECS::Registry& registry, Tsukino::ECS::Entity entity, const char* path) {
            registry.GetComponent<BI::SpriteComponent>(entity).textureHandle.path = path;
        }

        Tsukino::ECS::Entity MakeSprite(Tsukino::ECS::Registry& registry, const char* path, int sortOrder, bool scaleZero) {
            Tsukino::ECS::Entity entity    = registry.CreateEntity();
            auto&                transform = registry.AddComponent<BI::TransformComponent>(entity);
            if(scaleZero)
                transform.scale = hlslpp::float3(0.0f, 0.0f, 0.0f);
            auto& sprite                = registry.AddComponent<BI::SpriteComponent>(entity);
            sprite.textureHandle.path   = path;
            sprite.sortOrder            = sortOrder;
            return entity;
        }
    }    // namespace

    void TmpCaptureAll(Tsukino::EngineIntegration::EngineContext& context) {
        Tsukino::ECS::Registry registry;

        //------------------------------------------------------------- 敵とHPバー
        {
            struct EnemyCapture {
                const char* name;
                EnemySpawnConfig (*make)(Tsukino::EngineIntegration::EngineContext&, const hlslpp::float3&);
            };
            const EnemyCapture enemies[] = {{"SmallZombie", &MakeSmallZombieConfig}, {"BigZombie", &MakeBigZombieConfig}};

            auto captureEnemy = [&](const char* name, const EnemySpawnConfig& config, const std::string& attackClipOverride) {
                EnemyDefinition def;
                {
                    std::ifstream            is(kRoot + "Enemy/" + name + ".json");
                    cereal::JSONInputArchive archive(is);
                    archive(cereal::make_nvp("EnemyDefinition", def));
                }

                Tsukino::ECS::Entity e = SpawnBehaviorEnemy(registry, context, config);
                registry.GetComponent<BI::ModelComponent>(e).modelHandle.path = def.modelPath;
                auto& set              = registry.GetComponent<EnemyAnimationSetComponent>(e);
                set.walkClip.path      = def.walkClipPath;
                set.attackClip.path    = attackClipOverride.empty() ? def.attackClipPath : attackClipOverride;
                set.knockbackClip.path = def.knockbackClipPath;
                set.deathClip.path     = def.deathClipPath;
                Capture(registry, context, e, std::string("Enemy/") + name,
                        {"AnimationControllerComponent", "SkeletonOutputComponent", "BehaviorTreeComponent"});
                return e;
            };

            Tsukino::ECS::Entity first = entt::null;
            for(const EnemyCapture& enemy : enemies) {
                Tsukino::ECS::Entity e = captureEnemy(enemy.name, enemy.make(context, hlslpp::float3(0.0f, 0.0f, 0.0f)), "");
                if(first == entt::null)
                    first = e;
            }
            (void)captureEnemy("Paladin", MakePaladinConfig(context, hlslpp::float3(0.0f, 0.0f, 0.0f), WeaponId::Warhammer),
                               GetPaladinWeaponAttack(WeaponId::Warhammer).attackClipPath);

            const auto& health = registry.GetComponent<HealthComponent>(first);
            for(auto [bar, name] : {std::pair{health.hpBarBackgroundEntity, "Enemy/HpBarBackground"}, std::pair{health.hpBarFillEntity, "Enemy/HpBarFill"}}) {
                SetSpritePath(registry, bar, kWhitePixel);
                registry.GetComponent<BI::WorldAnchorComponent>(bar).worldOffset = hlslpp::float3(0.0f, 0.0f, 0.0f);
                Capture(registry, context, bar, name);
            }
        }

        //------------------------------------------------------------- 武器
        {
            const char* names[] = {"Warhammer", "Greatsword", "Battleaxe"};
            for(int i = 0; i < static_cast<int>(WeaponId::Count); ++i) {
                const WeaponSpawnDefinition& def = GetWeaponSpawnDefinition(static_cast<WeaponId>(i));
                Tsukino::ECS::Entity         e   = SpawnWeapon(registry, context, static_cast<WeaponId>(i), hlslpp::float3(0.0f, 0.0f, 0.0f));
                registry.GetComponent<BI::ModelComponent>(e).modelHandle.path = def.modelPath;
                auto& weapon                      = registry.GetComponent<WeaponComponent>(e);
                weapon.attackClip.path            = def.playerAttackClipPath;
                weapon.areaAttackEffectAsset.path = (def.areaAttackRadius > 0.0f) ? def.areaAttackEffectPath : std::string();
                weapon.projectileEffectAsset.path = def.projectileEffectPath;
                Capture(registry, context, e, std::string("Weapon/") + names[i]);
            }
        }

        //------------------------------------------------------------- 斬撃弾
        {
            Tsukino::ECS::Entity e = registry.CreateEntity();
            registry.AddComponent<BI::TransformComponent>(e);
            auto& effect          = registry.AddComponent<BI::EffectComponent>(e);
            effect.followRotation = true;
            Capture(registry, context, e, "Projectile", {"ProjectileComponent"});
        }

        //------------------------------------------------------------- 汎用UI
        {
            Tsukino::ECS::Entity rect = CreateUiRectEntity(registry, context, 0);
            SetSpritePath(registry, rect, kWhitePixel);
            Capture(registry, context, rect, "UI/Rect");

            Tsukino::ECS::Entity rectAnchored = CreateUiRectEntity(registry, context, 0);
            SetSpritePath(registry, rectAnchored, kWhitePixel);
            registry.AddComponent<BI::WorldAnchorComponent>(rectAnchored);
            Capture(registry, context, rectAnchored, "UI/RectAnchored");

            Capture(registry, context, CreateUiTextEntity(registry, 0, UiTextAlign::Center), "UI/Text");

            for(bool anchored : {false, true}) {
                Tsukino::ECS::Entity e = registry.CreateEntity();
                registry.AddComponent<BI::TransformComponent>(e).scale = hlslpp::float3(0.0f, 0.0f, 0.0f);
                auto& font            = registry.AddComponent<BI::FontComponent>(e);
                font.horizontalAlign  = BI::HorizontalAlign::Center;
                font.verticalAlign    = BI::VerticalAlign::Middle;
                font.outlineColor     = hlslpp::float4(1.0f, 1.0f, 1.0f, 0.85f);
                font.outlineWidth     = 1.5f;
                if(anchored)
                    registry.AddComponent<BI::WorldAnchorComponent>(e);
                Capture(registry, context, e, anchored ? "UI/PromptTextAnchored" : "UI/PromptText");
            }
        }

        //------------------------------------------------------------- 戦闘シーンのHUD・プール
        {
            {
                Tsukino::ECS::Entity e = registry.CreateEntity();
                registry.AddComponent<BI::TransformComponent>(e).scale = hlslpp::float3(0.0f, 0.0f, 0.0f);
                registry.AddComponent<BI::WorldAnchorComponent>(e);
                registry.AddComponent<BI::FontComponent>(e).sortOrder = UI::kDamageNumber;
                Capture(registry, context, e, "UI/DamageNumber", {"DamageNumberComponent"});
            }
            {
                Tsukino::ECS::Entity e = MakeSprite(registry, "CombatAndroid/Assets/Textures/UI/ExpOrb.png", UI::World::kExpOrb, true);
                auto& sprite           = registry.GetComponent<BI::SpriteComponent>(e);
                sprite.blendMode       = BI::SpriteBlendMode::Additive;
                sprite.space           = BI::SpriteSpace::World;
                Capture(registry, context, e, "UI/ExpOrb", {"ExpOrbComponent"});
            }
            Capture(registry, context, MakeSprite(registry, kWhitePixel, UI::kHudBarBackground, false), "UI/HudBar");
            {
                Tsukino::ECS::Entity e = registry.CreateEntity();
                registry.AddComponent<BI::TransformComponent>(e);
                auto& font         = registry.AddComponent<BI::FontComponent>(e);
                font.color         = hlslpp::float4(1.0f, 1.0f, 1.0f, 1.0f);
                font.outlineColor  = hlslpp::float4(0.0f, 0.0f, 0.0f, 1.0f);
                font.outlineWidth  = 2.0f;
                font.verticalAlign = BI::VerticalAlign::Middle;
                font.sortOrder     = UI::kHudText;
                Capture(registry, context, e, "UI/HudText");
            }
            {
                Tsukino::ECS::Entity e = registry.CreateEntity();
                registry.AddComponent<BI::TransformComponent>(e);
                auto& font           = registry.AddComponent<BI::FontComponent>(e);
                font.color           = hlslpp::float4(1.0f, 1.0f, 1.0f, 1.0f);
                font.outlineColor    = hlslpp::float4(0.0f, 0.0f, 0.0f, 1.0f);
                font.outlineWidth    = 2.0f;
                font.horizontalAlign = BI::HorizontalAlign::Center;
                font.verticalAlign   = BI::VerticalAlign::Top;
                font.sortOrder       = UI::kHudText;
                Capture(registry, context, e, "UI/HudTopText");
            }
            Capture(registry, context, MakeSprite(registry, kWhitePixel, UI::kGameLogPanel, true), "UI/GameLogPanel", {"GameLogComponent"});
            Capture(registry, context, MakeSprite(registry, kWhitePixel, UI::kGameLogAccent, true), "UI/GameLogAccent");
            {
                Tsukino::ECS::Entity e = registry.CreateEntity();
                registry.AddComponent<BI::TransformComponent>(e);
                auto& font           = registry.AddComponent<BI::FontComponent>(e);
                font.outlineColor    = hlslpp::float4(0.0f, 0.0f, 0.0f, 1.0f);
                font.outlineWidth    = 2.0f;
                font.horizontalAlign = BI::HorizontalAlign::Left;
                font.verticalAlign   = BI::VerticalAlign::Middle;
                font.sortOrder       = UI::kGameLogText;
                Capture(registry, context, e, "UI/GameLogText");
            }
            {
                Tsukino::ECS::Entity e = MakeSprite(registry, kWhitePixel, UI::kScreenDamageFlash, false);
                registry.GetComponent<BI::SpriteComponent>(e).tintColor = hlslpp::float4(0.9f, 0.05f, 0.05f, 0.0f);
                Capture(registry, context, e, "UI/ScreenFlash");
            }
            Capture(registry, context, MakeSprite(registry, kWhitePixel, UI::kSkillSelectBackdrop, true), "UI/SkillPanel");
            {
                Tsukino::ECS::Entity e = registry.CreateEntity();
                registry.AddComponent<BI::TransformComponent>(e);
                auto& font           = registry.AddComponent<BI::FontComponent>(e);
                font.outlineColor    = hlslpp::float4(0.0f, 0.0f, 0.0f, 1.0f);
                font.outlineWidth    = 3.0f;
                font.horizontalAlign = BI::HorizontalAlign::Center;
                font.verticalAlign   = BI::VerticalAlign::Middle;
                font.sortOrder       = UI::kSkillSelectText;
                Capture(registry, context, e, "UI/SkillText");
            }
        }

        //------------------------------------------------------------- デバッグ
        {
            struct DebugHud {
                const char*    name;
                float          y;
                hlslpp::float4 color;
                int            sortOrder;
                const char*    marker;
            };
            const DebugHud huds[] = {
                {"Debug/GripHud", 10.0f, hlslpp::float4(1.0f, 1.0f, 0.3f, 1.0f), UI::kDebugWeaponGripHud, "WeaponGripDebugComponent"},
                {"Debug/LevelHud", 230.0f, hlslpp::float4(1.0f, 0.8f, 0.3f, 1.0f), UI::kDebugWeaponLevelHud, "WeaponLevelDebugComponent"},
                {"Debug/StressHud", 120.0f, hlslpp::float4(0.4f, 1.0f, 0.6f, 1.0f), UI::kDebugStressTestHud, "EnemyStressTestComponent"},
            };
            for(const DebugHud& hud : huds) {
                Tsukino::ECS::Entity e         = registry.CreateEntity();
                auto&                transform = registry.AddComponent<BI::TransformComponent>(e);
                transform.position             = hlslpp::float3(10.0f, hud.y, 0.0f);
                auto& font                     = registry.AddComponent<BI::FontComponent>(e);
                font.color                     = hud.color;
                font.origin                    = hlslpp::float2(0.0f, 0.0f);
                font.sortOrder                 = hud.sortOrder;
                Capture(registry, context, e, hud.name, {hud.marker});
            }

            Tsukino::ECS::Entity e = registry.CreateEntity();
            registry.AddComponent<BI::TransformComponent>(e).position = hlslpp::float3(0.0f, 180.0f, 300.0f);
            auto& camera        = registry.AddComponent<BI::CameraComponent>(e);
            camera.lookAtTarget = hlslpp::float3(0.0f, 100.0f, 0.0f);
            camera.nearZ        = 1.0f;
            camera.farZ         = 10000.0f;
            camera.isPrimary    = false;
            registry.AddComponent<BI::DebugCameraComponent>(e).moveSpeed = 1.0f;
            Capture(registry, context, e, "Debug/Camera", {"DebugCameraTag"});
        }

        //------------------------------------------------------------- タイトル
        {
            const hlslpp::float3 cameraPosition(-250.0f, 205.0f, -330.0f);
            const hlslpp::float3 cameraLookAt(-30.0f, 120.0f, 40.0f);

            Tsukino::ECS::Entity camEntity = registry.CreateEntity();
            registry.AddComponent<BI::TransformComponent>(camEntity).position = cameraPosition;
            auto& camera          = registry.AddComponent<BI::CameraComponent>(camEntity);
            camera.projectionType = BI::CameraComponent::ProjectionType::Perspective;
            camera.fov            = 45.0f;
            camera.nearZ          = 1.0f;
            camera.farZ           = 3000.0f;
            camera.useLookAt      = true;
            camera.lookAtTarget   = cameraLookAt;
            camera.isPrimary      = true;
            Capture(registry, context, camEntity, "Title/Camera");

            struct Placement {
                hlslpp::float3 groundPosition;
                float          hoverHeight;
                float          burstTime;
            };
            const Placement placements[] = {
                {hlslpp::float3(10.0f, -220.0f, 30.0f), 120.0f, 0.9f},
                {hlslpp::float3(155.0f, -220.0f, 70.0f), 150.0f, 1.5f},
                {hlslpp::float3(80.0f, -220.0f, -70.0f), 95.0f, 2.1f},
            };

            TitleStageComponent stage;
            stage.cameraBasePosition = cameraPosition;
            stage.cameraLookAt       = cameraLookAt;
            for(int i = 0; i < kTitleStageWeaponCount; ++i) {
                Tsukino::ECS::Entity e = registry.CreateEntity();
                registry.AddComponent<BI::TransformComponent>(e).position = placements[i].groundPosition;
                auto& model              = registry.AddComponent<BI::ModelComponent>(e);
                model.modelHandle.path   = GetWeaponSpawnDefinition(static_cast<WeaponId>(i)).modelPath;
                model.visible            = true;
                registry.AddComponent<BI::RimGlowComponent>(e);
                Capture(registry, context, e, "Title/Weapon" + std::to_string(i));

                stage.weapons[i].groundPosition = placements[i].groundPosition;
                stage.weapons[i].hoverHeight    = placements[i].hoverHeight;
                stage.weapons[i].burstTime      = placements[i].burstTime;
                stage.weapons[i].spinPhase      = static_cast<float>(i) * 1.7f;
                stage.weapons[i].bobPhase       = static_cast<float>(i) * 0.9f;
            }

            Tsukino::ECS::Entity stageEntity = registry.CreateEntity();
            registry.AddComponent<TitleStageComponent>(stageEntity, stage);
            Capture(registry, context, stageEntity, "Title/Stage");

            AddNulls(kRoot + "Title/Menu", {"TitleMenuComponent"});
        }

        //------------------------------------------------------------- 実行時状態だけの束
        AddNulls(kRoot + "System/ScreenFade", {"ScreenFadeComponent"});
        AddNulls(kRoot + "System/Tutorial", {"TutorialComponent"});
    }
}    // namespace CombatAndroid::ECS
