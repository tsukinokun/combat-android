//-------------------------------------------------------------
//! @file   EliteIndicatorSystem.cpp
//! @brief  EliteIndicatorSystemクラスの実装
//-------------------------------------------------------------
#include <CombatAndroid/ECS/System/UI/EliteIndicatorSystem.hpp>
#include <CombatAndroid/ECS/Serialization/Common/SerializationHelper.hpp>
#include <CombatAndroid/ECS/Utility/Table/TableJson.hpp>
#include <CombatAndroid/ECS/Utility/UI/UiSprite.hpp>
#include <CombatAndroid/ECS/Utility/Spawn/EliteEnemy.hpp>
#include <CombatAndroid/ECS/Component/Enemy/EliteEnemyComponent.hpp>
#include <CombatAndroid/ECS/Component/Combat/HealthComponent.hpp>
#include <CombatAndroid/UI/UiSortOrder.hpp>

#include <Tsukino/EngineIntegration/EngineContext.hpp>

#include <Tsukino/BuiltIn/ECS/Component/CameraComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/CollisionComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/TransformComponent.hpp>

#include <Tsukino/Core/Log.hpp>
#include <Tsukino/Core/Window.hpp>
#include <Tsukino/Core/Math/Matrix.hpp>

#include <entt/entt.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    namespace {
        constexpr float kHalfPi = 1.5707963267948966f;

        //-------------------------------------------------------------
        //! @struct EliteIndicatorParams
        //! @brief  見た目の調整値（Assets/Tables/Systems/EliteIndicator.json。ここの初期値はJSONにキーが無いときの既定値）
        //-------------------------------------------------------------
        struct EliteIndicatorParams {
            float edgeMargin       = 60.0f;    //!< 画面端からの余白（ピクセル）。矢印がここより外へはみ出さない
            float indicatorLength  = 36.0f;    //!< 矢印（棒）の長さ
            float indicatorWidth   = 10.0f;    //!< 矢印（棒）の太さ
        };

        template <class Archive>
        void load(Archive& archive, EliteIndicatorParams& params) {
            LoadField(archive, "edgeMargin", params.edgeMargin);
            LoadField(archive, "indicatorLength", params.indicatorLength);
            LoadField(archive, "indicatorWidth", params.indicatorWidth);
        }

        //-------------------------------------------------------------
        //! @brief  チューニング値を得る関数（初回の呼び出しで1度だけ読む）
        //-------------------------------------------------------------
        const EliteIndicatorParams& GetParams() {
            static const EliteIndicatorParams s_params = LoadSystemParams<EliteIndicatorParams>("EliteIndicator");
            return s_params;
        }
    }    // namespace

    //-------------------------------------------------------------
    //! @brief 更新処理
    //-------------------------------------------------------------
    void EliteIndicatorSystem::Update(Tsukino::ECS::Registry& registry, float deltaTime) {
        const EliteIndicatorParams& params = GetParams();

        Tsukino::EngineIntegration::EngineContext* ctx = registry.GetContext<Tsukino::EngineIntegration::EngineContext*>();
        if(!ctx || !ctx->window)
            return;

        //-------------------------------------------------------------
        // 矢印のプールを初回だけ生成する（HitImpactEffectSystemの遅延ロードと同じ流儀）
        //-------------------------------------------------------------
        if(m_indicatorEntities.empty()) {
            const int poolSize = GetEliteSettings().maxLiveElites;
            m_indicatorEntities.reserve(poolSize);
            for(int i = 0; i < poolSize; ++i)
                m_indicatorEntities.push_back(CreateUiRectEntity(registry, *ctx, UI::kEliteIndicator));

            // 診断用（調整用。動作確認後は削除してよい）
            Tsukino::Core::Log::Info("EliteIndicatorSystem: pool created, size=" + std::to_string(m_indicatorEntities.size()));
        }

        //-------------------------------------------------------------
        // メインカメラ（isPrimary=true）のViewProjectionを取得。WorldAnchorSystemと同じ手順
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
        // 生きているエリートの位置（胴体中心へ寄せる。DamageNumberSystemと同じ流儀）を集める
        //-------------------------------------------------------------
        std::vector<hlslpp::float3> elitePositions;
        auto                        eliteView = registry.View<EliteEnemyComponent, HealthComponent, Tsukino::BuiltIn::ECS::TransformComponent>();
        eliteView.each([&](entt::entity entity, const EliteEnemyComponent&, const HealthComponent& health,
                           const Tsukino::BuiltIn::ECS::TransformComponent& transform) {
            if(health.isDead)
                return;

            hlslpp::float3 position = transform.position;
            if(const auto* collision = registry.try_get<Tsukino::BuiltIn::ECS::CollisionComponent>(entity))
                position.y += collision->offsetPosition.y;

            elitePositions.push_back(position);
        });

        const hlslpp::float3 glow = GetEliteSettings().glowColor;
        const hlslpp::float4 tint(glow.x, glow.y, glow.z, 1.0f);

        // 診断用：1秒に1回、生きているエリートの数と（いれば）先頭の計算値をログへ出す（調整用。動作確認後は削除してよい）
        m_logTimer += deltaTime;
        const bool shouldLogTick = m_logTimer >= 1.0f;
        if(shouldLogTick) {
            m_logTimer = 0.0f;
            Tsukino::Core::Log::Info("EliteIndicatorSystem: eliteCount=" + std::to_string(elitePositions.size()));
        }
        const bool shouldLog = shouldLogTick && !elitePositions.empty();

        size_t slot = 0;
        for(const hlslpp::float3& position : elitePositions) {
            if(slot >= m_indicatorEntities.size())
                break;

            hlslpp::float4 clip = hlslpp::mul(hlslpp::float4(position, 1.0f), cameraViewProj);

            // abs(w)で割ることで、カメラ後方（w<0）の対象でも符号反転なしに正しい方角のNDCになる
            const float          absW = std::max(std::abs(clip.w), 1.0e-4f);
            const hlslpp::float2 ndc  = hlslpp::float2(clip.x, clip.y) / absW;

            const bool onScreen = (clip.w > 0.0f) && std::abs(ndc.x) <= 1.0f && std::abs(ndc.y) <= 1.0f;

            if(shouldLog && slot == 0) {
                Tsukino::Core::Log::Info("EliteIndicatorSystem: clip.w=" + std::to_string(clip.w) + " ndc=(" + std::to_string(ndc.x) + "," +
                                         std::to_string(ndc.y) + ") onScreen=" + std::to_string(onScreen) +
                                         " hasCamera=" + std::to_string(hasCamera) + " screenW=" + std::to_string(screenWidth) +
                                         " screenH=" + std::to_string(screenHeight));
            }

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

            if(shouldLog && slot == 0) {
                Tsukino::Core::Log::Info("EliteIndicatorSystem: screen=(" + std::to_string(screenX) + "," + std::to_string(screenY) +
                                         ") roll=" + std::to_string(roll) + " width=" + std::to_string(params.indicatorWidth) +
                                         " height=" + std::to_string(params.indicatorLength));
            }

            StretchSpriteRotated(registry, *ctx, m_indicatorEntities[slot], screenX, screenY, params.indicatorWidth,
                                 params.indicatorLength, roll, tint);
            ++slot;
        }

        for(; slot < m_indicatorEntities.size(); ++slot)
            HideUiSprite(registry, m_indicatorEntities[slot]);
    }
}    // namespace CombatAndroid::ECS
