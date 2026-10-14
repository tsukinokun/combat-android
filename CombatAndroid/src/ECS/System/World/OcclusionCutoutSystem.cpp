//-------------------------------------------------------------
//! @file   OcclusionCutoutSystem.cpp
//! @brief  OcclusionCutoutSystemクラスの実装
//-------------------------------------------------------------
#include <CombatAndroid/ECS/System/World/OcclusionCutoutSystem.hpp>
#include <CombatAndroid/ECS/Component/Combat/HealthComponent.hpp>
#include <CombatAndroid/ECS/Component/Enemy/EnemyComponent.hpp>
#include <CombatAndroid/ECS/Component/Player/PlayerComponent.hpp>
#include <CombatAndroid/ECS/Component/Weapon/WeaponComponent.hpp>
#include <CombatAndroid/ECS/Serialization/Common/SerializationHelper.hpp>
#include <CombatAndroid/ECS/Utility/Table/TableJson.hpp>
#include <CombatAndroid/ECS/Utility/Time/WorldTimeContext.hpp>

#include <Tsukino/EngineIntegration/EngineContext.hpp>

#include <Tsukino/BuiltIn/ECS/Component/CameraComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/OcclusionCutoutComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/TransformComponent.hpp>

#include <Tsukino/Renderer/Renderer.hpp>

#include <Tsukino/Core/Window.hpp>
#include <Tsukino/Core/Math/Matrix.hpp>

#include <entt/entt.hpp>

