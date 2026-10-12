//-------------------------------------------------------------
//! @file   TreeFieldSystem.cpp
//! @brief  TreeFieldSystemクラスの実装
//-------------------------------------------------------------
#include <CombatAndroid/ECS/System/World/TreeFieldSystem.hpp>
#include <CombatAndroid/ECS/Component/World/TreeFieldComponent.hpp>
#include <CombatAndroid/ECS/Component/World/GrassFieldComponent.hpp>
#include <CombatAndroid/ECS/Component/Player/PlayerComponent.hpp>
#include <CombatAndroid/ECS/Utility/World/ProceduralTree.hpp>

#include <Tsukino/EngineIntegration/EngineContext.hpp>

#include <Tsukino/BuiltIn/ECS/Component/CollisionComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/TransformComponent.hpp>
#include <Tsukino/BuiltIn/BuiltInAssets.hpp>

#include <Tsukino/Engine/Asset/AssetManager.hpp>
#include <Tsukino/Engine/Asset/Shader/ShaderAsset.hpp>
#include <Tsukino/Engine/Asset/Texture/TextureAsset.hpp>
#include <Tsukino/Engine/Asset/Util/AssetHandleGenerator.hpp>

#include <Tsukino/Renderer/Renderer.hpp>
#include <Tsukino/Renderer/DrawCommand.hpp>
#include <Tsukino/Renderer/ShaderSlots.hpp>
#include <Tsukino/Renderer/DX11/PipelineFactory.hpp>

