//-------------------------------------------------------------
//! @file   WeaponTetherSystem.cpp
//! @brief  WeaponTetherSystemクラスの実装
//-------------------------------------------------------------
#include <CombatAndroid/ECS/System/Weapon/WeaponTetherSystem.hpp>
#include <CombatAndroid/ECS/Serialization/Common/SerializationHelper.hpp>
#include <CombatAndroid/ECS/Utility/Table/TableJson.hpp>
#include <Tsukino/Core/Math/Serialization/HlslppSerialization.hpp>
#include <CombatAndroid/ECS/Component/Weapon/WeaponComponent.hpp>
#include <CombatAndroid/ECS/Component/Player/PlayerComponent.hpp>

#include <Tsukino/EngineIntegration/EngineContext.hpp>
#include <Tsukino/EngineIntegration/ECS/BoneLookup.hpp>

#include <Tsukino/BuiltIn/ECS/Component/CameraComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/NodeWorldMatrixComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/TransformComponent.hpp>
#include <Tsukino/BuiltIn/BuiltInAssets.hpp>

#include <Tsukino/GraphicsCommon/Mesh/MeshData.hpp>
#include <Tsukino/GraphicsCommon/Vertex/VertexPUV.hpp>
#include <Tsukino/GraphicsCommon/Vertex/VertexFormat.hpp>

#include <Tsukino/Engine/Asset/AssetManager.hpp>
#include <Tsukino/Engine/Asset/Shader/ShaderAsset.hpp>
#include <Tsukino/Engine/Asset/Texture/TextureAsset.hpp>
#include <Tsukino/Engine/Asset/Util/AssetHandleGenerator.hpp>

#include <Tsukino/Renderer/Renderer.hpp>
#include <Tsukino/Renderer/DrawCommand.hpp>
#include <Tsukino/Renderer/DX11/Material.hpp>
#include <Tsukino/Renderer/DX11/PipelineFactory.hpp>

