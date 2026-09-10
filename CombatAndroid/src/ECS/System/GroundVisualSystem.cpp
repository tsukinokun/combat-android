//-------------------------------------------------------------
//! @file   GroundVisualSystem.cpp
//! @brief  GroundVisualSystemクラスの実装
//-------------------------------------------------------------
#include <CombatAndroid/ECS/System/GroundVisualSystem.hpp>
#include <CombatAndroid/ECS/Component/GroundFollowComponent.hpp>
#include <Tsukino/EngineIntegration/EngineContext.hpp>

#include <Tsukino/BuiltIn/ECS/Component/TransformComponent.hpp>
#include <Tsukino/BuiltIn/BuiltInAssets.hpp>

#include <Tsukino/GraphicsCommon/Mesh/MeshData.hpp>
#include <Tsukino/GraphicsCommon/Vertex/VertexPNUV.hpp>
#include <Tsukino/GraphicsCommon/Vertex/VertexFormat.hpp>

#include <Tsukino/Engine/Asset/AssetManager.hpp>
#include <Tsukino/Engine/Asset/Shader/ShaderAsset.hpp>
#include <Tsukino/Engine/Asset/Texture/TextureAsset.hpp>

#include <Tsukino/Renderer/Renderer.hpp>
#include <Tsukino/Renderer/DrawCommand.hpp>
#include <Tsukino/Renderer/ShaderSlots.hpp>
#include <Tsukino/Renderer/DX11/PipelineFactory.hpp>
#include <Tsukino/Renderer/DX11/UserConstantBuffer.hpp>

#include <Tsukino/Core/Log.hpp>
#include <Tsukino/Core/Math/Matrix.hpp>
#include <Tsukino/Core/Path.hpp>

