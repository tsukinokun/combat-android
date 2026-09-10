//--------------------------------------------------------------
//! @file   GrassFieldSystem.cpp
//! @brief  草原システムの実装
//! @author 山﨑愛
//--------------------------------------------------------------
#include <CombatAndroid/ECS/System/GrassFieldSystem.hpp>
#include <Tsukino/EngineIntegration/EngineContext.hpp>

#include <CombatAndroid/ECS/Component/GrassFieldComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/CharacterControllerComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/TransformComponent.hpp>
#include <Tsukino/BuiltIn/BuiltInAssets.hpp>

#include <Tsukino/GraphicsCommon/Mesh/MeshData.hpp>
#include <Tsukino/GraphicsCommon/Vertex/VertexPNUV.hpp>
#include <Tsukino/GraphicsCommon/Vertex/VertexFormat.hpp>

#include <Tsukino/Engine/Asset/AssetManager.hpp>
#include <Tsukino/Engine/Asset/Shader/ShaderAsset.hpp>
#include <Tsukino/Engine/Asset/Texture/TextureAsset.hpp>
#include <Tsukino/Engine/Asset/Util/AssetHandleGenerator.hpp>

#include <Tsukino/Renderer/Renderer.hpp>
#include <Tsukino/Renderer/ConstantBuffer.hpp>
#include <Tsukino/Renderer/DrawCommand.hpp>
#include <Tsukino/Renderer/ShaderSlots.hpp>
#include <Tsukino/Renderer/DX11/PipelineFactory.hpp>
#include <Tsukino/Renderer/DX11/UserConstantBuffer.hpp>

#include <Tsukino/Core/Log.hpp>
#include <Tsukino/Core/Math/Matrix.hpp>