#include <Tsukino/Core/Log.hpp>
#include <Tsukino/Core/Math/Matrix.hpp>
#include <Tsukino/Core/Path.hpp>

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    namespace {
        constexpr float kTwoPi = 6.28318531f;

        //! 色のグラデーションテクスチャの高さと幅（草のグラデーションと同じ作り）
        constexpr Tsukino::u32 kGradientHeight = 32;
        constexpr Tsukino::u32 kGradientWidth  = 4;

        //-------------------------------------------------------------
        //! @brief  32bitの値を混ぜる（升目の座標から乱数を作るのに使う）
        //-------------------------------------------------------------
        std::uint32_t Mix(std::uint32_t h) {
            h ^= h >> 16;
            h *= 0x7feb352du;
            h ^= h >> 15;
            h *= 0x846ca68bu;
            h ^= h >> 16;
            return h;
        }

        //-------------------------------------------------------------
        //! @class  CellRandom
        //! @brief  升目の座標と種から決まる乱数の列。同じ升目なら、いつ何度調べても同じ値が出る
        //-------------------------------------------------------------
        class CellRandom {
        public:
            CellRandom(int cellX, int cellZ, std::uint32_t seed)
                : m_state(Mix(seed ^ Mix(static_cast<std::uint32_t>(cellX) * 0x8da6b343u ^ static_cast<std::uint32_t>(cellZ) * 0xd8163841u))) {}

            //! 0〜1の乱数
            float Next01() {
                m_state = Mix(m_state + 0x9e3779b9u);
                return static_cast<float>(m_state >> 8) / static_cast<float>(1u << 24);
            }

        private:
            std::uint32_t m_state;
        };

        //-------------------------------------------------------------
        //! @brief  升目の座標を1つのキーにまとめる
        //-------------------------------------------------------------
        std::uint64_t MakeCellKey(int cellX, int cellZ) {
            return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(cellX)) << 32) | static_cast<std::uint32_t>(cellZ);
        }

        //-------------------------------------------------------------
        //! @brief  下→上の色のグラデーションテクスチャを得る（同じ色なら作り直さず使い回す）
        //! @param  context [in,out] エンジンコンテキスト
        //! @param  name    [in]     キャッシュのキーに含める名前（樹皮・葉）
        //! @param  bottom  [in]     下（v=0）の色
        //! @param  top     [in]     上（v=1）の色
        //! @return テクスチャのSRV。作れなければnullptr
        //-------------------------------------------------------------
        ID3D11ShaderResourceView* GetGradientSRV(Tsukino::EngineIntegration::EngineContext& context, const char* name,
                                                 const hlslpp::float3& bottom, const hlslpp::float3& top) {
            if(!context.assetManager || !context.renderer)
                return nullptr;

            auto toByte = [](float v) { return static_cast<int>(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f); };

            const std::string key = std::string("procedural|tree|") + name + "|" + std::to_string(toByte(bottom.x)) + "_"
                                    + std::to_string(toByte(bottom.y)) + "_" + std::to_string(toByte(bottom.z)) + "_" + std::to_string(toByte(top.x))
                                    + "_" + std::to_string(toByte(top.y)) + "_" + std::to_string(toByte(top.z));
            const Tsukino::Asset::AssetHandle handle = Tsukino::Asset::AssetHandleGenerator::GenerateFromKey(key);

            if(Tsukino::Core::Ref<Tsukino::Asset::IAsset> existing = context.assetManager->Get(handle)) {
                auto texture = std::static_pointer_cast<Tsukino::Asset::TextureAsset>(existing);
                return context.renderer->GetResources().GetTextureSRV(*texture);
            }

            auto texture    = std::make_shared<Tsukino::Asset::TextureAsset>();
            texture->width  = kGradientWidth;
            texture->height = kGradientHeight;
            texture->format = DXGI_FORMAT_R8G8B8A8_UNORM;
            texture->pixels.resize(static_cast<size_t>(kGradientWidth) * kGradientHeight * 4);

            // 先頭の行がv=0（下）。下から上へbottom→topで埋める
            for(Tsukino::u32 row = 0; row < kGradientHeight; ++row) {
                const float          t     = static_cast<float>(row) / static_cast<float>(kGradientHeight - 1);
                const hlslpp::float3 color = bottom + (top - bottom) * t;
                for(Tsukino::u32 x = 0; x < kGradientWidth; ++x) {
                    const size_t offset          = (static_cast<size_t>(row) * kGradientWidth + x) * 4;
                    texture->pixels[offset + 0] = static_cast<Tsukino::u8>(toByte(color.x));
                    texture->pixels[offset + 1] = static_cast<Tsukino::u8>(toByte(color.y));
                    texture->pixels[offset + 2] = static_cast<Tsukino::u8>(toByte(color.z));
                    texture->pixels[offset + 3] = 255;
                }
            }

            texture->SetHandle(handle);
            context.assetManager->RegisterAsset(handle, texture);
            return context.renderer->GetResources().GetTextureSRV(*texture);
        }

        //-------------------------------------------------------------
        //! @brief  葉カードに貼る葉テクスチャ（2×2のアトラス）を得る（同じ色・種なら作り直さず使い回す）
        //! @param  context [in,out] エンジンコンテキスト
        //! @param  field   [in]     色と種を持つ調整値
        //! @return テクスチャのSRV。作れなければnullptr
        //! @note   葉の外はアルファ0。マテリアルのalphaCutoffでくり抜くので、影も葉の形になる
        //-------------------------------------------------------------
        ID3D11ShaderResourceView* GetLeafAtlasSRV(Tsukino::EngineIntegration::EngineContext& context, const TreeFieldComponent& field) {
            if(!context.assetManager || !context.renderer)
                return nullptr;

            LeafAtlasParams params;
            params.seed = field.seed;
            const float dark[3]  = {field.leafBottomColor.x, field.leafBottomColor.y, field.leafBottomColor.z};
            const float light[3] = {field.leafTopColor.x, field.leafTopColor.y, field.leafTopColor.z};
            for(int k = 0; k < 3; ++k) {
                params.darkColor[k]  = std::clamp(dark[k], 0.0f, 1.0f);
                params.lightColor[k] = std::clamp(light[k], 0.0f, 1.0f);
            }

            auto        toByte = [](float v) { return static_cast<int>(v * 255.0f + 0.5f); };
            std::string key    = "procedural|tree|leafatlas|" + std::to_string(params.seed);
            for(int k = 0; k < 3; ++k)
                key += "_" + std::to_string(toByte(params.darkColor[k])) + "_" + std::to_string(toByte(params.lightColor[k]));
            const Tsukino::Asset::AssetHandle handle = Tsukino::Asset::AssetHandleGenerator::GenerateFromKey(key);

            if(Tsukino::Core::Ref<Tsukino::Asset::IAsset> existing = context.assetManager->Get(handle)) {
                auto texture = std::static_pointer_cast<Tsukino::Asset::TextureAsset>(existing);
                return context.renderer->GetResources().GetTextureSRV(*texture);
            }

            auto texture    = std::make_shared<Tsukino::Asset::TextureAsset>();
            texture->width  = static_cast<Tsukino::u32>(params.size);
            texture->height = static_cast<Tsukino::u32>(params.size);
            texture->format = DXGI_FORMAT_R8G8B8A8_UNORM;
            texture->pixels = BuildLeafAtlasPixels(params);

            texture->SetHandle(handle);
            context.assetManager->RegisterAsset(handle, texture);
            return context.renderer->GetResources().GetTextureSRV(*texture);
        }

        //-------------------------------------------------------------
        //! @brief  木の形の1種類ぶんの値を、TreeFieldComponentの範囲から乱数で選ぶ
        //-------------------------------------------------------------
        TreeShapeParams MakeShapeParams(const TreeFieldComponent& field, int variantIndex) {
            CellRandom random(variantIndex, -7777, field.seed);    // 升目とは別の列にするため、ありえない座標を使う
            auto       range = [&](float a, float b) { return a + (b - a) * random.Next01(); };

            TreeShapeParams params;
            params.seed          = Mix(field.seed + static_cast<std::uint32_t>(variantIndex) * 7919u);
            params.trunkHeight   = range(field.trunkHeightMin, field.trunkHeightMax);
            params.trunkRadius   = range(field.trunkRadiusMin, field.trunkRadiusMax);
            params.branchDepth   = std::clamp(field.branchDepth, 1, 4);    // 5段以上は枝の数が爆発的に増えるので止める
            params.childCountMin = std::max(field.childCountMin, 1);
            params.childCountMax = std::max(field.childCountMax, params.childCountMin);
            params.spreadAngle   = range(field.spreadAngleMin, field.spreadAngleMax);
            params.canopyRadius  = range(field.canopyRadiusMin, field.canopyRadiusMax);
            params.leafCardsPerTip = std::clamp(field.leafCardsPerTip, 1, 200);
            params.leafCardSize    = std::max(field.leafCardSize, 1.0f);
            return params;
        }

        //-------------------------------------------------------------
        //! @brief  マテリアル1つ（パイプラインと色のテクスチャ）を今フレームぶん確保する
        //-------------------------------------------------------------
        Tsukino::Renderer::Material& AllocTreeMaterial(Tsukino::EngineIntegration::EngineContext& ctx, Tsukino::Renderer::PipelineState* pipeline,
                                                       ID3D11ShaderResourceView* albedo) {
            auto& resources = ctx.renderer->GetResources();

            Tsukino::Renderer::Material& material = ctx.renderer->GetDrawQueue().AllocMaterial();
            material.SetPipeline(pipeline);
            material.SetSampler(resources.GetSampler(Tsukino::GraphicsCommon::SamplerType::LinearClamp));
            material.SetTexture(Tsukino::Renderer::SRVSlot::Albedo, albedo ? albedo : resources.GetWhiteTextureSRV());
            material.SetTexture(Tsukino::Renderer::SRVSlot::Normal, resources.GetFlatNormalTextureSRV());
            material.SetTexture(Tsukino::Renderer::SRVSlot::MetallicRoughness, resources.GetWhiteTextureSRV());
            material.SetTexture(Tsukino::Renderer::SRVSlot::Emissive, resources.GetWhiteTextureSRV());
            material.SetTexture(Tsukino::Renderer::SRVSlot::AO, resources.GetWhiteTextureSRV());
            return material;
        }

        //-------------------------------------------------------------
        //! @brief  マテリアル定数を今フレームぶん確保する（金属ではない、ざらついた面）
        //-------------------------------------------------------------
        Tsukino::Renderer::CBufferMaterial& AllocTreeMaterialData(Tsukino::EngineIntegration::EngineContext& ctx, float roughness) {
            Tsukino::Renderer::CBufferMaterial& data = ctx.renderer->GetDrawQueue().AllocMaterialData();
            data                                     = Tsukino::Renderer::CBufferMaterial{};
            data.baseColor                           = hlslpp::float4(1.0f, 1.0f, 1.0f, 1.0f);
            data.emissive                            = hlslpp::float3(0.0f, 0.0f, 0.0f);
            data.metallic                            = 0.0f;
            data.roughness                           = roughness;
            data.specular                            = 0.25f;
            return data;
        }
    }    // namespace

    //-------------------------------------------------------------
    //! @brief システムの更新
    //-------------------------------------------------------------
    void TreeFieldSystem::Update(Tsukino::ECS::Registry& registry, float /*deltaTime*/) {
        auto* ctx = registry.GetContext<Tsukino::EngineIntegration::EngineContext*>();
        if(!ctx || !ctx->renderer || !ctx->assetManager || !ctx->builtinAssets || m_buildFailed)
            return;

        //-------------------------------------------------------------
        // TreeFieldComponentを探す（複数あれば最初の有効な1つだけ使う。草と同じ流儀）
        //-------------------------------------------------------------
        const TreeFieldComponent* field = nullptr;
        registry.View<TreeFieldComponent>().each([&](entt::entity, const TreeFieldComponent& candidate) {
            if(!field && candidate.enabled)
                field = &candidate;
        });
        if(!field)
            return;

        //-------------------------------------------------------------
        // 木の形を作る（最初の1回だけ）。形を作るのはCPUで数ミリ秒かかるので、毎フレームはやらない
        //-------------------------------------------------------------
        if(m_variants.empty()) {
            const int variantCount = std::clamp(field->variantCount, 1, 32);
            m_variants.reserve(static_cast<size_t>(variantCount));
            for(int i = 0; i < variantCount; ++i) {
                const TreeMeshes meshes = BuildProceduralTree(MakeShapeParams(*field, i));

                TreeVariant variant;
                variant.trunk       = Tsukino::Renderer::CreateMeshBuffer(ctx->renderer->GetDevice(), meshes.trunk);
                variant.canopy      = Tsukino::Renderer::CreateMeshBuffer(ctx->renderer->GetDevice(), meshes.canopy);
                variant.trunkRadius = meshes.trunkRadius;
                if(variant.trunk.indexCount == 0 || variant.canopy.indexCount == 0) {
                    Tsukino::Core::Log::Error("TreeFieldSystem - failed to create a tree mesh. Trees will not be drawn.");
                    m_variants.clear();
                    m_buildFailed = true;
                    return;
                }
                m_variants.push_back(std::move(variant));
            }
        }

        //-------------------------------------------------------------
        // 中心にするプレイヤーの位置
        //-------------------------------------------------------------
        bool           hasPlayer = false;
        hlslpp::float3 center(0.0f, 0.0f, 0.0f);
        registry.View<PlayerComponent, Tsukino::BuiltIn::ECS::TransformComponent>().each(
            [&](entt::entity, const PlayerComponent&, const Tsukino::BuiltIn::ECS::TransformComponent& transform) {
                if(hasPlayer)
                    return;
                center    = transform.position;
                hasPlayer = true;
            });

        //-------------------------------------------------------------
        // パイプライン。頂点シェーダーは風で揺らす自前のもの（入出力はエンジンのModelStatic.vsと同じ）、
        // ピクセルシェーダーはエンジンのGBuffer.psを借りるので、ライティング・霧・くり抜き・網点フェードが効く。
        // 影は同じ揺れを足す影用の頂点シェーダー＋エンジンの影用ピクセルシェーダーで描き、
        // DrawCommand::shadowPipelineで差し替える（影も本体と一緒に揺れる）
        //-------------------------------------------------------------
        auto loadShader = [&](const char* path) {
            return std::static_pointer_cast<Tsukino::Asset::ShaderAsset>(ctx->assetManager->Get(ctx->assetManager->Load(Tsukino::Core::Path(path))));
        };
        auto vsAsset       = loadShader("CombatAndroid/Assets/Shaders/TreeSway.vs.hlsl");
        auto shadowVsAsset = loadShader("CombatAndroid/Assets/Shaders/TreeSwayShadow.vs.hlsl");
        auto psAsset       = std::static_pointer_cast<Tsukino::Asset::ShaderAsset>(ctx->assetManager->Get(ctx->builtinAssets->shaders.gbufferPS));
        auto shadowPsAsset = std::static_pointer_cast<Tsukino::Asset::ShaderAsset>(ctx->assetManager->Get(ctx->builtinAssets->shaders.shadowPS));
        if(!vsAsset || !shadowVsAsset || !psAsset || !shadowPsAsset)
            return;

        auto* pipelineFactory = ctx->renderer->GetResources().GetPipelineFactory();
        std::shared_ptr<Tsukino::Renderer::PipelineState> pipeline =
            pipelineFactory->Create(*vsAsset, *psAsset, Tsukino::GraphicsCommon::VertexFormat::PositionNormalUV, Tsukino::Renderer::DepthMode::ReadWrite,
                                    Tsukino::Renderer::BlendMode::Opaque);
        std::shared_ptr<Tsukino::Renderer::PipelineState> shadowPipeline = pipelineFactory->Create(
            *shadowVsAsset, *shadowPsAsset, Tsukino::GraphicsCommon::VertexFormat::PositionNormalUV, Tsukino::Renderer::DepthMode::ReadWrite);
        if(!pipeline || !shadowPipeline)
            return;

        //-------------------------------------------------------------
        // 揺れのパラメータ。風向きと突風は、草があれば草の値に揃える（草・霧・木が同じ風になびく）
        //-------------------------------------------------------------
        hlslpp::float3 windDirection  = field->windDirection;
        float          gustWavelength = field->gustWavelength;
        float          gustSpeed      = field->gustSpeed;
        float          gustStrength   = field->gustStrength;
        bool           foundGrass     = false;
        registry.View<GrassFieldComponent>().each([&](entt::entity, const GrassFieldComponent& grass) {
            if(foundGrass || !grass.enabled)
                return;
            windDirection  = grass.windDirection;
            gustWavelength = grass.gustWavelength;
            gustSpeed      = grass.gustSpeed;
            gustStrength   = grass.gustStrength;
            foundGrass     = true;
        });

        float windX = windDirection.x, windZ = windDirection.z;
        const float windLength = std::sqrt(windX * windX + windZ * windZ);
        if(windLength > 1.0e-4f) {
            windX /= windLength;
            windZ /= windLength;
        } else {
            windX = 1.0f;
            windZ = 0.0f;
        }

        auto updateSwayBuffer = [&](Tsukino::Renderer::UserConstantBuffer& buffer, float flutterStrength) {
            if(!buffer.IsValid())
                buffer = Tsukino::Renderer::CreateUserConstantBuffer(ctx->renderer->GetDevice(), sizeof(CBufferTree));
            if(!buffer.IsValid())
                return false;

            CBufferTree params;
            params.windParams = hlslpp::float4(windX, 0.0f, windZ, 0.0f);
            params.swayParams = hlslpp::float4(field->swayStrength, field->swaySpeed, flutterStrength, field->leafFlutterSpeed);
            params.gustParams = hlslpp::float4(gustWavelength, gustSpeed, gustStrength, std::max(field->swayHeightRef, 1.0f));
            Tsukino::Renderer::UpdateUserConstantBuffer(ctx->renderer->GetContext(), buffer, &params, sizeof(params));
            return true;
        };
        // 幹は木全体のしなりだけ、葉はさらに細かくそよがせる
        if(!updateSwayBuffer(m_trunkSwayBuffer, 0.0f) || !updateSwayBuffer(m_leafSwayBuffer, field->leafFlutterStrength)) {
            Tsukino::Core::Log::Error("TreeFieldSystem - failed to create the tree sway constant buffer. Trees will not be drawn.");
            m_buildFailed = true;
            return;
        }

        Tsukino::Renderer::Material& barkMaterial =
            AllocTreeMaterial(*ctx, pipeline.get(), GetGradientSRV(*ctx, "bark", field->barkBottomColor, field->barkTopColor));
        Tsukino::Renderer::Material& leafMaterial = AllocTreeMaterial(*ctx, pipeline.get(), GetLeafAtlasSRV(*ctx, *field));
        Tsukino::Renderer::CBufferMaterial& barkData = AllocTreeMaterialData(*ctx, 0.9f);
        Tsukino::Renderer::CBufferMaterial& leafData = AllocTreeMaterialData(*ctx, 0.75f);
        leafData.alphaCutoff                         = 0.5f;    // 葉テクスチャのアルファで葉の形にくり抜く（影もエンジンが同じ形で抜く）

        //-------------------------------------------------------------
        // プレイヤーの周りの升目を調べて、木を置く。
        // 升目の座標から決まる乱数で「生えるか・升目の中の位置・形・向き・大きさ」を決めるので、
        // 同じ升目には何度来ても同じ木が立っている
        //-------------------------------------------------------------
        const float cellSize       = std::max(field->cellSize, 50.0f);
        const float drawRadius     = std::max(field->drawRadius, 0.0f);
        const float colliderRadius = std::min(std::max(field->colliderRadius, 0.0f), drawRadius);
        const int   minCellX       = static_cast<int>(std::floor((center.x - drawRadius) / cellSize));
        const int   maxCellX       = static_cast<int>(std::floor((center.x + drawRadius) / cellSize));
        const int   minCellZ       = static_cast<int>(std::floor((center.z - drawRadius) / cellSize));
        const int   maxCellZ       = static_cast<int>(std::floor((center.z + drawRadius) / cellSize));

        m_wantedColliders.clear();

        for(int cellZ = minCellZ; cellZ <= maxCellZ; ++cellZ) {
            for(int cellX = minCellX; cellX <= maxCellX; ++cellX) {
                CellRandom random(cellX, cellZ, field->seed);
                if(random.Next01() >= field->spawnChance)
                    continue;

                // 升目の中の位置（中心から cellJitter の幅でずらす。升目の縁で隣の木とくっつかないよう1未満にしておく）
                const float jitter = std::clamp(field->cellJitter, 0.0f, 1.0f);
                const float x      = (static_cast<float>(cellX) + 0.5f + (random.Next01() - 0.5f) * jitter) * cellSize;
                const float z      = (static_cast<float>(cellZ) + 0.5f + (random.Next01() - 0.5f) * jitter) * cellSize;

                // 走行の開始地点の周りは空ける
                if(x * x + z * z < field->startClearRadius * field->startClearRadius)
                    continue;

                const float dx       = x - static_cast<float>(center.x);
                const float dz       = z - static_cast<float>(center.z);
                const float distance = std::sqrt(dx * dx + dz * dz);
                if(distance > drawRadius)
                    continue;

                const size_t       variantIndex = std::min(static_cast<size_t>(random.Next01() * static_cast<float>(m_variants.size())), m_variants.size() - 1);
                const TreeVariant& variant      = m_variants[variantIndex];
                const float        yaw          = random.Next01() * kTwoPi;
                const float        scale        = field->scaleMin + (field->scaleMax - field->scaleMin) * random.Next01();

                //-------------------------------------------------------------
                // 描画範囲の端の帯では、網点で少しずつ現す（DrawCommand::ditherFade）。
                // 範囲に入った瞬間に木が丸ごと出てくると、瞬間移動したように見えるため。
                // 帯は霧で霞む距離に置いてあるので、網点の粗さも目立ちにくい
                //-------------------------------------------------------------
                const float fadeBand = std::clamp(field->fadeBand, 1.0f, std::max(drawRadius, 1.0f));
                const float fadeT    = std::clamp((drawRadius - distance) / fadeBand, 0.0f, 1.0f);
                const float fade     = fadeT * fadeT * (3.0f - 2.0f * fadeT);
                if(fade <= 0.0f)
                    continue;

                // ワールド行列（拡大→回転→移動。エンジンのTransformSystemと同じ掛け順）
                const Tsukino::Core::Math::matrix world = hlslpp::mul(Tsukino::Core::Math::matrix::scale(scale),
                                                                      hlslpp::mul(Tsukino::Core::Math::matrix::rotateY(yaw),
                                                                                  Tsukino::Core::Math::matrix::translate(x, 0.0f, z)));

                //-------------------------------------------------------------
                // 幹と葉を1本ずつ積む。木は動かないので前フレームの行列も同じ
                // （カメラが動いたぶんのモーションブラーはこれで正しく出る）
                //-------------------------------------------------------------
                auto push = [&](Tsukino::Renderer::MeshBuffer& mesh, Tsukino::Renderer::Material& material, Tsukino::Renderer::CBufferMaterial& data,
                                Tsukino::Renderer::UserConstantBuffer& sway) {
                    Tsukino::Renderer::DrawCommand cmd{};
                    cmd.mesh          = &mesh;
                    cmd.material      = &material;
                    cmd.materialData  = &data;
                    cmd.transform     = world;
                    cmd.prevTransform = world;
                    cmd.hasPrevFrame  = true;
                    cmd.pass          = Tsukino::Renderer::RenderPass::GBuffer;
                    cmd.castsShadow   = true;
                    cmd.ditherFade    = fade;
                    // 揺れのパラメータ（本体と影の両方の頂点シェーダーが b12 から読む）と、同じ揺れで描く影用パイプライン
                    cmd.userConstantBuffer = sway.buffer.Get();
                    cmd.userConstantSlot   = Tsukino::Renderer::CBSlot::User0;
                    cmd.shadowPipeline     = shadowPipeline.get();
                    ctx->renderer->GetDrawQueue().Push(cmd);
                };
                push(m_variants[variantIndex].trunk, barkMaterial, barkData, m_trunkSwayBuffer);
                push(m_variants[variantIndex].canopy, leafMaterial, leafData, m_leafSwayBuffer);

                //-------------------------------------------------------------
                // 当たり判定。近くの木にだけ、幹のカプセルを持つエンティティを作る
                // （物理の静的ボディはPhysicsSystemがCollisionComponentを見て作り、エンティティを消せば外す）
                //-------------------------------------------------------------
                if(!hasPlayer || distance > colliderRadius)
                    continue;

                const std::uint64_t key = MakeCellKey(cellX, cellZ);
                m_wantedColliders.insert(key);
                if(m_colliders.contains(key))
                    continue;

                const float radius     = variant.trunkRadius * field->colliderRadiusScale * scale;
                const float halfHeight = std::max(field->colliderHeight * scale * 0.5f - radius, 1.0f);

                Tsukino::ECS::Entity collider = registry.CreateEntity();
                auto& transform               = registry.AddComponent<Tsukino::BuiltIn::ECS::TransformComponent>(collider);
                transform.position            = hlslpp::float3(x, halfHeight + radius, z);    // カプセルの下端を地面に付ける
                transform.dirty               = true;

                auto& collision  = registry.AddComponent<Tsukino::BuiltIn::ECS::CollisionComponent>(collider);
                collision.type   = Tsukino::BuiltIn::ECS::ColliderType::Capsule;
                collision.extent = hlslpp::float3(radius, halfHeight, 0.0f);    // x: 半径, y: 半分の高さ

                m_colliders.emplace(key, collider);
            }
        }

        //-------------------------------------------------------------
        // 離れた木の当たり判定を消す
        //-------------------------------------------------------------
        for(auto it = m_colliders.begin(); it != m_colliders.end();) {
            if(m_wantedColliders.contains(it->first)) {
                ++it;
                continue;
            }
            if(registry.IsValid(it->second))
                registry.QueueDestroy(it->second);
            it = m_colliders.erase(it);
        }
    }
}    // namespace CombatAndroid::ECS
