//-------------------------------------------------------------
//! @file   TreeFieldSystem.cpp
//! @brief  TreeFieldSystemクラスの実装
//-------------------------------------------------------------
#include <CombatAndroid/ECS/System/World/TreeFieldSystem.hpp>
#include <CombatAndroid/ECS/Component/World/TreeFieldComponent.hpp>
#include <CombatAndroid/ECS/Component/World/GrassFieldComponent.hpp>
#include <CombatAndroid/ECS/Component/World/TreeColliderComponent.hpp>
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

#include <Tsukino/GraphicsCommon/Mesh/MeshData.hpp>
#include <Tsukino/GraphicsCommon/Vertex/VertexPNUV.hpp>
#include <Tsukino/GraphicsCommon/Vertex/VertexFormat.hpp>

#include <Tsukino/Core/ECS/Event/EventBus.hpp>
#include <Tsukino/Core/Log.hpp>
#include <Tsukino/Core/Math/Spring.hpp>
#include <Tsukino/Core/Math/Matrix.hpp>
#include <Tsukino/Core/Path.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
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
        //! @brief  葉カードに貼る葉テクスチャ（2×2のアトラス）を得る（同じ色・種・枚数なら作り直さず使い回す）
        //! @param  context       [in,out] エンジンコンテキスト
        //! @param  field         [in]     色と種を持つ調整値
        //! @param  leavesPerTile [in]     1マスに描く葉の数（茂みの葉カードは数枚、落ちる葉は1枚）
        //! @return テクスチャのSRV。作れなければnullptr
        //! @note   葉の外はアルファ0。マテリアルのalphaCutoffでくり抜くので、影も葉の形になる
        //-------------------------------------------------------------
        ID3D11ShaderResourceView* GetLeafAtlasSRV(Tsukino::EngineIntegration::EngineContext& context, const TreeFieldComponent& field,
                                                  int leavesPerTile) {
            if(!context.assetManager || !context.renderer)
                return nullptr;

            LeafAtlasParams params;
            params.seed          = field.seed;
            params.leavesPerTile = leavesPerTile;
            const float dark[3]  = {field.leafBottomColor.x, field.leafBottomColor.y, field.leafBottomColor.z};
            const float light[3] = {field.leafTopColor.x, field.leafTopColor.y, field.leafTopColor.z};
            for(int k = 0; k < 3; ++k) {
                params.darkColor[k]  = std::clamp(dark[k], 0.0f, 1.0f);
                params.lightColor[k] = std::clamp(light[k], 0.0f, 1.0f);
            }

            auto        toByte = [](float v) { return static_cast<int>(v * 255.0f + 0.5f); };
            std::string key    = "procedural|tree|leafatlas|" + std::to_string(params.seed) + "|" + std::to_string(leavesPerTile);
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

        //-------------------------------------------------------------
        //! @struct FallingLeafInstance
        //! @brief  落ちる葉1枚ぶんのGPUへ渡すデータ（FallingLeaf.vs.hlsl の LeafInstance と一致させること。32バイト）
        //-------------------------------------------------------------
        struct FallingLeafInstance {
            hlslpp::float4 positionSize;    //!< xyz: ワールド座標, w: 一辺の長さ
            hlslpp::float4 rotation;        //!< x: ヨー, y: ピッチ, z: ロール, w: 葉の絵のマス
        };
        static_assert(sizeof(FallingLeafInstance) == 32, "FallingLeaf.vs.hlsl の LeafInstance と大きさを揃えること");

        //! 地面に着いた葉が消えるときに縮む時間（秒）
        constexpr float kLeafShrinkTime = 0.6f;
    }    // namespace

    //-------------------------------------------------------------
    //! @brief 升目の木を求める
    //-------------------------------------------------------------
    std::optional<TreeFieldSystem::TreeInstance> TreeFieldSystem::ResolveTree(const TreeFieldComponent& field, int cellX, int cellZ) const {
        if(m_variants.empty())
            return std::nullopt;

        const float cellSize = std::max(field.cellSize, 50.0f);
        CellRandom  random(cellX, cellZ, field.seed);
        if(random.Next01() >= field.spawnChance)
            return std::nullopt;

        // 升目の中の位置（中心から cellJitter の幅でずらす。升目の縁で隣の木とくっつかないよう1未満にしておく）
        const float  jitter = std::clamp(field.cellJitter, 0.0f, 1.0f);
        TreeInstance tree;
        tree.x = (static_cast<float>(cellX) + 0.5f + (random.Next01() - 0.5f) * jitter) * cellSize;
        tree.z = (static_cast<float>(cellZ) + 0.5f + (random.Next01() - 0.5f) * jitter) * cellSize;

        // 走行の開始地点の周りは空ける
        if(tree.x * tree.x + tree.z * tree.z < field.startClearRadius * field.startClearRadius)
            return std::nullopt;

        tree.variant = std::min(static_cast<size_t>(random.Next01() * static_cast<float>(m_variants.size())), m_variants.size() - 1);
        tree.yaw     = random.Next01() * kTwoPi;
        tree.scale   = field.scaleMin + (field.scaleMax - field.scaleMin) * random.Next01();
        return tree;
    }

    //-------------------------------------------------------------
    //! @brief 木に攻撃を当てる（揺らして、葉を落とす）
    //-------------------------------------------------------------
    void TreeFieldSystem::HitTree(const TreeFieldComponent& field, std::uint64_t cellKey, const TreeInstance& tree, const hlslpp::float3& direction,
                                  float impulse, float strength) {
        //-------------------------------------------------------------
        // しなりのばねへ、攻撃の向き（水平）に初速を与える。揺れている途中ならその上に足す
        //-------------------------------------------------------------
        float       dirX      = direction.x;
        float       dirZ      = direction.z;
        const float dirLength = std::sqrt(dirX * dirX + dirZ * dirZ);
        if(dirLength > 1.0e-4f) {
            dirX /= dirLength;
            dirZ /= dirLength;
        } else {
            dirX = 1.0f;
            dirZ = 0.0f;
        }

        TreeShake& shake = m_shakes[cellKey];
        shake.x          = tree.x;
        shake.z          = tree.z;
        shake.velocity   = shake.velocity + hlslpp::float3(dirX, 0.0f, dirZ) * (impulse * strength);

        //-------------------------------------------------------------
        // 葉を落とす。茂みの高さと広がりの中からランダムに出し、外向きと攻撃の向きへ少し弾ませる
        //-------------------------------------------------------------
        const TreeVariant& variant   = m_variants[tree.variant];
        const int          count     = static_cast<int>(std::round(static_cast<float>(std::max(field.leavesPerHit, 0)) * strength));
        const size_t       maxLeaves = static_cast<size_t>(std::max(field.maxFallingLeaves, 0));

        // xorshift。葉の散らばりは見た目だけなので、軽い乱数で十分
        auto random01 = [this]() {
            m_leafRandom ^= m_leafRandom << 13;
            m_leafRandom ^= m_leafRandom >> 17;
            m_leafRandom ^= m_leafRandom << 5;
            return static_cast<float>(m_leafRandom >> 8) / static_cast<float>(1u << 24);
        };

        for(int i = 0; i < count && maxLeaves > 0; ++i) {
            // 上限を超えたら古い葉から消す
            if(m_leaves.size() >= maxLeaves)
                m_leaves.erase(m_leaves.begin());

            const float angle  = random01() * kTwoPi;
            const float radius = variant.canopyRadius * tree.scale * std::sqrt(random01());
            const float height = variant.height * tree.scale * (0.55f + 0.4f * random01());

            FallingLeaf leaf;
            leaf.position   = hlslpp::float3(tree.x + std::cos(angle) * radius, height, tree.z + std::sin(angle) * radius);
            leaf.velocity   = hlslpp::float3(std::cos(angle) * 40.0f + dirX * 60.0f, 20.0f + random01() * 50.0f, std::sin(angle) * 40.0f + dirZ * 60.0f)
                            * (0.5f + 0.5f * strength);
            leaf.yaw        = random01() * kTwoPi;
            leaf.pitch      = random01() * kTwoPi;
            leaf.roll       = random01() * kTwoPi;
            leaf.yawSpeed   = (random01() - 0.5f) * 4.0f;
            leaf.pitchSpeed = (random01() - 0.5f) * 7.0f;
            leaf.rollSpeed  = (random01() - 0.5f) * 5.0f;
            leaf.size       = field.fallingLeafSize * (0.75f + 0.5f * random01());
            leaf.tile       = std::floor(random01() * 3.999f);
            leaf.swayPhase  = random01() * kTwoPi;
            m_leaves.push_back(leaf);
        }
    }

    //-------------------------------------------------------------
    //! @brief 攻撃の揺れと落ちる葉を進める
    //-------------------------------------------------------------
    void TreeFieldSystem::UpdateReactions(const TreeFieldComponent& field, float deltaTime, float windX, float windZ) {
        if(deltaTime <= 0.0f)
            return;

        //-------------------------------------------------------------
        // しなりのばね。エンジンの減衰ばね（解析解）で戻すので、フレームレートが変わっても同じ揺れになる。
        // 収まった木は外す
        //-------------------------------------------------------------
        for(auto it = m_shakes.begin(); it != m_shakes.end();) {
            TreeShake& shake = it->second;
            Tsukino::Core::Math::Spring::Step(shake.offset, shake.velocity, hlslpp::float3(0.0f, 0.0f, 0.0f), field.shakeFrequency, field.shakeDamping,
                                              deltaTime);
            shake.offset.y   = 0.0f;
            shake.velocity.y = 0.0f;

            if(static_cast<float>(hlslpp::length(shake.offset)) < 0.05f && static_cast<float>(hlslpp::length(shake.velocity)) < 0.5f)
                it = m_shakes.erase(it);
            else
                ++it;
        }

        //-------------------------------------------------------------
        // 落ちる葉。重力で落ちるが、空気抵抗で落ちる速さに上限があり、ゆらゆら横へ流れながら回る。
        // 地面に着いたら寝かせて止め、しばらくしたら縮めて消す（縮めるのはDrawFallingLeaves）
        //-------------------------------------------------------------
        constexpr float kGravity       = 260.0f;    // 葉は軽いので、物の落下より弱い重力にしておく
        constexpr float kAirDrag       = 2.5f;      // 横の速さが風の流れへ馴染む速さ
        constexpr float kWindDrift     = 18.0f;     // 風に流される速さ
        constexpr float kSwayAmplitude = 55.0f;     // ゆらゆら流れる横の速さ
        constexpr float kSwayRate      = 3.2f;      // ゆらゆらの速さ（ラジアン/秒）
        constexpr float kGroundLift    = 0.6f;      // 地面に着いた葉を浮かせる量（地面とちらつかないように）

        const float fallSpeed = std::max(field.leafFallSpeed, 1.0f);

        for(FallingLeaf& leaf : m_leaves) {
            leaf.age += deltaTime;

            if(leaf.restTimer >= 0.0f) {
                leaf.restTimer += deltaTime;
                continue;
            }

            // 縦：重力で加速し、上限の速さで頭打ち
            leaf.velocity.y = std::max(static_cast<float>(leaf.velocity.y) - kGravity * deltaTime, -fallSpeed);

            // 横：風の流れ＋ゆらゆら（落ち葉が左右に振れながら落ちる動き）へ馴染ませる
            const float sway    = std::sin(leaf.age * kSwayRate + leaf.swayPhase);
            const float targetX = windX * kWindDrift + std::cos(leaf.swayPhase) * sway * kSwayAmplitude;
            const float targetZ = windZ * kWindDrift + std::sin(leaf.swayPhase) * sway * kSwayAmplitude;
            const float blend   = std::min(kAirDrag * deltaTime, 1.0f);
            leaf.velocity.x     = leaf.velocity.x + (targetX - leaf.velocity.x) * blend;
            leaf.velocity.z     = leaf.velocity.z + (targetZ - leaf.velocity.z) * blend;

            leaf.position = leaf.position + leaf.velocity * deltaTime;
            leaf.yaw += leaf.yawSpeed * deltaTime;
            leaf.pitch += leaf.pitchSpeed * deltaTime;
            leaf.roll += leaf.rollSpeed * deltaTime;

            // 地面に着いたら寝かせる（ピッチ90度で面が上を向く）。重なった葉がちらつかないよう高さを少しずつずらす
            if(leaf.position.y <= kGroundLift) {
                leaf.position.y = kGroundLift + 0.05f * leaf.swayPhase;
                leaf.velocity   = hlslpp::float3(0.0f, 0.0f, 0.0f);
                leaf.pitch      = kTwoPi * 0.25f;
                leaf.roll       = 0.0f;
                leaf.restTimer  = 0.0f;
            }
        }

        const float restTime = std::max(field.leafRestTime, 0.0f);
        m_leaves.erase(std::remove_if(m_leaves.begin(), m_leaves.end(),
                                      [&](const FallingLeaf& leaf) { return leaf.restTimer >= restTime + kLeafShrinkTime || leaf.age > 60.0f; }),
                       m_leaves.end());
    }

    //-------------------------------------------------------------
    //! @brief 落ちる葉を描く
    //-------------------------------------------------------------
    void TreeFieldSystem::DrawFallingLeaves(Tsukino::ECS::Registry& registry, const TreeFieldComponent& field) {
        auto* ctx = registry.GetContext<Tsukino::EngineIntegration::EngineContext*>();
        if(!ctx || m_leaves.empty())
            return;

        //-------------------------------------------------------------
        // 四角形のメッシュ（-0.5〜0.5、XY平面、法線+Z）と、葉ごとのデータのバッファを初回に作る
        //-------------------------------------------------------------
        if(m_leafQuad.indexCount == 0) {
            const Tsukino::GraphicsCommon::VertexPNUV corners[4] = {
                {{-0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
                {{0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
                {{0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}},
                {{-0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}},
            };
            Tsukino::GraphicsCommon::MeshData quad;
            quad.format       = Tsukino::GraphicsCommon::VertexFormat::PositionNormalUV;
            quad.vertexStride = sizeof(Tsukino::GraphicsCommon::VertexPNUV);
            quad.vertexCount  = 4;
            quad.vertexData.resize(sizeof(corners));
            std::memcpy(quad.vertexData.data(), corners, sizeof(corners));
            quad.indices    = {0, 1, 2, 0, 2, 3};
            quad.indexCount = 6;
            m_leafQuad      = Tsukino::Renderer::CreateMeshBuffer(ctx->renderer->GetDevice(), quad);
        }
        const Tsukino::u32 capacity = static_cast<Tsukino::u32>(std::max(field.maxFallingLeaves, 1));
        if(!m_leafInstances.IsValid() || m_leafInstances.capacity < capacity)
            m_leafInstances = Tsukino::Renderer::CreateInstanceBuffer(ctx->renderer->GetDevice(), sizeof(FallingLeafInstance), capacity);
        if(m_leafQuad.indexCount == 0 || !m_leafInstances.IsValid())
            return;

        //-------------------------------------------------------------
        // 葉ごとのデータを書く。地面で寝ている葉は、消える前の kLeafShrinkTime 秒で縮める
        //-------------------------------------------------------------
        const float                      restTime = std::max(field.leafRestTime, 0.0f);
        std::vector<FallingLeafInstance> instances;
        instances.reserve(m_leaves.size());
        for(const FallingLeaf& leaf : m_leaves) {
            float size = leaf.size;
            if(leaf.restTimer > restTime)
                size *= std::clamp(1.0f - (leaf.restTimer - restTime) / kLeafShrinkTime, 0.0f, 1.0f);

            FallingLeafInstance instance;
            instance.positionSize = hlslpp::float4(leaf.position.x, leaf.position.y, leaf.position.z, size);
            instance.rotation     = hlslpp::float4(leaf.yaw, leaf.pitch, leaf.roll, leaf.tile);
            instances.push_back(instance);
        }
        const Tsukino::u32 written = Tsukino::Renderer::UpdateInstanceBuffer(ctx->renderer->GetContext(), m_leafInstances, instances.data(),
                                                                            static_cast<Tsukino::u32>(instances.size()));
        if(written == 0)
            return;

        //-------------------------------------------------------------
        // パイプラインとマテリアル。ピクセルシェーダーはエンジンのGBuffer.ps（葉の形はalphaCutoffでくり抜く）
        //-------------------------------------------------------------
        auto vsAsset = std::static_pointer_cast<Tsukino::Asset::ShaderAsset>(
            ctx->assetManager->Get(ctx->assetManager->Load(Tsukino::Core::Path("CombatAndroid/Assets/Shaders/FallingLeaf.vs.hlsl"))));
        auto psAsset = std::static_pointer_cast<Tsukino::Asset::ShaderAsset>(ctx->assetManager->Get(ctx->builtinAssets->shaders.gbufferPS));
        if(!vsAsset || !psAsset)
            return;
        std::shared_ptr<Tsukino::Renderer::PipelineState> pipeline = ctx->renderer->GetResources().GetPipelineFactory()->Create(
            *vsAsset, *psAsset, Tsukino::GraphicsCommon::VertexFormat::PositionNormalUV, Tsukino::Renderer::DepthMode::ReadWrite,
            Tsukino::Renderer::BlendMode::Opaque);
        if(!pipeline)
            return;

        // 落ちる葉は1マスに1枚の葉の絵（茂みの葉カードとは別のテクスチャ）
        Tsukino::Renderer::Material&        material = AllocTreeMaterial(*ctx, pipeline.get(), GetLeafAtlasSRV(*ctx, field, 1));
        Tsukino::Renderer::CBufferMaterial& data     = AllocTreeMaterialData(*ctx, 0.75f);
        data.alphaCutoff                             = 0.5f;

        Tsukino::Renderer::DrawCommand cmd{};
        cmd.mesh          = &m_leafQuad;
        cmd.material      = &material;
        cmd.materialData  = &data;
        cmd.transform     = Tsukino::Core::Math::matrix::identity();    // 位置は頂点シェーダーが葉ごとのデータから組み立てる
        cmd.prevTransform = cmd.transform;
        cmd.hasPrevFrame  = false;
        cmd.pass          = Tsukino::Renderer::RenderPass::GBuffer;
        cmd.instanceCount = written;
        cmd.instanceData  = m_leafInstances.srv.Get();
        cmd.castsShadow   = false;    // 小さく速く動く葉の影は見分けられないので落とさない（影用の頂点シェーダーも要らない）
        ctx->renderer->GetDrawQueue().Push(cmd);
    }

    //-------------------------------------------------------------
    //! @brief システムの更新
    //-------------------------------------------------------------
    void TreeFieldSystem::Update(Tsukino::ECS::Registry& registry, float deltaTime) {
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
        // 攻撃の通知の購読（最初の更新で張る。草の衝撃波と同じ流儀で、シーン側の配線は要らない）。
        // ハンドラは溜めるだけにして、木を揺らすのはこの更新の中で行う
        //-------------------------------------------------------------
        if(!m_subscribed) {
            if(auto* eventBus = registry.GetContext<Tsukino::ECS::EventBus*>()) {
                m_hitConnection  = eventBus->Subscribe<TreeHitEvent>([this](const TreeHitEvent& event) { m_pendingHits.push_back(event); });
                m_waveConnection = eventBus->Subscribe<GrassWaveEvent>([this](const GrassWaveEvent& event) { m_pendingWaves.push_back(event); });
                m_subscribed     = true;
            }
        }

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
                variant.height      = meshes.height;
                // 葉の茂みの水平の広がり。箱の半分より少し内側にする（茂みは丸いので、箱の角には葉が無い）
                variant.canopyRadius = std::max(static_cast<float>(meshes.canopy.bounds.max.x - meshes.canopy.bounds.min.x),
                                                static_cast<float>(meshes.canopy.bounds.max.z - meshes.canopy.bounds.min.z))
                                       * 0.35f;
                if(variant.trunk.indexCount == 0 || variant.canopy.indexCount == 0) {
                    Tsukino::Core::Log::Error("TreeFieldSystem - failed to create a tree mesh. Trees will not be drawn.");
                    m_variants.clear();
                    m_buildFailed = true;
                    return;
                }
                m_variants.push_back(std::move(variant));
            }
        }

        const float cellSize = std::max(field->cellSize, 50.0f);

        //-------------------------------------------------------------
        // 攻撃への反応。武器・斬撃弾が幹に当たった木（升目のキーで分かる）と、
        // 範囲攻撃の衝撃波が届いた木（半径内の升目を調べる）を揺らし、葉を落とす
        //-------------------------------------------------------------
        for(const TreeHitEvent& hit : m_pendingHits) {
            const int cellX = static_cast<int>(static_cast<std::int32_t>(hit.cellKey >> 32));
            const int cellZ = static_cast<int>(static_cast<std::int32_t>(hit.cellKey & 0xffffffffu));
            if(const std::optional<TreeInstance> tree = ResolveTree(*field, cellX, cellZ))
                HitTree(*field, hit.cellKey, *tree, hit.direction, field->hitShakeImpulse, hit.strength);
        }
        m_pendingHits.clear();

        for(const GrassWaveEvent& wave : m_pendingWaves) {
            const float radius = std::max(wave.radius, 0.0f);
            const int   minX   = static_cast<int>(std::floor((wave.center.x - radius) / cellSize));
            const int   maxX   = static_cast<int>(std::floor((wave.center.x + radius) / cellSize));
            const int   minZ   = static_cast<int>(std::floor((wave.center.z - radius) / cellSize));
            const int   maxZ   = static_cast<int>(std::floor((wave.center.z + radius) / cellSize));
            for(int cellZ = minZ; cellZ <= maxZ; ++cellZ) {
                for(int cellX = minX; cellX <= maxX; ++cellX) {
                    const std::optional<TreeInstance> tree = ResolveTree(*field, cellX, cellZ);
                    if(!tree || radius <= 0.0f)
                        continue;
                    const float dx       = tree->x - static_cast<float>(wave.center.x);
                    const float dz       = tree->z - static_cast<float>(wave.center.z);
                    const float distance = std::sqrt(dx * dx + dz * dz);
                    if(distance > radius)
                        continue;
                    // 中心から外向きに倒し、縁へ行くほど弱める
                    HitTree(*field, MakeCellKey(cellX, cellZ), *tree, hlslpp::float3(dx, 0.0f, dz), field->waveShakeImpulse, 1.0f - distance / radius);
                }
            }
        }
        m_pendingWaves.clear();

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

        // 攻撃の揺れと落ちる葉を進める（ゲーム内時間。メニュー中は止まり、ヒットストップ・スロー中は遅くなる）
        UpdateReactions(*field, deltaTime, windX, windZ);

        //-------------------------------------------------------------
        // 攻撃で揺れている木を、揺れの大きい順に kMaxShakingTrees 本までシェーダーへ渡す
        //-------------------------------------------------------------
        std::vector<const TreeShake*> shaking;
        shaking.reserve(m_shakes.size());
        for(const auto& entry : m_shakes)
            shaking.push_back(&entry.second);
        const size_t slotCount = std::min<size_t>(shaking.size(), kMaxShakingTrees);
        auto         shakeSize = [](const TreeShake* shake) {
            return static_cast<float>(hlslpp::length(shake->offset)) + static_cast<float>(hlslpp::length(shake->velocity)) * 0.1f;
        };
        std::partial_sort(shaking.begin(), shaking.begin() + static_cast<std::ptrdiff_t>(slotCount), shaking.end(),
                          [&](const TreeShake* a, const TreeShake* b) { return shakeSize(a) > shakeSize(b); });

        auto updateSwayBuffer = [&](Tsukino::Renderer::UserConstantBuffer& buffer, float flutterStrength) {
            if(!buffer.IsValid())
                buffer = Tsukino::Renderer::CreateUserConstantBuffer(ctx->renderer->GetDevice(), sizeof(CBufferTree));
            if(!buffer.IsValid())
                return false;

            CBufferTree params{};
            params.windParams = hlslpp::float4(windX, 0.0f, windZ, 0.0f);
            params.swayParams = hlslpp::float4(field->swayStrength, field->swaySpeed, flutterStrength, field->leafFlutterSpeed);
            params.gustParams = hlslpp::float4(gustWavelength, gustSpeed, gustStrength, std::max(field->swayHeightRef, 1.0f));
            for(size_t i = 0; i < kMaxShakingTrees; ++i) {
                params.shakeSlots[i] = (i < slotCount) ? hlslpp::float4(shaking[i]->x, shaking[i]->z, shaking[i]->offset.x, shaking[i]->offset.z)
                                                       : hlslpp::float4(0.0f, 0.0f, 0.0f, 0.0f);
            }
            params.shakeParams = hlslpp::float4(static_cast<float>(slotCount), field->shakeFlutterBoost, 0.0f, 0.0f);
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
        Tsukino::Renderer::Material& leafMaterial = AllocTreeMaterial(*ctx, pipeline.get(), GetLeafAtlasSRV(*ctx, *field, LeafAtlasParams{}.leavesPerTile));
        Tsukino::Renderer::CBufferMaterial& barkData = AllocTreeMaterialData(*ctx, 0.9f);
        Tsukino::Renderer::CBufferMaterial& leafData = AllocTreeMaterialData(*ctx, 0.75f);
        leafData.alphaCutoff                         = 0.5f;    // 葉テクスチャのアルファで葉の形にくり抜く（影もエンジンが同じ形で抜く）

        //-------------------------------------------------------------
        // プレイヤーの周りの升目を調べて、木を置く。
        // 升目の座標から決まる乱数で「生えるか・升目の中の位置・形・向き・大きさ」を決めるので、
        // 同じ升目には何度来ても同じ木が立っている
        //-------------------------------------------------------------
        const float drawRadius     = std::max(field->drawRadius, 0.0f);
        const float colliderRadius = std::min(std::max(field->colliderRadius, 0.0f), drawRadius);
        const int   minCellX       = static_cast<int>(std::floor((center.x - drawRadius) / cellSize));
        const int   maxCellX       = static_cast<int>(std::floor((center.x + drawRadius) / cellSize));
        const int   minCellZ       = static_cast<int>(std::floor((center.z - drawRadius) / cellSize));
        const int   maxCellZ       = static_cast<int>(std::floor((center.z + drawRadius) / cellSize));

        m_wantedColliders.clear();

        for(int cellZ = minCellZ; cellZ <= maxCellZ; ++cellZ) {
            for(int cellX = minCellX; cellX <= maxCellX; ++cellX) {
                // 升目に立っている木（升目の座標から決まる。生えていない升目と開始地点の周りは無し）
                const std::optional<TreeInstance> tree = ResolveTree(*field, cellX, cellZ);
                if(!tree)
                    continue;
                const float x = tree->x;
                const float z = tree->z;

                const float dx       = x - static_cast<float>(center.x);
                const float dz       = z - static_cast<float>(center.z);
                const float distance = std::sqrt(dx * dx + dz * dz);
                if(distance > drawRadius)
                    continue;

                const size_t       variantIndex = tree->variant;
                const TreeVariant& variant      = m_variants[variantIndex];
                const float        yaw          = tree->yaw;
                const float        scale        = tree->scale;

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

                // 目印。武器や斬撃弾の当たり判定が幹を拾ったとき、どの木か分かって揺らせる（CombatHit::NotifyTreeHit）
                registry.AddComponent<TreeColliderComponent>(collider).cellKey = key;

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

        DrawFallingLeaves(registry, *field);
    }
}    // namespace CombatAndroid::ECS