#include <entt/entt.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    namespace {
        //--------------------------------------------------------------
        // 刃メッシュの分割数。
        // 4段だと根元から先端までのしなりが滑らかに見え、かつ
        // 1本9頂点（先端は1点に潰す）に収まる
        //--------------------------------------------------------------
        constexpr Tsukino::u32 kBladeSegments = 4;

        //! 根元→先端のグラデーションテクスチャの高さ（ピクセル）。種1つぶん
        //! @note Grass.vs.hlsl 側でも同じ値を前提にUVを計算しているため、
        //!       変えるならそちらも直すこと
        constexpr Tsukino::u32 kGradientHeight = 32;

        //! 同じく幅。1pxでも足りるが、行あたりのバイト数が極端に小さいと
        //! ドライバによっては扱いが不安定なので4pxにしておく
        constexpr Tsukino::u32 kGradientWidth = 4;

        //! 草の種類数。CBufferGrass（GrassFieldSystem.hpp）と
        //! Grass.vs.hlsl の kSpeciesCount にも同じ値を決め打ちしている
        constexpr Tsukino::u32 kSpeciesCount = 3;

        //! 1本の刃を構成する面の数（クロスビルボード。下のBuildBladeMeshData参照）
        constexpr Tsukino::u32 kBladeFaceCount = 2;

        //--------------------------------------------------------------
        //! 草1本ぶんの刃メッシュを組み立てます。
        //! @param  [in] bladeWidth 根元の幅
        //! @return 刃メッシュのデータ
        //! @note   高さは1.0で作り、実際の高さは頂点シェーダーが掛ける。
        //!         こうしておくと草ごとに高さがばらついてもメッシュは1本で済む。
        //!         UVのvに根元→先端の比率をそのまま入れてあり、
        //!         頂点シェーダーはこれをしなりの重み、ピクセルシェーダーは
        //!         グラデーションテクスチャの参照位置として使う。
        //!
        //!         板を1枚だけにすると、真横に近い角度から見たときに厚み0の
        //!         線へ潰れて奥の地面が素通しに見えてしまう。根元の中心線で
        //!         直交する板をもう1枚足した「クロスビルボード」にすることで、
        //!         どの向きから見ても必ずどちらか一方が正面〜斜めに近い角度で
        //!         見え、地肌が透けにくくなる（多くのゲームで使われる定番の
        //!         草表現）。面0はローカルX方向に幅を持ち法線はZ向き、
        //!         面1はその90度回転版（Z方向に幅、法線はX向き）。
        //!         Grass.vs.hlslは法線のx/z成分で頂点がどちらの面のものかを
        //!         読み分け、幅を出す軸（sideDir/facingDir）を切り替える
        //--------------------------------------------------------------
        Tsukino::GraphicsCommon::MeshData BuildBladeMeshData(float bladeWidth) {
            Tsukino::GraphicsCommon::MeshData mesh;
            mesh.format       = Tsukino::GraphicsCommon::VertexFormat::PositionNormalUV;
            mesh.vertexStride = sizeof(Tsukino::GraphicsCommon::VertexPNUV);

            const Tsukino::u32 verticesPerFace = kBladeSegments * 2 + 1;

            std::vector<Tsukino::GraphicsCommon::VertexPNUV> vertices;
            vertices.reserve(verticesPerFace * kBladeFaceCount);

            //----------------------------------------------------------
            // 面を1枚ぶん積む。根元から先端へ向かって、幅を細らせながら
            // 2頂点ずつ積み、最上段だけは1頂点に潰して尖らせる
            //----------------------------------------------------------
            auto appendFace = [&](bool widthAlongZ) {
                for(Tsukino::u32 row = 0; row <= kBladeSegments; ++row) {
                    const float t = static_cast<float>(row) / static_cast<float>(kBladeSegments);

                    // 先細りの曲線。1 - t^2 にすると根元側の太さが保たれたまま
                    // 先端だけが急に細くなり、草らしいシルエットになる
                    const float halfWidth = bladeWidth * 0.5f * (1.0f - t * t);

                    if(row == kBladeSegments) {
                        // 先端は1点
                        Tsukino::GraphicsCommon::VertexPNUV tip{};
                        tip.position = {0.0f, t, 0.0f};
                        tip.normal   = widthAlongZ ? DirectX::XMFLOAT3{1.0f, 0.0f, 0.0f} : DirectX::XMFLOAT3{0.0f, 0.0f, 1.0f};
                        tip.uv       = {0.5f, t};
                        vertices.push_back(tip);
                        continue;
                    }

                    Tsukino::GraphicsCommon::VertexPNUV a{};
                    Tsukino::GraphicsCommon::VertexPNUV b{};
                    if(widthAlongZ) {
                        a.position = {0.0f, t, -halfWidth};
                        b.position = {0.0f, t, halfWidth};
                        a.normal = b.normal = DirectX::XMFLOAT3{1.0f, 0.0f, 0.0f};
                    } else {
                        a.position = {-halfWidth, t, 0.0f};
                        b.position = {halfWidth, t, 0.0f};
                        a.normal = b.normal = DirectX::XMFLOAT3{0.0f, 0.0f, 1.0f};
                    }
                    a.uv = {0.0f, t};
                    b.uv = {1.0f, t};
                    vertices.push_back(a);
                    vertices.push_back(b);
                }
            };

            appendFace(false);    // 面0：幅はX方向
            appendFace(true);     // 面1：幅はZ方向（90度回転）

            //----------------------------------------------------------
            // インデックス。面ごとに頂点の開始位置をずらして同じ並びを積む。
            // 最上段以外は四角形、最上段だけ三角形。
            // カリングは無効（エンジン全体がCullNone）なので巻き方向は問わない
            //----------------------------------------------------------
            for(Tsukino::u32 face = 0; face < kBladeFaceCount; ++face) {
                const Tsukino::u32 faceBase = face * verticesPerFace;

                for(Tsukino::u32 seg = 0; seg < kBladeSegments; ++seg) {
                    const Tsukino::u32 base = faceBase + seg * 2;

                    if(seg == kBladeSegments - 1) {
                        // 先端の三角形（左・右・頂点）
                        mesh.indices.push_back(base);
                        mesh.indices.push_back(base + 1);
                        mesh.indices.push_back(base + 2);
                        continue;
                    }

                    mesh.indices.push_back(base);
                    mesh.indices.push_back(base + 1);
                    mesh.indices.push_back(base + 2);

                    mesh.indices.push_back(base + 1);
                    mesh.indices.push_back(base + 3);
                    mesh.indices.push_back(base + 2);
                }
            }

            mesh.vertexCount = static_cast<Tsukino::u32>(vertices.size());
            mesh.vertexData.resize(vertices.size() * sizeof(Tsukino::GraphicsCommon::VertexPNUV));
            std::memcpy(mesh.vertexData.data(), vertices.data(), mesh.vertexData.size());

            mesh.indexCount = static_cast<Tsukino::u32>(mesh.indices.size());

            return mesh;
        }

        //--------------------------------------------------------------
        //! 種ごとの根元→先端グラデーションを縦に並べたテクスチャを取得します。
        //! @param  [in,out] context エンジンコンテキスト
        //! @param  [in]     species 種の配列（各要素の rootColor/tipColor を使う）
        //! @return グラデーションのSRV。作れなければ nullptr
        //! @note   ピクセルシェーダー（GBuffer.ps.hlsl）を書き換えずに
        //!         種ごとの色分けを出すための手。テクスチャは縦に
        //!         種の数ぶんの帯（各 kGradientHeight px）を並べたもので、
        //!         種0の帯が根元→先端で rootColor→tipColor、種1の帯が
        //!         続けて同じ変化…という並び。頂点シェーダーが
        //!         speciesIndexから自分の帯のV座標を計算してサンプルする。
        //!         色をキーに含めるので、色を変えれば別のテクスチャが作られる
        //--------------------------------------------------------------
        ID3D11ShaderResourceView* GetGradientSRV(Tsukino::EngineIntegration::EngineContext& context,
                                                 const std::array<GrassSpecies, kSpeciesCount>& species) {
            if(!context.assetManager || !context.renderer)
                return nullptr;

            //----------------------------------------------------------
            // 全種の色から一意なキーを作る。AssetManagerはハンドルでキャッシュ
            // するので、同じ組み合わせなら2回目以降は生成せずキャッシュが返る
            //----------------------------------------------------------
            auto toByte = [](float v) { return static_cast<int>(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f); };

            std::string key = "procedural|grass|gradient";
            for(const GrassSpecies& s : species) {
                key += "|" + std::to_string(toByte(s.rootColor.x)) + "_" + std::to_string(toByte(s.rootColor.y)) + "_"
                      + std::to_string(toByte(s.rootColor.z)) + "_" + std::to_string(toByte(s.tipColor.x)) + "_"
                      + std::to_string(toByte(s.tipColor.y)) + "_" + std::to_string(toByte(s.tipColor.z));
            }

            const Tsukino::Asset::AssetHandle handle = Tsukino::Asset::AssetHandleGenerator::GenerateFromKey(key);

            //----------------------------------------------------------
            // 既に登録済みならそれを使う
            //----------------------------------------------------------
            if(Tsukino::Core::Ref<Tsukino::Asset::IAsset> existing = context.assetManager->Get(handle)) {
                auto texture = std::static_pointer_cast<Tsukino::Asset::TextureAsset>(existing);
                return context.renderer->GetTextureSRV(*texture);
            }

            //----------------------------------------------------------
            // ピクセルを作る。種ごとに kGradientHeight 行の帯を積む。
            // 各帯の中はvが0（根元）の行が先頭に来るので、
            // 上から下へ rootColor → tipColor で埋める
            //----------------------------------------------------------
            const Tsukino::u32 totalHeight = kGradientHeight * kSpeciesCount;

            auto texture    = std::make_shared<Tsukino::Asset::TextureAsset>();
            texture->width  = kGradientWidth;
            texture->height = totalHeight;
            texture->format = DXGI_FORMAT_R8G8B8A8_UNORM;
            texture->pixels.resize(static_cast<size_t>(kGradientWidth) * totalHeight * 4);

            for(Tsukino::u32 speciesIndex = 0; speciesIndex < kSpeciesCount; ++speciesIndex) {
                const hlslpp::float3& rootColor = species[speciesIndex].rootColor;
                const hlslpp::float3& tipColor  = species[speciesIndex].tipColor;

                for(Tsukino::u32 row = 0; row < kGradientHeight; ++row) {
                    const float t = static_cast<float>(row) / static_cast<float>(kGradientHeight - 1);

                    const hlslpp::float3 color = rootColor + (tipColor - rootColor) * t;

                    const Tsukino::u8 r = static_cast<Tsukino::u8>(toByte(color.x));
                    const Tsukino::u8 g = static_cast<Tsukino::u8>(toByte(color.y));
                    const Tsukino::u8 b = static_cast<Tsukino::u8>(toByte(color.z));

                    const Tsukino::u32 y = speciesIndex * kGradientHeight + row;

                    for(Tsukino::u32 x = 0; x < kGradientWidth; ++x) {
                        const size_t offset = (static_cast<size_t>(y) * kGradientWidth + x) * 4;

                        texture->pixels[offset + 0] = r;
                        texture->pixels[offset + 1] = g;
                        texture->pixels[offset + 2] = b;
                        texture->pixels[offset + 3] = 255;
                    }
                }
            }

            texture->SetHandle(handle);
            context.assetManager->RegisterAsset(handle, texture);

            return context.renderer->GetTextureSRV(*texture);
        }
    }    // namespace

    //--------------------------------------------------------------
    //! 更新処理を行います。
    //--------------------------------------------------------------
    void GrassFieldSystem::Update(Tsukino::ECS::Registry& registry, float deltaTime) {
        Tsukino::EngineIntegration::EngineContext* ctx = registry.GetContext<Tsukino::EngineIntegration::EngineContext*>();
        if(!ctx || !ctx->renderer || !ctx->assetManager || !ctx->builtinAssets)
            return;

        //--------------------------------------------------------------
        // GrassFieldComponent を探す
        // 複数あった場合は最初の1つだけ使用する（AmbientParticleSystemと同じ流儀）
        //--------------------------------------------------------------
        const GrassFieldComponent* activeField = nullptr;

        auto fieldView = registry.View<GrassFieldComponent>();
        fieldView.each([&](entt::entity entity, const GrassFieldComponent& field) {
            if(!activeField && field.enabled)
                activeField = &field;
        });

        if(!activeField)
            return;

        //--------------------------------------------------------------
        // 草を揺らすための経過時間を進める
        //--------------------------------------------------------------
        m_time += deltaTime;

        //--------------------------------------------------------------
        // 本数の上限チェック（超過分は切り捨て、初回のみ警告する）
        //--------------------------------------------------------------
        Tsukino::u32 bladeCount = activeField->bladeCount;
        if(bladeCount > kMaxGrassBlades) {
            if(!m_countOverflowWarned) {
                Tsukino::Core::Log::Error("GrassFieldSystem - blade count (" + std::to_string(bladeCount) + ") exceeds kMaxGrassBlades ("
                                          + std::to_string(kMaxGrassBlades) + "). Extra blades are dropped.");
                m_countOverflowWarned = true;
            }
            bladeCount = kMaxGrassBlades;
        }

        if(bladeCount == 0)
            return;

        //--------------------------------------------------------------
        // 刃メッシュの作成（幅が変わったときだけ作り直す）
        //--------------------------------------------------------------
        if(m_bladeMesh.indexCount == 0 || m_builtBladeWidth != activeField->bladeWidth) {
            const Tsukino::GraphicsCommon::MeshData meshData = BuildBladeMeshData(activeField->bladeWidth);

            m_bladeMesh       = Tsukino::Renderer::CreateMeshBuffer(ctx->renderer->GetDevice(), meshData);
            m_builtBladeWidth = activeField->bladeWidth;

            if(m_bladeMesh.indexCount == 0) {
                Tsukino::Core::Log::Error("GrassFieldSystem - failed to create the grass blade mesh. Grass will not be drawn.");
                return;
            }
        }

        //--------------------------------------------------------------
        // 格子の分割数を決める。
        // セルが細かすぎると1セルに1本も入らず配置が偏るので、
        // 「1セルあたり4本前後」になるところを狙って分割数を決める
        //--------------------------------------------------------------
        constexpr float kTargetBladesPerCell = 4.0f;

        const float gridDimF   = std::floor(std::sqrt(static_cast<float>(bladeCount) / kTargetBladesPerCell));
        const float gridDim    = std::max(gridDimF, 1.0f);
        const float cellCountF = gridDim * gridDim;

        const Tsukino::u32 cellCount = static_cast<Tsukino::u32>(cellCountF);

        //--------------------------------------------------------------
        // 1セルあたりの本数は切り捨てる。
        // 切り上げると gridDim^2 * perCell が要求本数を上回り、あぶれたセルには
        // インスタンス番号が割り当たらないまま残る。頂点シェーダーは
        // 「インスタンス番号 → セル番号」の順で格子を埋めていくので、
        // 埋まらなかったぶんは格子の後ろ側の行にまとまって現れ、
        // 「+Z方向だけ草が近くで途切れる」という形で見える
        //--------------------------------------------------------------
        const Tsukino::u32 perCell = std::max(1u, bladeCount / cellCount);

        //--------------------------------------------------------------
        // 実際に描くのは格子をちょうど埋める本数。
        // 要求本数をそのまま渡すと端数のぶんだけ格子の一部が空く。
        // perCellが切り捨てなので、この値が要求本数を超えることはない
        //--------------------------------------------------------------
        const Tsukino::u32 drawCount = cellCount * perCell;

        //--------------------------------------------------------------
        // プレイヤー位置の収集。
        // CharacterControllerComponent を持つ最初のエンティティを追う
        //--------------------------------------------------------------
        hlslpp::float3 playerPos(0.0f, 0.0f, 0.0f);
        float          pushRadius = 0.0f;

        auto playerView =
            registry.View<Tsukino::BuiltIn::ECS::CharacterControllerComponent, Tsukino::BuiltIn::ECS::TransformComponent>();
        playerView.each([&](entt::entity entity, const Tsukino::BuiltIn::ECS::CharacterControllerComponent& controller,
                            const Tsukino::BuiltIn::ECS::TransformComponent& transform) {
            if(pushRadius > 0.0f)
                return;

            playerPos  = transform.position;
            pushRadius = activeField->playerPushRadius;
        });

        //--------------------------------------------------------------
        // 定数バッファの詰め替え
        //--------------------------------------------------------------
        hlslpp::float3 windDir = activeField->windDirection;

        // 真上向きだけを入れられると水平成分がゼロになり normalize が壊れるので、
        // 水平成分が無いときは既定の向きへ倒す
        const float windHorizontal = std::sqrt(static_cast<float>(windDir.x * windDir.x + windDir.z * windDir.z));
        if(windHorizontal < 1.0e-4f)
            windDir = hlslpp::float3(1.0f, 0.0f, 0.0f);
        else
            windDir = windDir / windHorizontal;

        CBufferGrass params{};
        params.fieldParams       = hlslpp::float4(activeField->fieldSize, gridDim, static_cast<float>(perCell), m_time);
        params.bladeParams       = hlslpp::float4(activeField->distantWidthBoost, activeField->heightVariance, activeField->groundHeight,
                                                  activeField->patchSize);
        params.windParams        = hlslpp::float4(windDir.x, windDir.y, windDir.z, activeField->windStrength);
        params.gustParams        = hlslpp::float4(activeField->gustWavelength, activeField->gustSpeed, activeField->gustStrength,
                                                  activeField->swaySpeed);
        params.swayParams        = hlslpp::float4(activeField->swayStrength, static_cast<float>(activeField->seed & 0x00ffffffu), 0.0f, 0.0f);
        params.speciesHeight     = hlslpp::float4(activeField->species[0].height, activeField->species[1].height,
                                                  activeField->species[2].height, 0.0f);
        params.speciesWidthScale = hlslpp::float4(activeField->species[0].widthScale, activeField->species[1].widthScale,
                                                  activeField->species[2].widthScale, 0.0f);
        params.playerParams      = hlslpp::float4(playerPos.x, playerPos.y, playerPos.z, pushRadius);
        params.fadeParams        = hlslpp::float4(activeField->fadeStartRatio, activeField->playerPushStrength, 0.0f, 0.0f);

        //--------------------------------------------------------------
        // パラメータをゲーム所有の定数バッファへ流し込む。
        // エンジンは草を知らないので、バッファもこちらで持つ
        //--------------------------------------------------------------
        if(!m_paramBuffer.IsValid()) {
            m_paramBuffer = Tsukino::Renderer::CreateUserConstantBuffer(ctx->renderer->GetDevice(), sizeof(CBufferGrass));

            if(!m_paramBuffer.IsValid()) {
                Tsukino::Core::Log::Error("GrassFieldSystem - failed to create the grass constant buffer. Grass will not be drawn.");
                return;
            }
        }

        Tsukino::Renderer::UpdateUserConstantBuffer(ctx->renderer->GetContext(), m_paramBuffer, &params, sizeof(params));

        //--------------------------------------------------------------
        // マテリアルの構築。
        // 頂点シェーダーはゲームのAssetsから読む（エンジンの組み込みではない）。
        // 一方ピクセルシェーダーはモデルと同じ GBuffer.ps.hlsl を使う ――
        // PBRはエンジンの機構なので、これは正しい借り方。
        // おかげで草はディファードライティング・ポイントライト・影・フォグの
        // すべてに自動的に乗る
        //--------------------------------------------------------------
        Tsukino::Asset::AssetHandle grassVSHandle =
            ctx->assetManager->Load(Tsukino::Core::Path("CombatAndroid/Assets/Shaders/Grass.vs.hlsl"));

        auto vsAsset = std::static_pointer_cast<Tsukino::Asset::ShaderAsset>(ctx->assetManager->Get(grassVSHandle));
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
        material.SetSampler(ctx->renderer->GetSampler(Tsukino::GraphicsCommon::SamplerType::LinearClamp));

        // アルベドに種ごとの根元→先端グラデーションを差す。頂点シェーダーが
        // 選んだ種に応じてUVのvを自分の帯へずらして出すので、これだけで
        // 種ごとの色分けが出る
        ID3D11ShaderResourceView* gradientSRV = GetGradientSRV(*ctx, activeField->species);
        material.SetTexture(Tsukino::Renderer::SRVSlot::Albedo, gradientSRV ? gradientSRV : ctx->renderer->GetWhiteTextureSRV());

        // ノーマルマップは使わない。フラット法線を入れると頂点法線がそのまま残る
        material.SetTexture(Tsukino::Renderer::SRVSlot::Normal, ctx->renderer->GetFlatNormalTextureSRV());
        material.SetTexture(Tsukino::Renderer::SRVSlot::MetallicRoughness, ctx->renderer->GetWhiteTextureSRV());
        material.SetTexture(Tsukino::Renderer::SRVSlot::Emissive, ctx->renderer->GetWhiteTextureSRV());
        material.SetTexture(Tsukino::Renderer::SRVSlot::AO, ctx->renderer->GetWhiteTextureSRV());

        //--------------------------------------------------------------
        // マテリアル定数。草は金属ではないので metallic は0、
        // 表面はざらついているので roughness は高め
        //--------------------------------------------------------------
        Tsukino::Renderer::CBufferMaterial& materialData = ctx->renderer->AllocMaterialData();
        materialData            = Tsukino::Renderer::CBufferMaterial{};
        materialData.baseColor  = hlslpp::float4(1.0f, 1.0f, 1.0f, 1.0f);
        materialData.emissive   = hlslpp::float3(0.0f, 0.0f, 0.0f);
        materialData.metallic   = 0.0f;
        materialData.roughness  = 0.85f;
        materialData.specular   = 0.2f;
        materialData.rimColor   = hlslpp::float4(0.0f, 0.0f, 0.0f, 0.0f);
        materialData.rimParams  = hlslpp::float4(1.0f, 0.0f, 0.0f, 0.0f);    // z=alphaCutoff。草はくり抜かないので0

        //--------------------------------------------------------------
        // 描画コマンドを1本だけ積む。
        // 位置も向きも頂点シェーダーがワールド座標で直接組み立てるため、
        // モデル行列は単位行列でよい
        //--------------------------------------------------------------
        Tsukino::Renderer::DrawCommand cmd{};
        cmd.mesh          = &m_bladeMesh;
        cmd.material      = &material;
        cmd.materialData  = &materialData;
        cmd.transform     = Tsukino::Core::Math::matrix::identity();
        cmd.prevTransform = cmd.transform;
        cmd.hasPrevFrame  = false;
        cmd.pass          = Tsukino::Renderer::RenderPass::GBuffer;
        cmd.instanceCount = drawCount;

        // 影は落とさない。シャドウパスは固定のシャドウ用シェーダーで描き直すため、
        // 草のように頂点シェーダーが位置を組み立てるものは全インスタンスが
        // 原点へ重なった状態で描かれてしまう（影を落とさせるには専用の
        // シャドウ用頂点シェーダーを差し替えられるようにする必要がある）
        cmd.castsShadow = false;

        // 草のパラメータをゲーム予約枠へ渡す
        cmd.userConstantBuffer = m_paramBuffer.buffer.Get();
        cmd.userConstantSlot   = Tsukino::Renderer::CBSlot::User0;

        ctx->renderer->PushDrawCommand(cmd);
    }
}    // namespace CombatAndroid::ECS
