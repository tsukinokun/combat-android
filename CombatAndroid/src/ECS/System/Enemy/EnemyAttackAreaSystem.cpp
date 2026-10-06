//-------------------------------------------------------------
//! @file   EnemyAttackAreaSystem.cpp
//! @brief  EnemyAttackAreaSystemクラスの実装
//-------------------------------------------------------------
#include <CombatAndroid/ECS/System/Enemy/EnemyAttackAreaSystem.hpp>
#include <CombatAndroid/ECS/Serialization/Common/SerializationHelper.hpp>
#include <CombatAndroid/ECS/Utility/Table/TableJson.hpp>
#include <Tsukino/Core/Math/Serialization/HlslppSerialization.hpp>
#include <CombatAndroid/ECS/Component/Enemy/EnemyAnimationSetComponent.hpp>
#include <CombatAndroid/ECS/Component/Enemy/EnemyAttackHitboxComponent.hpp>
#include <CombatAndroid/ECS/Component/Enemy/EnemyComponent.hpp>
#include <CombatAndroid/ECS/Component/Combat/HealthComponent.hpp>

#include <Tsukino/EngineIntegration/EngineContext.hpp>

#include <Tsukino/BuiltIn/ECS/Component/TransformComponent.hpp>
#include <Tsukino/BuiltIn/BuiltInAssets.hpp>

#include <Tsukino/GraphicsCommon/Mesh/MeshData.hpp>
#include <Tsukino/GraphicsCommon/Vertex/VertexPUV.hpp>
#include <Tsukino/GraphicsCommon/Vertex/VertexFormat.hpp>

#include <Tsukino/Engine/Asset/AssetManager.hpp>
#include <Tsukino/Engine/Asset/Shader/ShaderAsset.hpp>

#include <Tsukino/Renderer/Renderer.hpp>
#include <Tsukino/Renderer/DrawCommand.hpp>
#include <Tsukino/Renderer/DX11/Material.hpp>
#include <Tsukino/Renderer/DX11/PipelineFactory.hpp>

#include <Tsukino/Core/Log.hpp>
#include <Tsukino/Core/Math/Matrix.hpp>