#include <Tsukino/Core/Log.hpp>
#include <Tsukino/Core/Math/Matrix.hpp>
#include <Tsukino/Core/Path.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    namespace {
        constexpr int kMaxSegments = 32;    //!< 糸1本の区切りの上限

        //-------------------------------------------------------------
        //! @struct WeaponTetherParams
        //! @brief  光の糸のチューニング値（Assets/Tables/Systems/WeaponTether.json。ここの初期値はJSONにキーが無いときの既定値）
        //-------------------------------------------------------------
        struct WeaponTetherParams {
            //-------------------------------------------------------------
            // 背中の点
            //-------------------------------------------------------------
            std::string    anchorBone   = "mixamorig:Spine2";                      //!< 糸の根元を置くボーン（背中の上の方）
            hlslpp::float3 anchorOffset = hlslpp::float3(0.0f, 10.0f, -22.0f);    //!< ボーンからのずらし（プレイヤーの向きの空間。-Zが背中側）
            float          anchorSize   = 26.0f;                                   //!< 背中の光の点の大きさ
            float          anchorPulse  = 0.25f;                                   //!< 点の大きさの脈打ち（大きさに対する比）
            float          anchorPulseSpeed = 3.0f;                                //!< 脈打ちの角速度（rad/sec）

            //-------------------------------------------------------------
            // 糸の見た目
            //-------------------------------------------------------------
            hlslpp::float3 color         = hlslpp::float3(0.45f, 0.85f, 1.0f);    //!< 色（加算なので明るいほど光って見える）
            float          coreWidth     = 2.2f;                                   //!< 芯の太さ
            float          coreIntensity = 1.6f;                                   //!< 芯の明るさ
            float          glowWidth     = 9.0f;                                   //!< 周りのぼんやりした光の太さ
            float          glowIntensity = 0.35f;                                  //!< 周りの光の明るさ
            float          selectedBoost = 1.5f;                                   //!< 選んでいる武器の糸の明るさ・太さの倍率

            //! 糸の下に敷く暗い帯。加算の光は、霧や空のような明るい背景の上ではほとんど明るさが変わらず消えて見えるので、
            //! 普通の半透明で少し暗い縁取りを先に描き、その上に光を重ねて輪郭を残す
            hlslpp::float3 shadeColor = hlslpp::float3(0.02f, 0.07f, 0.14f);
            float          shadeAlpha = 0.45f;    //!< 暗い帯の濃さ（0で敷かない）
            float          shadeWidth = 6.0f;     //!< 暗い帯の太さ（芯より太く、周りの光より細く）
            int            segments      = 14;                                     //!< 曲線の区切り数

            //-------------------------------------------------------------
            // 糸の形
            //-------------------------------------------------------------
            float sag             = 8.0f;     //!< 何もしなくても下へ垂れる量（糸の中ほどで）
            float lagBend         = 0.12f;    //!< 武器の遅れ（背中の点から見た柄の動きの速さ）を逆向きのしなりへ変える倍率（秒）
            float maxBend         = 45.0f;    //!< しなりの上限
            float bendSmoothing   = 10.0f;    //!< しなりの追従の速さ（大きいほど素早く）
            float flutter         = 3.0f;     //!< ゆらゆら揺れる量
            float flutterSpeed    = 2.4f;     //!< 揺れの角速度（rad/sec）
            float growSeconds     = 0.3f;     //!< 拾ったとき、背中から柄まで糸が伸びきる秒数
        };

        template <class Archive>
        void load(Archive& archive, WeaponTetherParams& params) {
            LoadField(archive, "anchorBone", params.anchorBone);
            LoadField(archive, "anchorOffset", params.anchorOffset);
            LoadField(archive, "anchorSize", params.anchorSize);
            LoadField(archive, "anchorPulse", params.anchorPulse);
            LoadField(archive, "anchorPulseSpeed", params.anchorPulseSpeed);
            LoadField(archive, "color", params.color);
            LoadField(archive, "coreWidth", params.coreWidth);
            LoadField(archive, "coreIntensity", params.coreIntensity);
            LoadField(archive, "glowWidth", params.glowWidth);
            LoadField(archive, "glowIntensity", params.glowIntensity);
            LoadField(archive, "selectedBoost", params.selectedBoost);
            LoadField(archive, "shadeColor", params.shadeColor);
            LoadField(archive, "shadeAlpha", params.shadeAlpha);
            LoadField(archive, "shadeWidth", params.shadeWidth);
            LoadField(archive, "segments", params.segments);
            LoadField(archive, "sag", params.sag);
            LoadField(archive, "lagBend", params.lagBend);
            LoadField(archive, "maxBend", params.maxBend);
            LoadField(archive, "bendSmoothing", params.bendSmoothing);
            LoadField(archive, "flutter", params.flutter);
            LoadField(archive, "flutterSpeed", params.flutterSpeed);
            LoadField(archive, "growSeconds", params.growSeconds);
        }

        //-------------------------------------------------------------
        //! @brief  チューニング値を得る関数（初回の呼び出しで読む。Debugでは保存し直すと読み直す）
        //-------------------------------------------------------------
        const WeaponTetherParams& GetParams() {
            return GetSystemParams<WeaponTetherParams>("WeaponTether");
        }

        //-------------------------------------------------------------
        //! @brief  幅1・長さ1の板のメッシュデータを作る（手前の辺の中央が原点、+Zへ伸びる。Uは横0〜1、Vは長さ0〜1）
        //! @note   表裏両方の三角形を入れる（どちらから見ても描かれるように）
        //-------------------------------------------------------------
        Tsukino::GraphicsCommon::MeshData BuildStripMeshData() {
            Tsukino::GraphicsCommon::MeshData mesh;
            mesh.format       = Tsukino::GraphicsCommon::VertexFormat::PositionUV;
            mesh.vertexStride = sizeof(Tsukino::GraphicsCommon::VertexPUV);

            const std::vector<Tsukino::GraphicsCommon::VertexPUV> vertices = {
                {{-0.5f, 0.0f, 0.0f}, {0.0f, 0.0f}},
                {{0.5f, 0.0f, 0.0f}, {1.0f, 0.0f}},
                {{0.5f, 0.0f, 1.0f}, {1.0f, 1.0f}},
                {{-0.5f, 0.0f, 1.0f}, {0.0f, 1.0f}},
            };
            mesh.indices = {0u, 1u, 2u, 0u, 2u, 3u, 0u, 2u, 1u, 0u, 3u, 2u};

            mesh.vertexCount = static_cast<Tsukino::u32>(vertices.size());
            mesh.vertexData.resize(vertices.size() * sizeof(Tsukino::GraphicsCommon::VertexPUV));
            std::memcpy(mesh.vertexData.data(), vertices.data(), mesh.vertexData.size());
            mesh.indexCount = static_cast<Tsukino::u32>(mesh.indices.size());
            return mesh;
        }

        //-------------------------------------------------------------
        //! @brief  縁をぼかしたテクスチャを得る（初回だけ作り、以後は使い回す）
        //! @param  round [in] trueなら丸い光の点（背中の点）、falseなら横方向だけぼかした帯（糸）
        //! @note   中央が白く、縁へ向けて黒・透明へ落ちる。加算合成なので色と透明度の両方を落としておく
        //-------------------------------------------------------------
        ID3D11ShaderResourceView* GetSoftTextureSRV(Tsukino::EngineIntegration::EngineContext& context, bool round) {
            const Tsukino::Asset::AssetHandle handle =
                Tsukino::Asset::AssetHandleGenerator::GenerateFromKey(round ? "procedural|weapon|tether|softdot" : "procedural|weapon|tether|softline");
            if(Tsukino::Core::Ref<Tsukino::Asset::IAsset> existing = context.assetManager->Get(handle))
                return context.renderer->GetResources().GetTextureSRV(*std::static_pointer_cast<Tsukino::Asset::TextureAsset>(existing));

            const int width = 32, height = round ? 32 : 4;
            std::vector<std::uint8_t> pixels(static_cast<size_t>(width) * height * 4);
            for(int y = 0; y < height; ++y) {
                for(int x = 0; x < width; ++x) {
                    const float dx    = ((x + 0.5f) / width - 0.5f) * 2.0f;    // 中央0、両端±1
                    const float dy    = round ? ((y + 0.5f) / height - 0.5f) * 2.0f : 0.0f;
                    const float d     = std::sqrt(dx * dx + dy * dy);
                    const float value = std::exp(-d * d * 4.0f) * std::clamp(1.0f - std::abs(d), 0.0f, 1.0f) * 1.2f;
                    const auto  byte  = static_cast<std::uint8_t>(std::clamp(value, 0.0f, 1.0f) * 255.0f + 0.5f);
                    std::uint8_t* pixel = &pixels[(static_cast<size_t>(y) * width + x) * 4];
                    pixel[0] = pixel[1] = pixel[2] = pixel[3] = byte;
                }
            }

            auto texture    = std::make_shared<Tsukino::Asset::TextureAsset>();
            texture->width  = static_cast<Tsukino::u32>(width);
            texture->height = static_cast<Tsukino::u32>(height);
            texture->format = DXGI_FORMAT_R8G8B8A8_UNORM;
            texture->pixels = std::move(pixels);
            texture->SetHandle(handle);
            context.assetManager->RegisterAsset(handle, texture);
            return context.renderer->GetResources().GetTextureSRV(*texture);
        }

        //-------------------------------------------------------------
        //! @brief  2次ベジェ曲線上の点
        //-------------------------------------------------------------
        hlslpp::float3 Bezier(const hlslpp::float3& p0, const hlslpp::float3& p1, const hlslpp::float3& p2, float t) {
            const float s = 1.0f - t;
            return p0 * (s * s) + p1 * (2.0f * s * t) + p2 * (t * t);
        }
    }    // namespace

    //-------------------------------------------------------------
    //! @brief システムの更新
    //-------------------------------------------------------------
    void WeaponTetherSystem::Update(Tsukino::ECS::Registry& registry, float deltaTime) {
        const WeaponTetherParams& params = GetParams();

        auto* ctx = registry.GetContext<Tsukino::EngineIntegration::EngineContext*>();
        if(!ctx || !ctx->renderer || !ctx->assetManager || !ctx->builtinAssets)
            return;

        //-------------------------------------------------------------
        // メッシュとパイプラインは初回だけ作る
        //-------------------------------------------------------------
        if(m_stripMesh.indexCount == 0) {
            m_stripMesh = Tsukino::Renderer::CreateMeshBuffer(ctx->renderer->GetDevice(), BuildStripMeshData());
            if(m_stripMesh.indexCount == 0) {
                Tsukino::Core::Log::Error("WeaponTetherSystem - failed to create the strip mesh. Weapon tethers will not be drawn.");
                return;
            }
        }
        if(!m_pipeline || !m_shadePipeline) {
            auto vsAsset = std::static_pointer_cast<Tsukino::Asset::ShaderAsset>(ctx->assetManager->Get(ctx->builtinAssets->shaders.spriteWorldVS));
            auto psAsset = std::static_pointer_cast<Tsukino::Asset::ShaderAsset>(ctx->assetManager->Get(ctx->builtinAssets->shaders.spritePS));
            if(!vsAsset || !psAsset)
                return;
            // 光は加算合成、下に敷く暗い帯は普通の半透明。どちらも体や地面には深度テストで正しく隠れ、自分は深度を書かない
            auto* factory   = ctx->renderer->GetResources().GetPipelineFactory();
            m_pipeline      = factory->Create(*vsAsset, *psAsset, Tsukino::GraphicsCommon::VertexFormat::Sprite, Tsukino::Renderer::DepthMode::ReadOnly,
                                              Tsukino::Renderer::BlendMode::Additive);
            m_shadePipeline = factory->Create(*vsAsset, *psAsset, Tsukino::GraphicsCommon::VertexFormat::Sprite, Tsukino::Renderer::DepthMode::ReadOnly,
                                              Tsukino::Renderer::BlendMode::Alpha);
            if(!m_pipeline || !m_shadePipeline)
                return;
        }

        //-------------------------------------------------------------
        // プレイヤーと主カメラ
        //-------------------------------------------------------------
        entt::entity player = entt::null;
        for(auto entity : registry.View<PlayerComponent, Tsukino::BuiltIn::ECS::TransformComponent>()) {
            player = entity;
            break;
        }
        hlslpp::float3 cameraPosition(0.0f, 0.0f, 0.0f);
        bool           hasCamera = false;
        {
            auto cameraView = registry.View<Tsukino::BuiltIn::ECS::CameraComponent, Tsukino::BuiltIn::ECS::TransformComponent>();
            for(auto entity : cameraView) {
                if(!cameraView.get<Tsukino::BuiltIn::ECS::CameraComponent>(entity).isPrimary)
                    continue;
                cameraPosition = cameraView.get<Tsukino::BuiltIn::ECS::TransformComponent>(entity).position;
                hasCamera      = true;
                break;
            }
        }
        if(player == entt::null || !hasCamera) {
            m_tethers.clear();
            return;
        }

        //-------------------------------------------------------------
        // 背中の点。背骨のボーンから、プレイヤーの向きで背中側へずらす（ボーンが引けなければ体の中心から）
        //-------------------------------------------------------------
        const auto& ownerTransform = registry.GetComponent<Tsukino::BuiltIn::ECS::TransformComponent>(player);
        Tsukino::BuiltIn::ECS::ResolveBoneNodeIndex(ctx, registry, player, params.anchorBone, m_spineResolvedModel, m_spineNode);
        hlslpp::float3 anchor = ownerTransform.position + hlslpp::float3(0.0f, 130.0f, 0.0f);
        if(m_spineNode != UINT32_MAX && registry.HasComponent<Tsukino::BuiltIn::ECS::NodeWorldMatrixComponent>(player)) {
            const auto& matrices = registry.GetComponent<Tsukino::BuiltIn::ECS::NodeWorldMatrixComponent>(player);
            if(m_spineNode < matrices.matrices.size()) {
                hlslpp::float3     bonePosition;
                hlslpp::quaternion boneRotation;
                Tsukino::Core::Math::matrix::decomposePositionRotation(matrices.matrices[m_spineNode], bonePosition, boneRotation);
                anchor = ownerTransform.position + hlslpp::mul(bonePosition * ownerTransform.scale, ownerTransform.rotation);
            }
        }
        anchor += hlslpp::mul(params.anchorOffset, ownerTransform.rotation);

        //-------------------------------------------------------------
        // カメラを向いた細い板を1枚積む。from→toを長さ方向、カメラとの外積を幅方向にする
        //-------------------------------------------------------------
        ID3D11ShaderResourceView* softLine = GetSoftTextureSRV(*ctx, false);
        ID3D11ShaderResourceView* softDot  = GetSoftTextureSRV(*ctx, true);
        //! shadeがtrueなら暗い帯（普通の半透明。intensityを透明度に使う）。Transparentパスは積んだ順に描く約束
        //! （エンジンのRenderPassの注記）なので、暗い帯を光より先に積めば、光がその上に重なる
        auto pushStrip = [&](const hlslpp::float3& from, const hlslpp::float3& to, float width, const hlslpp::float3& color, float intensity,
                             ID3D11ShaderResourceView* texture, bool shade = false) {
            const hlslpp::float3 along  = to - from;
            const float          length = hlslpp::length(along);
            if(length < 0.01f || width <= 0.0f || intensity <= 0.0f)
                return;
            const hlslpp::float3 forward = along / length;
            hlslpp::float3       side    = hlslpp::cross(forward, cameraPosition - (from + to) * 0.5f);
            const float          sideLen = hlslpp::length(side);
            if(sideLen < 1.0e-4f)
                return;
            side                       = side / sideLen;
            const hlslpp::float3 normal = hlslpp::cross(side, forward);

            Tsukino::Renderer::Material& material = ctx->renderer->GetDrawQueue().AllocMaterial();
            material.SetPipeline(shade ? m_shadePipeline.get() : m_pipeline.get());
            material.SetSampler(ctx->renderer->GetResources().GetSampler(Tsukino::GraphicsCommon::SamplerType::LinearClamp));
            material.SetTexture(texture ? texture : ctx->renderer->GetResources().GetWhiteTextureSRV());

            Tsukino::Renderer::CBufferMaterial& data = ctx->renderer->GetDrawQueue().AllocMaterialData();
            data                                     = Tsukino::Renderer::CBufferMaterial{};
            data.baseColor = shade ? hlslpp::float4(color, std::min(intensity, 1.0f)) : hlslpp::float4(color * intensity, std::min(intensity, 1.0f));
            // 霧の掛け方。光（加算）は霧の色へ寄せず、遠いほど弱める
            data.spriteFog = hlslpp::float4(shade ? 0.0f : 1.0f, 0.0f, 0.0f, 0.0f);

            // 行ベクトル規約：1行目が板の横（幅）、2行目が板の法線、3行目が長さ方向、4行目が根元の位置
            Tsukino::Renderer::DrawCommand cmd{};
            cmd.mesh          = &m_stripMesh;
            cmd.material      = &material;
            cmd.materialData  = &data;
            cmd.transform     = Tsukino::Core::Math::matrix(hlslpp::float4x4(hlslpp::float4(side * width, 0.0f), hlslpp::float4(normal, 0.0f),
                                                                               hlslpp::float4(forward * length, 0.0f), hlslpp::float4(from, 1.0f)));
            cmd.prevTransform = cmd.transform;
            cmd.hasPrevFrame  = false;
            // 深度を書かないので、フォグの後のTransparentで描く（エンジンのRenderPassの約束。自分の距離ぶんの霧はSprite.ps.hlslが掛ける）
            cmd.pass          = Tsukino::Renderer::RenderPass::Transparent;
            cmd.castsShadow   = false;
            ctx->renderer->GetDrawQueue().Push(cmd);
        };

        //-------------------------------------------------------------
        // 浮いている武器ごとに、背中の点から柄へ糸を張る
        //-------------------------------------------------------------
        for(auto& entry : m_tethers)
            entry.second.seen = false;

        if(deltaTime > 0.0f)
            m_time += deltaTime;
        const float time       = m_time;
        float       strongest  = 0.0f;
        const int   segments   = std::clamp(params.segments, 2, kMaxSegments);
        auto        weaponView = registry.View<WeaponComponent, Tsukino::BuiltIn::ECS::TransformComponent>();
        for(auto entity : weaponView) {
            const WeaponComponent& weapon = weaponView.get<WeaponComponent>(entity);
            if(weapon.owner != player || !weapon.floatEnabled)
                continue;
            const hlslpp::float3 grip = weaponView.get<Tsukino::BuiltIn::ECS::TransformComponent>(entity).position;

            TetherState& state = m_tethers[entity];
            state.seen         = true;

            //-------------------------------------------------------------
            // 伸び方と、武器の遅れによるしなり。背中の点から見た柄の動きの逆向きへしならせる
            // （武器が後ろへ置いていかれると、糸は前へ引っ張られる弧になる）
            //-------------------------------------------------------------
            const hlslpp::float3 offset = grip - anchor;
            if(deltaTime > 0.0f) {
                state.appear = std::min(state.appear + deltaTime / std::max(params.growSeconds, 0.01f), 1.0f);
                hlslpp::float3 bendTarget(0.0f, 0.0f, 0.0f);
                if(state.hasPrev) {
                    bendTarget         = -(offset - state.prevOffset) / deltaTime * params.lagBend;
                    const float amount = hlslpp::length(bendTarget);
                    if(amount > params.maxBend)
                        bendTarget = bendTarget * (params.maxBend / amount);
                }
                state.bend += (bendTarget - state.bend) * (1.0f - std::exp(-params.bendSmoothing * deltaTime));
                state.prevOffset = offset;
                state.hasPrev    = true;
            }

            // 攻撃で手に持っている間は消す。選んでいる武器は明るく太く
            const float visible  = (1.0f - weapon.attackBlend) * (1.0f - weapon.attackBlend);
            const float selected = 1.0f + (params.selectedBoost - 1.0f) * weapon.floatSelectBlend;
            const float strength = visible * selected;
            if(strength <= 0.01f || state.appear <= 0.0f)
                continue;
            strongest = std::max(strongest, strength);

            //-------------------------------------------------------------
            // 曲線。中ほどを下へ垂らし、しなりとゆらぎを足す（武器ごとに位相をずらす）
            //-------------------------------------------------------------
            const float          phase   = static_cast<float>(static_cast<std::uint32_t>(entity) % 97u) * 0.37f;
            const hlslpp::float3 sway    = hlslpp::float3(std::sin(time * params.flutterSpeed + phase), std::sin(time * params.flutterSpeed * 0.7f + phase * 2.0f),
                                                          std::cos(time * params.flutterSpeed * 1.3f + phase)) * params.flutter;
            const hlslpp::float3 control = (anchor + grip) * 0.5f + hlslpp::float3(0.0f, -params.sag * 2.0f, 0.0f) + state.bend * 2.0f + sway;

            // 拾った直後は背中側から伸ばす（曲線のうち appear の割合だけ描く）
            const float ease = 1.0f - (1.0f - state.appear) * (1.0f - state.appear);
            hlslpp::float3 previous = anchor;
            for(int i = 1; i <= segments; ++i) {
                const hlslpp::float3 point = Bezier(anchor, control, grip, ease * static_cast<float>(i) / static_cast<float>(segments));
                pushStrip(previous, point, params.shadeWidth * selected, params.shadeColor, params.shadeAlpha * visible, softLine, true);
                pushStrip(previous, point, params.glowWidth * selected, params.color, params.glowIntensity * strength, softLine);
                pushStrip(previous, point, params.coreWidth * selected, params.color, params.coreIntensity * strength, softLine);
                previous = point;
            }
        }

        // 見えなくなった武器（捨てた・吸収された・壊れた）の状態を消す。次に拾ったらまた背中から伸ばす
        for(auto it = m_tethers.begin(); it != m_tethers.end();)
            it = it->second.seen ? std::next(it) : m_tethers.erase(it);

        //-------------------------------------------------------------
        // 背中の光の点。糸が1本でも見えている間だけ、ゆっくり脈打たせて出す
        //-------------------------------------------------------------
        if(strongest > 0.01f) {
            const float size = params.anchorSize * (1.0f + params.anchorPulse * std::sin(time * params.anchorPulseSpeed));
            const hlslpp::float3 toCamera = hlslpp::normalize(cameraPosition - anchor);
            hlslpp::float3       up       = hlslpp::float3(0.0f, 1.0f, 0.0f);
            up                            = hlslpp::normalize(up - toCamera * hlslpp::dot(up, toCamera));
            // 板は手前の辺が原点なので、点の中心から下へ半分ずらして立てる
            pushStrip(anchor - up * (size * 0.4f), anchor + up * (size * 0.4f), size * 0.8f, params.shadeColor, params.shadeAlpha * std::min(strongest, 1.0f),
                      softDot, true);
            pushStrip(anchor - up * (size * 0.5f), anchor + up * (size * 0.5f), size, params.color, params.coreIntensity * std::min(strongest, 1.0f), softDot);
            pushStrip(anchor - up * (size * 0.2f), anchor + up * (size * 0.2f), size * 0.4f, hlslpp::float3(1.0f, 1.0f, 1.0f),
                      params.coreIntensity * std::min(strongest, 1.0f), softDot);
        }
    }
}    // namespace CombatAndroid::ECS
