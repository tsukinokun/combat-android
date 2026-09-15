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
#include <array>
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
                return context.renderer->GetResources().GetTextureSRV(*texture);
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

            return context.renderer->GetResources().GetTextureSRV(*texture);
        }

        //--------------------------------------------------------------
        //! 草を並べる格子の分け方
        //--------------------------------------------------------------
        struct GrassGrid {
            float        gridDim   = 0.0f;    //!< 1辺のセル数
            Tsukino::u32 perCell   = 0;       //!< 1セルあたりの本数
            Tsukino::u32 drawCount = 0;       //!< 実際に描く本数（格子をちょうど埋める本数）
        };

        //--------------------------------------------------------------
        //! 近景・遠景それぞれの描き方
        //--------------------------------------------------------------
        struct GrassLayer {
            float     fieldSize         = 0.0f;    //!< カメラを中心に草を敷く正方形の一辺
            GrassGrid grid;                        //!< 格子の分け方
            float     lodStart          = 0.0f;    //!< 近景→遠景の切替を始める距離
            float     lodEnd            = 0.0f;    //!< 切替を終える距離
            float     layerIndex        = 0.0f;    //!< 0: 近景（奥ほど間引く）, 1: 遠景（手前ほど間引く）
            float     fadeStart         = 0.0f;    //!< 外周で背を縮め始める距離
            float     widthCompensation = 1.0f;    //!< 本数の少なさを補う幅の倍率
        };

        //--------------------------------------------------------------
        //! 要求された本数から、草を並べる格子の分け方を決めます。
        //! @param  [in] bladeCount 要求された本数（kMaxGrassBlades以下に丸め済みであること）
        //! @return 格子の分け方。bladeCountが0ならdrawCountも0
        //--------------------------------------------------------------
        GrassGrid ComputeGrassGrid(Tsukino::u32 bladeCount) {
            GrassGrid grid;
            if(bladeCount == 0)
                return grid;

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
            grid.gridDim   = gridDim;
            grid.perCell   = perCell;
            grid.drawCount = cellCount * perCell;
            return grid;
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
        // 本数の上限チェック（超過分は切り捨て、初回のみ警告する）。
        // 上限は1回の描画あたりなので、近景と遠景で別々に丸める
        //--------------------------------------------------------------
        auto clampBladeCount = [&](Tsukino::u32 requested) {
            if(requested <= kMaxGrassBlades)
                return requested;

            if(!m_countOverflowWarned) {
                Tsukino::Core::Log::Error("GrassFieldSystem - blade count (" + std::to_string(requested) + ") exceeds kMaxGrassBlades ("
                                          + std::to_string(kMaxGrassBlades) + "). Extra blades are dropped.");
                m_countOverflowWarned = true;
            }
            return kMaxGrassBlades;
        };

        //--------------------------------------------------------------
        // 近景と遠景の受け持ちを決める。
        // 近景（fieldSize）は密で細い草、遠景（farFieldSize）は本数を抑えて幅で補った草で、
        // TPSカメラの視界の端まで敷く。1回で地平線まで敷くと、画面上で数ピクセルにしか
        // ならない遠くの草にまで近景と同じ密度を割くことになり、本数が何倍にも膨らむ。
        // 切替の距離帯では頂点シェーダーが1本ごとの乱数で本数を入れ替える。
        //
        // 遠景を描かない設定（farFieldSize が fieldSize 以下、または farBladeCount が0）なら、
        // 近景が外周で背を縮めて消える（遠景を足す前と同じ挙動）
        //--------------------------------------------------------------
        const float nearFieldSize = std::max(activeField->fieldSize, 1.0f);
        const float nearHalf      = nearFieldSize * 0.5f;
        const float fadeRatio     = std::clamp(activeField->fadeStartRatio, 0.0f, 0.99f);

        const GrassGrid nearGrid = ComputeGrassGrid(clampBladeCount(activeField->bladeCount));
        const GrassGrid farGrid =
            (activeField->farFieldSize > nearFieldSize) ? ComputeGrassGrid(clampBladeCount(activeField->farBladeCount)) : GrassGrid{};

        std::array<GrassLayer, kGrassLayerCount> layers{};
        Tsukino::u32                             layerCount = 0;

        if(farGrid.drawCount > 0) {
            const float farFieldSize = activeField->farFieldSize;

            // 切替は近景の格子の内側で終わらせる。外にはみ出すと、近景がもう無い距離で
            // 遠景がまだ間引かれていて、帯状に草が薄くなる
            const float blendEnd   = std::clamp(activeField->lodBlendEnd, 0.0f, nearHalf);
            const float blendStart = std::clamp(activeField->lodBlendStart, 0.0f, blendEnd);

            // 面積あたりの本数の比だけ遠景の草を太らせ、画面を覆う割合を近景と揃える。
            // 太らせすぎると1本1本が板に見えるので4倍で頭打ちにする
            const float nearDensity          = static_cast<float>(nearGrid.drawCount) / (nearFieldSize * nearFieldSize);
            const float farDensity           = static_cast<float>(farGrid.drawCount) / (farFieldSize * farFieldSize);
            const float farWidthCompensation = std::clamp(nearDensity / farDensity, 1.0f, 4.0f);

            if(nearGrid.drawCount > 0)
                layers[layerCount++] = {nearFieldSize, nearGrid, blendStart, blendEnd, 0.0f, nearHalf, 1.0f};

            layers[layerCount++] = {farFieldSize, farGrid, blendStart, blendEnd, 1.0f, farFieldSize * 0.5f * fadeRatio, farWidthCompensation};
        } else if(nearGrid.drawCount > 0) {
            // 近景だけ。フィールドの外周より外でだけ間引き、外周で背を縮めて消す
            layers[layerCount++] = {nearFieldSize, nearGrid, nearHalf, nearHalf + 1.0f, 0.0f, nearHalf * fadeRatio, 1.0f};
        }

        if(layerCount == 0)
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

        // 層ごとに変わる fieldParams / lodParams / coverageParams は、
        // 下の描画コマンドを積むところで層ごとに入れる
        CBufferGrass params{};
        params.bladeParams       = hlslpp::float4(activeField->distantWidthBoost, activeField->heightVariance, activeField->groundHeight,
                                                  0.0f);
        params.windParams        = hlslpp::float4(windDir.x, windDir.y, windDir.z, activeField->windStrength);
        params.gustParams        = hlslpp::float4(activeField->gustWavelength, activeField->gustSpeed, activeField->gustStrength,
                                                  activeField->swaySpeed);
        params.swayParams        = hlslpp::float4(activeField->swayStrength, static_cast<float>(activeField->seed & 0x00ffffffu), 0.0f, 0.0f);
        params.speciesHeight     = hlslpp::float4(activeField->species[0].height, activeField->species[1].height,
                                                  activeField->species[2].height, 0.0f);
        params.speciesWidthScale = hlslpp::float4(activeField->species[0].widthScale, activeField->species[1].widthScale,
                                                  activeField->species[2].widthScale, 0.0f);
        params.playerParams      = hlslpp::float4(playerPos.x, playerPos.y, playerPos.z, pushRadius);
        params.fadeParams        = hlslpp::float4(0.0f, activeField->playerPushStrength, 0.0f, 0.0f);

        //--------------------------------------------------------------
        // 草むら（塊）。
        // 頂点シェーダーは周囲3x3の粗セルしか塊を探さないので、セルの一辺を
        // 「塊が届く最大距離」に合わせる。これより小さいと2セル先の塊が届いてしまい、
        // セルの境界で塊が直線的に切れる。そのためユーザー設定にはせずここで決める
        //--------------------------------------------------------------
        float clumpRadiusMin = std::max(activeField->clumpRadiusMin, 1.0f);
        float clumpRadiusMax = std::max(activeField->clumpRadiusMax, 1.0f);
        if(clumpRadiusMin > clumpRadiusMax)
            std::swap(clumpRadiusMin, clumpRadiusMax);

        const float clumpShapeNoise = std::clamp(activeField->clumpShapeNoise, 0.0f, 0.5f);
        const float clumpCellSize   = clumpRadiusMax * (1.0f + clumpShapeNoise);

        params.clumpParams      = hlslpp::float4(clumpCellSize, clumpRadiusMin, clumpRadiusMax, std::clamp(activeField->clumpSpawnChance, 0.0f, 1.0f));
        params.clumpShapeParams = hlslpp::float4(clumpShapeNoise, std::clamp(activeField->clumpEdgeSoftness, 0.01f, 1.0f),
                                                 std::clamp(activeField->fillerDensity, 0.0f, 1.0f),
                                                 std::max(activeField->fillerHeightScale, 0.0f));

        //--------------------------------------------------------------
        // 遠くほど塊の隙間を埋める距離帯。smoothstepの両端が同じだと
        // シェーダー側で0除算になるので、最低1だけ幅を持たせる
        //--------------------------------------------------------------
        const float horizonFillStart = std::max(activeField->horizonFillStart, 0.0f);
        const float horizonFillEnd   = std::max(activeField->horizonFillEnd, horizonFillStart + 1.0f);

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

        std::shared_ptr<Tsukino::Renderer::PipelineState> pipeline = ctx->renderer->GetResources().GetPipelineFactory()->Create(
            *vsAsset, *psAsset, Tsukino::GraphicsCommon::VertexFormat::PositionNormalUV, Tsukino::Renderer::DepthMode::ReadWrite,
            Tsukino::Renderer::BlendMode::Opaque);

        if(!pipeline)
            return;

        Tsukino::Renderer::Material& material = ctx->renderer->GetDrawQueue().AllocMaterial();
        material.SetPipeline(pipeline.get());
        material.SetSampler(ctx->renderer->GetResources().GetSampler(Tsukino::GraphicsCommon::SamplerType::LinearClamp));

        // アルベドに種ごとの根元→先端グラデーションを差す。頂点シェーダーが
        // 選んだ種に応じてUVのvを自分の帯へずらして出すので、これだけで
        // 種ごとの色分けが出る
        ID3D11ShaderResourceView* gradientSRV = GetGradientSRV(*ctx, activeField->species);
        material.SetTexture(Tsukino::Renderer::SRVSlot::Albedo, gradientSRV ? gradientSRV : ctx->renderer->GetResources().GetWhiteTextureSRV());

        // ノーマルマップは使わない。フラット法線を入れると頂点法線がそのまま残る
        material.SetTexture(Tsukino::Renderer::SRVSlot::Normal, ctx->renderer->GetResources().GetFlatNormalTextureSRV());
        material.SetTexture(Tsukino::Renderer::SRVSlot::MetallicRoughness, ctx->renderer->GetResources().GetWhiteTextureSRV());
        material.SetTexture(Tsukino::Renderer::SRVSlot::Emissive, ctx->renderer->GetResources().GetWhiteTextureSRV());
        material.SetTexture(Tsukino::Renderer::SRVSlot::AO, ctx->renderer->GetResources().GetWhiteTextureSRV());

        //--------------------------------------------------------------
        // マテリアル定数。草は金属ではないので metallic は0、
        // 表面はざらついているので roughness は高め
        //--------------------------------------------------------------
        Tsukino::Renderer::CBufferMaterial& materialData = ctx->renderer->GetDrawQueue().AllocMaterialData();
        materialData            = Tsukino::Renderer::CBufferMaterial{};
        materialData.baseColor  = hlslpp::float4(1.0f, 1.0f, 1.0f, 1.0f);
        materialData.emissive   = hlslpp::float3(0.0f, 0.0f, 0.0f);
        materialData.metallic   = 0.0f;
        materialData.roughness  = 0.85f;
        materialData.specular   = 0.2f;
        materialData.rimColor   = hlslpp::float4(0.0f, 0.0f, 0.0f, 0.0f);
        materialData.rimParams  = hlslpp::float4(1.0f, 0.0f, 0.0f, 0.0f);    // z=alphaCutoff。草はくり抜かないので0

        //--------------------------------------------------------------
        // 層ごとにパラメータを流し込み、描画コマンドを1本ずつ積む。
        // 描画コマンドはバッファを指しているだけで、実際の描画は後でまとめて行われる。
        // 1本のバッファを書き換えながら2回積むと両方とも最後の値で描かれてしまうので、
        // バッファは層ごとに分けて持つ。エンジンは草を知らないので、バッファもこちらで持つ。
        //
        // 位置も向きも頂点シェーダーがワールド座標で直接組み立てるため、
        // モデル行列は単位行列でよい
        //--------------------------------------------------------------
        for(Tsukino::u32 layerIndex = 0; layerIndex < layerCount; ++layerIndex) {
            const GrassLayer&                      layer  = layers[layerIndex];
            Tsukino::Renderer::UserConstantBuffer& buffer = m_paramBuffers[layerIndex];

            if(!buffer.IsValid()) {
                buffer = Tsukino::Renderer::CreateUserConstantBuffer(ctx->renderer->GetDevice(), sizeof(CBufferGrass));

                if(!buffer.IsValid()) {
                    Tsukino::Core::Log::Error("GrassFieldSystem - failed to create the grass constant buffer. Grass will not be drawn.");
                    return;
                }
            }

            params.fieldParams    = hlslpp::float4(layer.fieldSize, layer.grid.gridDim, static_cast<float>(layer.grid.perCell), m_time);
            params.lodParams      = hlslpp::float4(layer.lodStart, layer.lodEnd, layer.layerIndex, layer.fadeStart);
            params.coverageParams = hlslpp::float4(horizonFillStart, horizonFillEnd, nearHalf, layer.widthCompensation);

            Tsukino::Renderer::UpdateUserConstantBuffer(ctx->renderer->GetContext(), buffer, &params, sizeof(params));

            Tsukino::Renderer::DrawCommand cmd{};
            cmd.mesh          = &m_bladeMesh;
            cmd.material      = &material;
            cmd.materialData  = &materialData;
            cmd.transform     = Tsukino::Core::Math::matrix::identity();
            cmd.prevTransform = cmd.transform;
            cmd.hasPrevFrame  = false;
            cmd.pass          = Tsukino::Renderer::RenderPass::GBuffer;
            cmd.instanceCount = layer.grid.drawCount;

            // 影は落とさない。シャドウパスは固定のシャドウ用シェーダーで描き直すため、
            // 草のように頂点シェーダーが位置を組み立てるものは全インスタンスが
            // 原点へ重なった状態で描かれてしまう（影を落とさせるには専用の
            // シャドウ用頂点シェーダーを差し替えられるようにする必要がある）
            cmd.castsShadow = false;

            // 草のパラメータをゲーム予約枠へ渡す
            cmd.userConstantBuffer = buffer.buffer.Get();
            cmd.userConstantSlot   = Tsukino::Renderer::CBSlot::User0;

            ctx->renderer->GetDrawQueue().Push(cmd);
        }
    }
}    // namespace CombatAndroid::ECS