#include <entt/entt.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    namespace {
        constexpr int   kSegments = 32;             //!< 円・扇の分割数（メッシュの構造なのでコードに残す）
        constexpr float kPi       = 3.14159265f;

        //-------------------------------------------------------------
        //! @struct EnemyAttackAreaParams
        //! @brief  見た目と挙動のチューニング値（Assets/Tables/Systems/EnemyAttackArea.json。ここの初期値はJSONにキーが無いときの既定値）
        //-------------------------------------------------------------
        struct EnemyAttackAreaParams {
            //-------------------------------------------------------------
            // 見た目のチューニング値
            //-------------------------------------------------------------

            //! 色。EnemyAttackTelegraph.json の telegraphColor と同じ赤にして、
            //! 体の光と足元の範囲が同じ攻撃の予兆だと一目で結び付くようにする
            hlslpp::float3 areaColor = hlslpp::float3(1.0f, 0.12f, 0.08f);

            //! 範囲全体の薄い面。草の緑と地面の茶の上でも赤と読めるよう、半透明の中では濃いめにしてある
            float outerAlpha = 0.28f;
            float fillAlpha = 0.50f;    //!< 判定の瞬間へ向けて広がる内側の塗り（外側の面に重なる）

            //! 攻撃に入った直後に面がパッと出ないよう、この秒数で薄い面を立ち上げる
            float fadeInSeconds = 0.12f;

            //! 地面からの高さ。足元の草（高さ約34、GrassFieldSystem）は地面をほぼ覆っていて、
            //! 地面すれすれに置くとTPSカメラの浅い角度からは草に隠れて見えない。
            //! 草の穂先の少し下に置き、面が草の上に乗って見えるようにする（背の高い穂先は面を突き抜けて
            //! 見えるので、地面から浮いた板には見えない）。深度テストは行うので、敵の脚には正しく隠れる
            float areaHeight = 28.0f;

            //-------------------------------------------------------------
            // 形。当たり判定の形から決める：
            //   判定が球（頭突き：終点ボーン無し）  → 前方に置いた円
            //   判定がカプセル（腕・武器を振る）    → 前方の扇
            //-------------------------------------------------------------
            float fanAngleDegrees = 120.0f;    //!< 扇の中心角
            float discCenterRatio = 0.6f;      //!< 円の中心を置く位置（attackRangeに対する前方の割合）
            float discRadiusScale = 1.2f;      //!< 円の半径（判定半径に対する倍率。頭の突き出しのぶれを含める）
        };

        template <class Archive>
        void load(Archive& archive, EnemyAttackAreaParams& params) {
            LoadField(archive, "areaColor", params.areaColor);
            LoadField(archive, "outerAlpha", params.outerAlpha);
            LoadField(archive, "fillAlpha", params.fillAlpha);
            LoadField(archive, "fadeInSeconds", params.fadeInSeconds);
            LoadField(archive, "areaHeight", params.areaHeight);
            LoadField(archive, "fanAngleDegrees", params.fanAngleDegrees);
            LoadField(archive, "discCenterRatio", params.discCenterRatio);
            LoadField(archive, "discRadiusScale", params.discRadiusScale);
        }

        //-------------------------------------------------------------
        //! @brief  チューニング値を得る関数（初回の呼び出しで1度だけ読む）
        //-------------------------------------------------------------
        const EnemyAttackAreaParams& GetParams() {
            static const EnemyAttackAreaParams s_params = LoadSystemParams<EnemyAttackAreaParams>("EnemyAttackArea");
            return s_params;
        }

        //-------------------------------------------------------------
        //! @brief  XZ平面の扇（中心角360°なら円）のメッシュを作る関数
        //! @param  angleDegrees [in] 中心角（度）。+Z方向を中心に左右へ開く
        //! @return 半径1のメッシュデータ（要が原点）
        //! @note   上から見ても下から見ても描けるよう、表裏両方の三角形を入れておく
        //!         （パイプラインのカリング向きに依存させない）
        //-------------------------------------------------------------
        Tsukino::GraphicsCommon::MeshData BuildFanMeshData(float angleDegrees) {
            Tsukino::GraphicsCommon::MeshData mesh;
            mesh.format       = Tsukino::GraphicsCommon::VertexFormat::PositionUV;
            mesh.vertexStride = sizeof(Tsukino::GraphicsCommon::VertexPUV);

            const float angle = angleDegrees * kPi / 180.0f;

            std::vector<Tsukino::GraphicsCommon::VertexPUV> vertices;
            vertices.push_back({{0.0f, 0.0f, 0.0f}, {0.5f, 0.5f}});    // 要（円なら中心）
            for(int i = 0; i <= kSegments; ++i) {
                const float t = -angle * 0.5f + angle * static_cast<float>(i) / static_cast<float>(kSegments);
                vertices.push_back({{std::sin(t), 0.0f, std::cos(t)}, {0.5f, 0.5f}});
            }

            for(int i = 0; i < kSegments; ++i) {
                const Tsukino::u32 a = static_cast<Tsukino::u32>(i + 1);
                const Tsukino::u32 b = static_cast<Tsukino::u32>(i + 2);
                mesh.indices.insert(mesh.indices.end(), {0u, a, b, 0u, b, a});
            }

            mesh.vertexCount = static_cast<Tsukino::u32>(vertices.size());
            mesh.vertexData.resize(vertices.size() * sizeof(Tsukino::GraphicsCommon::VertexPUV));
            std::memcpy(mesh.vertexData.data(), vertices.data(), mesh.vertexData.size());
            mesh.indexCount = static_cast<Tsukino::u32>(mesh.indices.size());

            return mesh;
        }
    }    // namespace

    //-------------------------------------------------------------
    //! @brief システムの更新
    //-------------------------------------------------------------
    void EnemyAttackAreaSystem::Update(Tsukino::ECS::Registry& registry, float /*deltaTime*/) {
        const EnemyAttackAreaParams& params = GetParams();

        Tsukino::EngineIntegration::EngineContext* ctx = registry.GetContext<Tsukino::EngineIntegration::EngineContext*>();
        if(!ctx || !ctx->renderer || !ctx->assetManager || !ctx->builtinAssets)
            return;

        //-------------------------------------------------------------
        // メッシュとパイプラインは形・設定が固定なので初回だけ作る
        //-------------------------------------------------------------
        if(m_discMesh.indexCount == 0) {
            m_discMesh = Tsukino::Renderer::CreateMeshBuffer(ctx->renderer->GetDevice(), BuildFanMeshData(360.0f));
            m_fanMesh  = Tsukino::Renderer::CreateMeshBuffer(ctx->renderer->GetDevice(), BuildFanMeshData(params.fanAngleDegrees));

            if(m_discMesh.indexCount == 0 || m_fanMesh.indexCount == 0) {
                Tsukino::Core::Log::Error("EnemyAttackAreaSystem - failed to create the area meshes. Attack areas will not be drawn.");
                return;
            }
        }

        if(!m_pipeline) {
            auto vsAsset = std::static_pointer_cast<Tsukino::Asset::ShaderAsset>(ctx->assetManager->Get(ctx->builtinAssets->shaders.spriteWorldVS));
            auto psAsset = std::static_pointer_cast<Tsukino::Asset::ShaderAsset>(ctx->assetManager->Get(ctx->builtinAssets->shaders.spritePS));
            if(!vsAsset || !psAsset)
                return;

            // ワールド空間スプライトと同じ組み合わせ：地面・敵の深度に対してテストだけ行い、
            // 自身は深度を書かない半透明（SpriteRendererSystemのm_worldPipelineCacheと同じ）
            m_pipeline = ctx->renderer->GetResources().GetPipelineFactory()->Create(
                *vsAsset, *psAsset, Tsukino::GraphicsCommon::VertexFormat::Sprite, Tsukino::Renderer::DepthMode::ReadOnly,
                Tsukino::Renderer::BlendMode::Alpha);
            if(!m_pipeline)
                return;
        }

        //-------------------------------------------------------------
        //! @brief 1枚ぶんの描画コマンドを積む
        //-------------------------------------------------------------
        auto pushArea = [&](Tsukino::Renderer::MeshBuffer& mesh, const hlslpp::float3& position, const hlslpp::quaternion& rotation,
                            float radius, float alpha) {
            if(radius <= 0.0f || alpha <= 0.0f)
                return;

            Tsukino::Renderer::Material& material = ctx->renderer->GetDrawQueue().AllocMaterial();
            material.SetPipeline(m_pipeline.get());
            material.SetSampler(ctx->renderer->GetResources().GetSampler(Tsukino::GraphicsCommon::SamplerType::LinearClamp));
            material.SetTexture(ctx->renderer->GetResources().GetWhiteTextureSRV());

            Tsukino::Renderer::CBufferMaterial& materialData = ctx->renderer->GetDrawQueue().AllocMaterialData();
            materialData                                      = Tsukino::Renderer::CBufferMaterial{};
            materialData.baseColor                            = hlslpp::float4(params.areaColor, alpha);

            Tsukino::Renderer::DrawCommand cmd{};
            cmd.mesh          = &mesh;
            cmd.material      = &material;
            cmd.materialData  = &materialData;
            cmd.transform     = hlslpp::mul(Tsukino::Core::Math::matrix::scale(radius, 1.0f, radius),
                                            hlslpp::mul(Tsukino::Core::Math::matrix::fromQuaternion(rotation),
                                                        Tsukino::Core::Math::matrix::translate(position)));
            cmd.prevTransform = cmd.transform;
            cmd.hasPrevFrame  = false;
            cmd.pass          = Tsukino::Renderer::RenderPass::World;
            cmd.castsShadow   = false;

            ctx->renderer->GetDrawQueue().Push(cmd);
        };

        auto view = registry.View<EnemyComponent, EnemyAnimationSetComponent, EnemyAttackHitboxComponent, HealthComponent,
                                  Tsukino::BuiltIn::ECS::TransformComponent>();

        view.each([&](const EnemyComponent& enemy, const EnemyAnimationSetComponent& animSet, const EnemyAttackHitboxComponent& hitbox,
                      const HealthComponent& health, const Tsukino::BuiltIn::ECS::TransformComponent& transform) {
            //-------------------------------------------------------------
            // 出す条件はリムライトの予兆（EnemyAttackTelegraphSystem）と同じ：
            // 攻撃モーション中で、まだ判定が出ていない生きている敵
            //-------------------------------------------------------------
            const bool isWindingUp = !health.isDead && animSet.currentState == EnemyAnimState::Attack
                                     && animSet.attackTimer < hitbox.hitStartTime && hitbox.hitStartTime > 0.0f;
            if(!isWindingUp)
                return;

            // 塗りの広がり（0→1）。1になった瞬間に判定が出る
            const float progress = std::clamp(animSet.attackTimer / hitbox.hitStartTime, 0.0f, 1.0f);
            const float fadeIn   = std::clamp(animSet.attackTimer / params.fadeInSeconds, 0.0f, 1.0f);

            // モデルの正面は+Z（MoveToPlayerがatan2(x, z)で向きを決めている）
            const hlslpp::float3 forward  = hlslpp::mul(hlslpp::float3(0.0f, 0.0f, 1.0f), transform.rotation);
            hlslpp::float3       basePosition = transform.position;
            basePosition.y                    = params.areaHeight;

            if(hitbox.endBoneName.empty()) {
                //-------------------------------------------------------------
                // 頭突き：前方に置いた円。塗りは円の中心から広がる
                //-------------------------------------------------------------
                const hlslpp::float3 center = basePosition + forward * (enemy.attackRange * params.discCenterRatio);
                const float          radius = hitbox.radius * params.discRadiusScale;

                pushArea(m_discMesh, center, transform.rotation, radius, params.outerAlpha * fadeIn);
                pushArea(m_discMesh, center, transform.rotation, radius * progress, params.fillAlpha * fadeIn);
            } else {
                //-------------------------------------------------------------
                // 薙ぎ払い：足元を要にした前方の扇。塗りは要から外へ広がる。
                // 半径は足を止める距離（attackRange）に判定の太さを足した、実際に届く距離
                //-------------------------------------------------------------
                const float radius = enemy.attackRange + hitbox.radius;

                pushArea(m_fanMesh, basePosition, transform.rotation, radius, params.outerAlpha * fadeIn);
                pushArea(m_fanMesh, basePosition, transform.rotation, radius * progress, params.fillAlpha * fadeIn);
            }
        });
    }
}    // namespace CombatAndroid::ECS