#include <algorithm>
#include <cmath>
#include <optional>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    namespace {
        //-------------------------------------------------------------
        //! @struct OcclusionCutoutParams
        //! @brief  調整値（Assets/Tables/Systems/OcclusionCutout.json。ここの初期値はJSONにキーが無いときの既定値）
        //-------------------------------------------------------------
        struct OcclusionCutoutParams {
            float radiusWorld      = 110.0f;    //!< 抜く円の大きさ（プレイヤーの位置でのワールドの半径。cm）。画面のピクセルへは毎フレーム換算する
            float centerHeight     = 105.0f;    //!< 円の中心にする高さ（足元から。胸のあたり）
            float deadCenterHeight = 35.0f;     //!< 倒れた後の円の中心の高さ（地面に横たわった体のあたり）

            //! 倒れた後は、画面に映る敵を（円に関係なく）丸ごと網点で透かす。その見えている割合（1で透かさない）
            float deadEnemyVisibility = 0.35f;
            float deadEnemyFadeSpeed  = 4.0f;    //!< 透かし始め・戻りの速さ（1秒あたりの寄せの強さ）
            float depthMargin      = 60.0f;     //!< プレイヤーよりこれだけ手前の物から抜く（プレイヤーのすぐ脇にいる敵まで抜かないため）
            float centerVisibility = 0.1f;      //!< 円の中心で見えている割合（0で全部抜く）
            float edgeSoftStart    = 0.35f;     //!< 縁へ向かって元へ戻り始める位置（半径に対する割合）
        };

        template <class Archive>
        void load(Archive& archive, OcclusionCutoutParams& params) {
            LoadField(archive, "radiusWorld", params.radiusWorld);
            LoadField(archive, "centerHeight", params.centerHeight);
            LoadField(archive, "deadCenterHeight", params.deadCenterHeight);
            LoadField(archive, "deadEnemyVisibility", params.deadEnemyVisibility);
            LoadField(archive, "deadEnemyFadeSpeed", params.deadEnemyFadeSpeed);
            LoadField(archive, "depthMargin", params.depthMargin);
            LoadField(archive, "centerVisibility", params.centerVisibility);
            LoadField(archive, "edgeSoftStart", params.edgeSoftStart);
        }

        //-------------------------------------------------------------
        //! @brief  チューニング値を得る関数（初回の呼び出しで1度だけ読む）
        //-------------------------------------------------------------
        const OcclusionCutoutParams& GetParams() {
            static const OcclusionCutoutParams s_params = LoadSystemParams<OcclusionCutoutParams>("OcclusionCutout");
            return s_params;
        }

        //-------------------------------------------------------------
        //! @brief  ワールド座標を画面ピクセルへ写す
        //! @return 写せたか（カメラの後ろならfalse）
        //-------------------------------------------------------------
        bool ProjectToScreen(const hlslpp::float3& world, const Tsukino::Core::Math::matrix& viewProj, float screenW, float screenH, float& outX,
                             float& outY) {
            const hlslpp::float4 clip = hlslpp::mul(hlslpp::float4(world, 1.0f), viewProj);
            if(clip.w <= 0.0001f)
                return false;
            outX = (static_cast<float>(clip.x / clip.w) * 0.5f + 0.5f) * screenW;
            outY = (1.0f - (static_cast<float>(clip.y / clip.w) * 0.5f + 0.5f)) * screenH;    // NDCのYは上向き、画面のピクセルは下向き
            return true;
        }
    }    // namespace

    //-------------------------------------------------------------
    //! @brief システムの更新
    //-------------------------------------------------------------
    void OcclusionCutoutSystem::Update(Tsukino::ECS::Registry& registry, float deltaTime) {

        Tsukino::EngineIntegration::EngineContext* ctx = registry.GetContext<Tsukino::EngineIntegration::EngineContext*>();
        if(!ctx || !ctx->renderer || !ctx->window)
            return;

        //-------------------------------------------------------------
        // プレイヤー
        //-------------------------------------------------------------
        Tsukino::ECS::Entity          playerEntity = entt::null;
        std::optional<hlslpp::float3> playerFeet;
        bool                          playerDead = false;
        {
            auto view = registry.View<PlayerComponent, Tsukino::BuiltIn::ECS::TransformComponent>();
            for(entt::entity entity : view) {
                playerEntity = entity;
                playerFeet   = view.get<Tsukino::BuiltIn::ECS::TransformComponent>(entity).position;
                if(const auto* health = registry.try_get<HealthComponent>(entity))
                    playerDead = health->isDead;
                break;
            }
        }

        //-------------------------------------------------------------
        // プレイヤーが持っている武器は抜かない（体の脇で浮いているので、抜くとプレイヤーの一部が欠けて見える）。
        // 地面に落ちている武器・敵が持っている武器は抜く
        //-------------------------------------------------------------
        {
            auto view = registry.View<WeaponComponent, Tsukino::BuiltIn::ECS::OcclusionCutoutComponent>();
            for(entt::entity entity : view) {
                const auto& weapon                                            = view.get<WeaponComponent>(entity);
                view.get<Tsukino::BuiltIn::ECS::OcclusionCutoutComponent>(entity).enabled = (playerEntity == entt::null || weapon.owner != playerEntity);
            }
        }

        //-------------------------------------------------------------
        // 倒れた後は、敵（と敵が持っている武器）を丸ごと網点で透かす。倒れたプレイヤーの周りを回るカメラから、
        // 群がった敵の陰に隠れずにプレイヤーが見えるようにする。リザルト中はゲームが止まって
        // deltaTimeが0になるので、スロー・停止を掛ける前の実時間で寄せる
        //-------------------------------------------------------------
        {
            const OcclusionCutoutParams& params        = GetParams();
            const float                  realDeltaTime = registry.HasContext<WorldTimeContext>() ? registry.GetContext<WorldTimeContext>().realDeltaTime : deltaTime;
            const float                  target        = playerDead ? std::clamp(params.deadEnemyVisibility, 0.0f, 1.0f) : 1.0f;
            const float                  blend         = 1.0f - std::exp(-params.deadEnemyFadeSpeed * realDeltaTime);

            auto view = registry.View<Tsukino::BuiltIn::ECS::OcclusionCutoutComponent>();
            for(entt::entity entity : view) {
                // 敵本体と、敵が持っている武器だけ。木・地面の武器・プレイヤーの武器は触らない
                bool isEnemyPart = registry.HasComponent<EnemyComponent>(entity);
                if(const auto* weapon = registry.try_get<WeaponComponent>(entity); weapon && weapon->owner != entt::null && registry.IsValid(weapon->owner))
                    isEnemyPart = isEnemyPart || registry.HasComponent<EnemyComponent>(weapon->owner);
                auto& cutout = view.get<Tsukino::BuiltIn::ECS::OcclusionCutoutComponent>(entity);
                if(!isEnemyPart) {
                    cutout.ditherFade = 1.0f;
                    continue;
                }
                cutout.ditherFade += (target - cutout.ditherFade) * blend;
            }
        }

        //-------------------------------------------------------------
        // プレイヤーがいなければ設定しない（FrameConstantsは毎フレーム無効へ戻るので、設定しなければ抜かれない）。
        // ゲームが止まっている間（死亡後のリザルト・メニュー・カットシーン）も抜き続ける。
        // 特に死亡後はカメラが倒れたプレイヤーの周りを回るので、手前の敵や木で隠れないようにする
        //-------------------------------------------------------------
        if(!playerFeet)
            return;

        //-------------------------------------------------------------
        // 主カメラ（行列と位置）
        //-------------------------------------------------------------
        Tsukino::Core::Math::matrix   viewProj;
        std::optional<hlslpp::float3> cameraPosition;
        {
            auto view = registry.View<Tsukino::BuiltIn::ECS::CameraComponent, Tsukino::BuiltIn::ECS::TransformComponent>();
            for(entt::entity entity : view) {
                const auto& camera = view.get<Tsukino::BuiltIn::ECS::CameraComponent>(entity);
                if(!camera.isPrimary)
                    continue;
                viewProj       = camera.viewProjMatrix;
                cameraPosition = view.get<Tsukino::BuiltIn::ECS::TransformComponent>(entity).position;
                break;
            }
        }
        if(!cameraPosition)
            return;

        //-------------------------------------------------------------
        // プレイヤーの胸の高さを画面へ写し、そこでのradiusWorldが画面で何ピクセルかを、
        // 同じ点を真上へずらして写した差で求める（カメラが寄るほど円も大きくなる）
        //-------------------------------------------------------------
        const OcclusionCutoutParams& params  = GetParams();
        const float                  screenW = static_cast<float>(ctx->window->GetWidth());
        const float                  screenH = static_cast<float>(ctx->window->GetHeight());

        // 倒れた後は体が地面に横たわるので、円の中心を下げる
        const float          height = playerDead ? params.deadCenterHeight : params.centerHeight;
        const hlslpp::float3 center = *playerFeet + hlslpp::float3(0.0f, height, 0.0f);
        const hlslpp::float3 above  = center + hlslpp::float3(0.0f, params.radiusWorld, 0.0f);

        float cx, cy, ax, ay;
        if(!ProjectToScreen(center, viewProj, screenW, screenH, cx, cy) || !ProjectToScreen(above, viewProj, screenW, screenH, ax, ay))
            return;    // プレイヤーがカメラの後ろ（通常は起きない）

        const float radiusPixels = std::sqrt((ax - cx) * (ax - cx) + (ay - cy) * (ay - cy));
        const float distance     = static_cast<float>(hlslpp::length(center - *cameraPosition));

        ctx->renderer->GetFrameConstants().SetOcclusionCutout(cx, cy, radiusPixels, distance, params.depthMargin,
                                                              std::clamp(params.centerVisibility, 0.0f, 1.0f),
                                                              std::clamp(params.edgeSoftStart, 0.0f, 0.99f));
    }
}    // namespace CombatAndroid::ECS
