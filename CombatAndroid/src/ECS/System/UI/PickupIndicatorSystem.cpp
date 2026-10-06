//-------------------------------------------------------------
//! @file   PickupIndicatorSystem.cpp
//! @brief  PickupIndicatorSystemクラスの実装
//-------------------------------------------------------------
#include <CombatAndroid/ECS/System/UI/PickupIndicatorSystem.hpp>
#include <CombatAndroid/ECS/Serialization/Common/SerializationHelper.hpp>
#include <CombatAndroid/ECS/Utility/Table/TableJson.hpp>
#include <CombatAndroid/ECS/Utility/UI/UiSprite.hpp>
#include <CombatAndroid/ECS/Component/Weapon/PickupComponent.hpp>
#include <CombatAndroid/UI/UiSortOrder.hpp>

#include <Tsukino/EngineIntegration/EngineContext.hpp>

#include <Tsukino/BuiltIn/ECS/Component/CameraComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/TransformComponent.hpp>

#include <Tsukino/Core/Window.hpp>
#include <Tsukino/Core/Math/Matrix.hpp>
#include <Tsukino/Core/Math/Serialization/HlslppSerialization.hpp>

#include <entt/entt.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    namespace {
        constexpr float kHalfPi = 1.5707963267948966f;

        //-------------------------------------------------------------
        //! @struct PickupIndicatorParams
        //! @brief  見た目の調整値（Assets/Tables/Systems/PickupIndicator.json。ここの初期値はJSONにキーが無いときの既定値）
        //-------------------------------------------------------------
        struct PickupIndicatorParams {
            float          edgeMargin      = 60.0f;                                //!< 画面端からの余白（ピクセル）。矢印がここより外へはみ出さない
            float          indicatorLength = 36.0f;                                //!< 矢印（棒）の長さ
            float          indicatorWidth  = 10.0f;                                //!< 矢印（棒）の太さ
            hlslpp::float3 indicatorColor  = hlslpp::float3(0.3f, 0.9f, 1.0f);    //!< 矢印の色。Pickup.rimColorと揃える（Assets/Tables/README.md参照）
        };

        template <class Archive>
        void load(Archive& archive, PickupIndicatorParams& params) {
            LoadField(archive, "edgeMargin", params.edgeMargin);
            LoadField(archive, "indicatorLength", params.indicatorLength);
            LoadField(archive, "indicatorWidth", params.indicatorWidth);
            LoadField(archive, "indicatorColor", params.indicatorColor);
        }

        //-------------------------------------------------------------
        //! @brief  チューニング値を得る関数（初回の呼び出しで1度だけ読む）
        //-------------------------------------------------------------
        const PickupIndicatorParams& GetParams() {
            static const PickupIndicatorParams s_params = LoadSystemParams<PickupIndicatorParams>("PickupIndicator");
            return s_params;
        }
    }    // namespace

    //-------------------------------------------------------------
    //! @brief 更新処理
    //-------------------------------------------------------------
    void PickupIndicatorSystem::Update(Tsukino::ECS::Registry& registry, float /*deltaTime*/) {
        const PickupIndicatorParams& params = GetParams();

        Tsukino::EngineIntegration::EngineContext* ctx = registry.GetContext<Tsukino::EngineIntegration::EngineContext*>();
        if(!ctx || !ctx->window)
            return;

        //-------------------------------------------------------------
        // メインカメラ（isPrimary=true）のViewProjectionを取得。WorldAnchorSystem/EliteIndicatorSystemと同じ手順
        //-------------------------------------------------------------
        Tsukino::Core::Math::matrix cameraViewProj;
        bool                        hasCamera = false;
        {
            auto cameraView = registry.View<Tsukino::BuiltIn::ECS::CameraComponent>();
            for(auto entity : cameraView) {
                const auto& camera = cameraView.get<Tsukino::BuiltIn::ECS::CameraComponent>(entity);
                if(camera.isPrimary) {
                    cameraViewProj = camera.viewProjMatrix;
                    hasCamera      = true;
                    break;
                }
            }
        }

        if(!hasCamera) {
            for(Tsukino::ECS::Entity entity : m_indicatorEntities)
                HideUiSprite(registry, entity);
            return;
        }

        const float screenWidth  = static_cast<float>(ctx->window->GetWidth());
        const float screenHeight = static_cast<float>(ctx->window->GetHeight());
        const float centerX      = screenWidth * 0.5f;
        const float centerY      = screenHeight * 0.5f;
        const float halfW        = std::max(centerX - params.edgeMargin, 1.0f);
        const float halfH        = std::max(centerY - params.edgeMargin, 1.0f);

        //-------------------------------------------------------------
        // 落ちている拾得アイテムの位置（ラベルを出す高さへ寄せる。PickupSystemと同じ流儀）を集める
        //-------------------------------------------------------------
        std::vector<hlslpp::float3> pickupPositions;
        auto                        pickupView = registry.View<PickupComponent, Tsukino::BuiltIn::ECS::TransformComponent>();
        pickupView.each([&](entt::entity, const PickupComponent& pickup, const Tsukino::BuiltIn::ECS::TransformComponent& transform) {
            pickupPositions.push_back(transform.position + hlslpp::float3(0.0f, pickup.labelHeight, 0.0f));
        });

        //-------------------------------------------------------------
        // 矢印のプールを必要な数まで増やす（同時に落ちている数の上限が無いため、
        // EliteIndicatorSystemのような固定数ではなく必要な分だけ増やしていく）
        //-------------------------------------------------------------
        while(m_indicatorEntities.size() < pickupPositions.size())
            m_indicatorEntities.push_back(CreateUiRectEntity(registry, *ctx, UI::kPickupIndicator));

        const hlslpp::float4 tint(params.indicatorColor.x, params.indicatorColor.y, params.indicatorColor.z, 1.0f);

        size_t slot = 0;
        for(const hlslpp::float3& position : pickupPositions) {
            hlslpp::float4 clip = hlslpp::mul(hlslpp::float4(position, 1.0f), cameraViewProj);

            // abs(w)で割ることで、カメラ後方（w<0）の対象でも符号反転なしに正しい方角のNDCになる
            const float          absW = std::max(std::abs(clip.w), 1.0e-4f);
            const hlslpp::float2 ndc  = hlslpp::float2(clip.x, clip.y) / absW;

            const bool onScreen = (clip.w > 0.0f) && std::abs(ndc.x) <= 1.0f && std::abs(ndc.y) <= 1.0f;
            if(onScreen) {
                HideUiSprite(registry, m_indicatorEntities[slot]);
                ++slot;
                continue;
            }

            //-------------------------------------------------------------
            // 画面中心からの方向へ、余白付き矩形の境界まで伸ばした点を求める
            //-------------------------------------------------------------
            hlslpp::float2 dir(ndc.x * centerX, -ndc.y * centerY);    // NDCのYは上向きなので、下向きの画面ピクセルへ変換する際に反転する
            const float    dirLength = hlslpp::length(dir);
            if(dirLength < 1.0e-3f)
                dir = hlslpp::float2(0.0f, -1.0f);
            else
                dir = dir / dirLength;

            const float scaleX = (std::abs(dir.x) > 1.0e-4f) ? halfW / std::abs(dir.x) : std::numeric_limits<float>::max();
            const float scaleY = (std::abs(dir.y) > 1.0e-4f) ? halfH / std::abs(dir.y) : std::numeric_limits<float>::max();
            const float scale  = std::min(scaleX, scaleY);

            const float screenX = centerX + dir.x * scale;
            const float screenY = centerY + dir.y * scale;

            // 線対称な棒なので180°の食い違いは見た目に影響しない
            const float roll = std::atan2(dir.y, dir.x) - kHalfPi;

            StretchSpriteRotated(registry, *ctx, m_indicatorEntities[slot], screenX, screenY, params.indicatorWidth,
                                 params.indicatorLength, roll, tint);
            ++slot;
        }

        for(; slot < m_indicatorEntities.size(); ++slot)
            HideUiSprite(registry, m_indicatorEntities[slot]);
    }
}    // namespace CombatAndroid::ECS