#include <entt/entt.hpp>
#include <cstring>
#include <vector>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    namespace {
        //! 板の半径（一辺の半分）。地面コライダーの半径2000・草原の可視半径1800より
        //! 一回り広くして、板の端が画面に映り込まないようにする
        constexpr float kGroundHalfSize = 2500.0f;

        //! タイル1枚ぶんのワールド距離。Ground.vs.hlslがUVをワールド座標から
        //! 算出する際に使う（CBufferGround::tileParams.x）
        constexpr float kGroundTileWorldSize = 250.0f;

        //--------------------------------------------------------------
        //! 土の板1枚ぶんのメッシュを組み立てます（4頂点・6インデックスの静的な平面）。
        //! @return 板メッシュのデータ
        //! @note   ローカルYは常に0（地面コライダーの上面に合わせる）。
        //!         UVはメッシュには焼き込まない（0で埋めるだけの未使用値）。
        //!         Ground.vs.hlslがワールドXZ座標から算出したUVで上書きする
        //!         ため、板がプレイヤーへ追従して動いてもテクスチャが世界に
        //!         対して固定されて見える（AnisotropicWrapサンプラーで
        //!         繰り返し表示。継ぎ目が出ないようgenerate_dirt_ground.py側で
        //!         タイル張り前提に作ってある）
        //--------------------------------------------------------------
        Tsukino::GraphicsCommon::MeshData BuildGroundMeshData() {
            Tsukino::GraphicsCommon::MeshData mesh;
            mesh.format       = Tsukino::GraphicsCommon::VertexFormat::PositionNormalUV;
            mesh.vertexStride = sizeof(Tsukino::GraphicsCommon::VertexPNUV);

            std::vector<Tsukino::GraphicsCommon::VertexPNUV> vertices(4);
            vertices[0].position = {-kGroundHalfSize, 0.0f, -kGroundHalfSize};
            vertices[1].position = {kGroundHalfSize, 0.0f, -kGroundHalfSize};
            vertices[2].position = {kGroundHalfSize, 0.0f, kGroundHalfSize};
            vertices[3].position = {-kGroundHalfSize, 0.0f, kGroundHalfSize};

            for(auto& v : vertices) {
                v.normal = {0.0f, 1.0f, 0.0f};
                v.uv     = {0.0f, 0.0f};    // 未使用。Ground.vs.hlslがワールド座標から算出する
            }

            mesh.indices = {0, 1, 2, 0, 2, 3};

            mesh.vertexCount = static_cast<Tsukino::u32>(vertices.size());
            mesh.vertexData.resize(vertices.size() * sizeof(Tsukino::GraphicsCommon::VertexPNUV));
            std::memcpy(mesh.vertexData.data(), vertices.data(), mesh.vertexData.size());

            mesh.indexCount = static_cast<Tsukino::u32>(mesh.indices.size());

            return mesh;
        }
    }    // namespace

    //-------------------------------------------------------------
    //! @brief 更新処理
    //-------------------------------------------------------------
    void GroundVisualSystem::Update(Tsukino::ECS::Registry& registry, float /*deltaTime*/) {
        Tsukino::EngineIntegration::EngineContext* ctx = registry.GetContext<Tsukino::EngineIntegration::EngineContext*>();
        if(!ctx || !ctx->renderer || !ctx->assetManager || !ctx->builtinAssets)
            return;

        //--------------------------------------------------------------
        // 地面エンティティ（GroundFollowComponentを持つ最初のエンティティ）を追う。
        // GroundFollowSystemが同じフレームでX/Zをプレイヤーへ寄せた後のものを読む
        //--------------------------------------------------------------
        hlslpp::float3 groundPosition(0.0f, 0.0f, 0.0f);
        bool           found = false;

        auto groundView = registry.View<GroundFollowComponent, Tsukino::BuiltIn::ECS::TransformComponent>();
        groundView.each([&](entt::entity, const GroundFollowComponent&, const Tsukino::BuiltIn::ECS::TransformComponent& transform) {
            if(found)
                return;

            groundPosition = transform.position;
            found          = true;
        });

        if(!found)
            return;

        //--------------------------------------------------------------
        // 板メッシュの作成（形状は固定なので初回のみ）
        //--------------------------------------------------------------
        if(m_groundMesh.indexCount == 0) {
            const Tsukino::GraphicsCommon::MeshData meshData = BuildGroundMeshData();

            m_groundMesh = Tsukino::Renderer::CreateMeshBuffer(ctx->renderer->GetDevice(), meshData);

            if(m_groundMesh.indexCount == 0) {
                Tsukino::Core::Log::Error("GroundVisualSystem - failed to create the ground mesh. Ground will not be drawn.");
                return;
            }
        }

        //--------------------------------------------------------------
        // 土テクスチャの遅延ロード
        //--------------------------------------------------------------
        if(!m_dirtTextureHandle.IsValid())
            m_dirtTextureHandle = ctx->assetManager->Load(Tsukino::Core::Path("CombatAndroid/Assets/Textures/Ground/DirtGround.bmp"));

        auto textureAsset = std::static_pointer_cast<Tsukino::Asset::TextureAsset>(ctx->assetManager->Get(m_dirtTextureHandle));

        //--------------------------------------------------------------
        // 頂点シェーダーの構築用パラメータ（CBSlot::User0）を更新する
        //--------------------------------------------------------------
        CBufferGround groundParams{};
        groundParams.tileParams = hlslpp::float4(kGroundTileWorldSize, 0.0f, 0.0f, 0.0f);

        if(!m_paramBuffer.IsValid()) {
            m_paramBuffer = Tsukino::Renderer::CreateUserConstantBuffer(ctx->renderer->GetDevice(), sizeof(CBufferGround));

            if(!m_paramBuffer.IsValid()) {
                Tsukino::Core::Log::Error("GroundVisualSystem - failed to create the ground constant buffer. Ground will not be drawn.");
                return;
            }
        }

        Tsukino::Renderer::UpdateUserConstantBuffer(ctx->renderer->GetContext(), m_paramBuffer, &groundParams, sizeof(groundParams));

        //--------------------------------------------------------------
        // マテリアルの構築。頂点シェーダーはゲームAssetsのGround.vs.hlsl
        // （ワールド座標からUVを算出する。理由はヘッダのコメント参照）、
        // ピクセルシェーダーはモデルと同じGBuffer.ps.hlsl。草原と同じ借り方で、
        // ディファードライティング・ポイントライト・フォグが自動的に乗る
        //--------------------------------------------------------------
        Tsukino::Asset::AssetHandle groundVSHandle =
            ctx->assetManager->Load(Tsukino::Core::Path("CombatAndroid/Assets/Shaders/Ground.vs.hlsl"));

        auto vsAsset = std::static_pointer_cast<Tsukino::Asset::ShaderAsset>(ctx->assetManager->Get(groundVSHandle));
        auto psAsset = std::static_pointer_cast<Tsukino::Asset::ShaderAsset>(ctx->assetManager->Get(ctx->builtinAssets->shaders.gbufferPS));
        if(!vsAsset || !psAsset)
            return;

        std::shared_ptr<Tsukino::Renderer::PipelineState> pipeline = ctx->renderer->GetPipelineFactory()->Create(
            *vsAsset, *psAsset, Tsukino::GraphicsCommon::VertexFormat::PositionNormalUV, Tsukino::Renderer::DepthMode::ReadWrite,
            Tsukino::Renderer::BlendMode::Opaque);

        if(!pipeline)
            return;

        Tsukino::Renderer::Material& material = ctx->renderer->AllocMaterial();
        material.SetPipeline(pipeline.get());
        // AnisotropicWrap：タイル張り（UVが0〜1を超えて繰り返す）かつ、地面を
        // 斜めに見下ろす角度でもぼやけにくい異方性フィルタのサンプラー
        material.SetSampler(ctx->renderer->GetSampler(Tsukino::GraphicsCommon::SamplerType::AnisotropicWrap));

        ID3D11ShaderResourceView* albedoSRV = textureAsset ? ctx->renderer->GetTextureSRV(*textureAsset) : nullptr;
        material.SetTexture(Tsukino::Renderer::SRVSlot::Albedo, albedoSRV ? albedoSRV : ctx->renderer->GetWhiteTextureSRV());

        // ノーマルマップは使わない。フラット法線を入れると頂点法線（真上）がそのまま残る
        material.SetTexture(Tsukino::Renderer::SRVSlot::Normal, ctx->renderer->GetFlatNormalTextureSRV());
        material.SetTexture(Tsukino::Renderer::SRVSlot::MetallicRoughness, ctx->renderer->GetWhiteTextureSRV());
        material.SetTexture(Tsukino::Renderer::SRVSlot::Emissive, ctx->renderer->GetWhiteTextureSRV());
        material.SetTexture(Tsukino::Renderer::SRVSlot::AO, ctx->renderer->GetWhiteTextureSRV());

        //--------------------------------------------------------------
        // マテリアル定数。土は金属ではないので metallic は0、
        // 表面はざらついているので roughness は高め（草より少し高くする）
        //--------------------------------------------------------------
        Tsukino::Renderer::CBufferMaterial& materialData = ctx->renderer->AllocMaterialData();
        materialData            = Tsukino::Renderer::CBufferMaterial{};
        materialData.baseColor  = hlslpp::float4(1.0f, 1.0f, 1.0f, 1.0f);
        materialData.emissive   = hlslpp::float3(0.0f, 0.0f, 0.0f);
        materialData.metallic   = 0.0f;
        materialData.roughness  = 0.95f;
        materialData.specular   = 0.1f;
        materialData.rimColor   = hlslpp::float4(0.0f, 0.0f, 0.0f, 0.0f);
        materialData.rimParams  = hlslpp::float4(1.0f, 0.0f, 0.0f, 0.0f);    // z=alphaCutoff。土はくり抜かないので0

        //--------------------------------------------------------------
        // 描画コマンドを1本だけ積む。
        // Yは常に0固定（地面コライダーの上面）にする。GroundFollowComponent::
        // groundHeightはコライダー中心（-5）であってこの板の高さではないため、
        // エンティティのTransform.position.yはそのまま使わない
        //--------------------------------------------------------------
        Tsukino::Renderer::DrawCommand cmd{};
        cmd.mesh          = &m_groundMesh;
        cmd.material      = &material;
        cmd.materialData  = &materialData;
        cmd.transform     = Tsukino::Core::Math::matrix::translate(groundPosition.x, 0.0f, groundPosition.z);
        cmd.prevTransform = cmd.transform;
        cmd.hasPrevFrame  = false;
        cmd.pass          = Tsukino::Renderer::RenderPass::GBuffer;

        // 巨大な板に自分自身の影を落とさせても意味が薄く、シャドウマップの
        // 解像度を無駄に消費するだけなので落とさない
        cmd.castsShadow = false;

        // ワールド座標からUVを算出するためのパラメータをゲーム予約枠へ渡す
        cmd.userConstantBuffer = m_paramBuffer.buffer.Get();
        cmd.userConstantSlot   = Tsukino::Renderer::CBSlot::User0;

        ctx->renderer->PushDrawCommand(cmd);
    }
}    // namespace CombatAndroid::ECS
