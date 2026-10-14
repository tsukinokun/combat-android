//-------------------------------------------------------------
//! @file    ProceduralTree.cpp
//! @brief   木の形をプログラムで組み立てる関数の実装
//-------------------------------------------------------------
#include <CombatAndroid/ECS/Utility/World/ProceduralTree.hpp>

#include <Tsukino/GraphicsCommon/Vertex/VertexPNUV.hpp>
#include <Tsukino/GraphicsCommon/Vertex/VertexFormat.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <execution>
#include <limits>
#include <vector>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    namespace {
        constexpr float kPi         = 3.14159265f;
        constexpr float kGoldenAngle = 2.39996323f;    //!< 黄金角（ラジアン）。子の枝を回す向きに使い、枝が同じ方向へ重ならないようにする

        //-------------------------------------------------------------
        //! @struct Vec3
        //! @brief  形を組み立てる間だけ使う、軽い3次元ベクトル
        //-------------------------------------------------------------
        struct Vec3 {
            float x = 0.0f;
            float y = 0.0f;
            float z = 0.0f;
        };

        Vec3  operator+(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
        Vec3  operator-(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
        Vec3  operator*(Vec3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
        float Dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
        Vec3  Cross(Vec3 a, Vec3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
        float Length(Vec3 a) { return std::sqrt(Dot(a, a)); }

        Vec3 Normalize(Vec3 a) {
            const float length = Length(a);
            return (length > 1.0e-6f) ? a * (1.0f / length) : Vec3{0.0f, 1.0f, 0.0f};
        }

        //-------------------------------------------------------------
        //! @class  Rng
        //! @brief  種から決まる乱数（線形合同法）。同じ種なら同じ木になる
        //-------------------------------------------------------------
        class Rng {
        public:
            explicit Rng(std::uint32_t seed) : m_state(seed * 2654435761u + 1u) {}

            //! 0〜1の乱数
            float Next01() {
                m_state = m_state * 1664525u + 1013904223u;
                return static_cast<float>(m_state >> 8) / static_cast<float>(1u << 24);
            }

            //! a〜bの乱数
            float Range(float a, float b) { return a + (b - a) * Next01(); }

            //! a〜b（両端を含む）の整数の乱数
            int RangeInt(int a, int b) {
                if(b <= a)
                    return a;
                return std::min(a + static_cast<int>(Next01() * static_cast<float>(b - a + 1)), b);
            }

            //! 各成分が-1〜1のベクトル
            Vec3 NextVector() { return {Range(-1.0f, 1.0f), Range(-1.0f, 1.0f), Range(-1.0f, 1.0f)}; }

        private:
            std::uint32_t m_state;
        };

        //-------------------------------------------------------------
        //! @struct MeshBuilder
        //! @brief  頂点と面を積んでいく入れ物
        //-------------------------------------------------------------
        struct MeshBuilder {
            std::vector<Tsukino::GraphicsCommon::VertexPNUV> vertices;
            std::vector<std::uint32_t>                       indices;

            std::uint32_t Add(Vec3 position, Vec3 normal, float u, float v = 0.0f) {
                Tsukino::GraphicsCommon::VertexPNUV vertex{};
                vertex.position = {position.x, position.y, position.z};
                vertex.normal   = {normal.x, normal.y, normal.z};
                vertex.uv       = {u, v};    // 幹のvは、全部積み終えてから高さの割合で埋める（FillHeightV）
                vertices.push_back(vertex);
                return static_cast<std::uint32_t>(vertices.size() - 1);
            }

            //! 全頂点のYの範囲を得る
            void GetYRange(float& minY, float& maxY) const {
                minY = std::numeric_limits<float>::max();
                maxY = std::numeric_limits<float>::lowest();
                for(const auto& vertex : vertices) {
                    minY = std::min(minY, vertex.position.y);
                    maxY = std::max(maxY, vertex.position.y);
                }
            }

            //! UVのvへ「下0→上1」の高さの割合を入れる
            void FillHeightV(float minY, float maxY) {
                const float range = std::max(maxY - minY, 1.0e-3f);
                for(auto& vertex : vertices)
                    vertex.uv.y = std::clamp((vertex.position.y - minY) / range, 0.0f, 1.0f);
            }

            //! エンジンへ渡すメッシュデータにする
            Tsukino::GraphicsCommon::MeshData ToMeshData() const {
                Tsukino::GraphicsCommon::MeshData mesh;
                mesh.format       = Tsukino::GraphicsCommon::VertexFormat::PositionNormalUV;
                mesh.vertexStride = sizeof(Tsukino::GraphicsCommon::VertexPNUV);
                mesh.vertexCount  = static_cast<Tsukino::u32>(vertices.size());
                mesh.vertexData.resize(vertices.size() * sizeof(Tsukino::GraphicsCommon::VertexPNUV));
                std::memcpy(mesh.vertexData.data(), vertices.data(), mesh.vertexData.size());
                mesh.indices.assign(indices.begin(), indices.end());
                mesh.indexCount = static_cast<Tsukino::u32>(indices.size());
                return mesh;
            }
        };

        //-------------------------------------------------------------
        //! @class  TreeBuilder
        //! @brief  幹から枝を再帰で伸ばし、枝の先に葉の塊を付ける
        //-------------------------------------------------------------
        class TreeBuilder {
        public:
            explicit TreeBuilder(const TreeShapeParams& params) : m_params(params), m_rng(params.seed) {}

            //! 木を1本組み立てる
            TreeMeshes Build() {
                // 主幹はほぼ真上へ。少しだけ傾けて、どの木も同じ立ち姿にならないようにする
                const Vec3 trunkDir = Normalize(Vec3{m_rng.Range(-0.08f, 0.08f), 1.0f, m_rng.Range(-0.08f, 0.08f)});
                Branch({0.0f, 0.0f, 0.0f}, trunkDir, m_params.trunkHeight, m_params.trunkRadius, 0, m_rng.Range(0.0f, 2.0f * kPi));

                for(const Vec3& tip : m_tips)
                    AddCanopyAt(tip);

                TreeMeshes result;
                result.trunkRadius = m_params.trunkRadius;

                float trunkMinY = 0.0f, trunkMaxY = 0.0f, canopyMinY = 0.0f, canopyMaxY = 0.0f;
                m_trunk.GetYRange(trunkMinY, trunkMaxY);
                m_canopy.GetYRange(canopyMinY, canopyMaxY);
                m_trunk.FillHeightV(0.0f, trunkMaxY);

                result.trunk  = m_trunk.ToMeshData();
                result.canopy = m_canopy.ToMeshData();
                result.height = std::max(trunkMaxY, m_canopy.vertices.empty() ? 0.0f : canopyMaxY);

                //-------------------------------------------------------------
                // 囲む箱。影の範囲の判定に使われるので、幹と葉のどちらにも木全体の箱を入れる
                //-------------------------------------------------------------
                Vec3 minP{std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};
                Vec3 maxP{std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest()};
                for(const MeshBuilder* builder : {&m_trunk, &m_canopy}) {
                    for(const auto& vertex : builder->vertices) {
                        minP = {std::min(minP.x, vertex.position.x), std::min(minP.y, vertex.position.y), std::min(minP.z, vertex.position.z)};
                        maxP = {std::max(maxP.x, vertex.position.x), std::max(maxP.y, vertex.position.y), std::max(maxP.z, vertex.position.z)};
                    }
                }
                for(Tsukino::GraphicsCommon::MeshData* mesh : {&result.trunk, &result.canopy}) {
                    mesh->bounds.min.x = minP.x;
                    mesh->bounds.min.y = minP.y;
                    mesh->bounds.min.z = minP.z;
                    mesh->bounds.max.x = maxP.x;
                    mesh->bounds.max.y = maxP.y;
                    mesh->bounds.max.z = maxP.z;
                }
                return result;
            }

        private:
            //-------------------------------------------------------------
            //! @brief  枝を1本伸ばし、子の枝を出す（再帰）
            //! @param  origin  [in] 根元の位置
            //! @param  dir     [in] 伸びる向き（正規化済み）
            //! @param  length  [in] 長さ
            //! @param  radius  [in] 根元の半径
            //! @param  depth   [in] 深さ（主幹が0）
            //! @param  azimuth [in] 子の枝を出し始める向き（親の向きの周りの角度）
            //-------------------------------------------------------------
            void Branch(Vec3 origin, Vec3 dir, float length, float radius, int depth, float azimuth) {
                const int   segments   = std::max(m_params.lengthSegments, 1);
                const int   radial     = std::max(m_params.radialSegments, 3);
                const bool  isTip      = depth >= m_params.branchDepth;
                const float endRatio   = isTip ? 0.3f : m_params.radiusRatio * 0.95f;    // 先へ行くほど細らせる
                const float segmentLen = length / static_cast<float>(segments);

                //-------------------------------------------------------------
                // 断面の向き（枝に垂直な2軸）。節ごとに曲げても断面がねじれないよう、
                // 前の節の軸を新しい向きへ直交させて使い回す
                //-------------------------------------------------------------
                const Vec3 reference = (std::abs(dir.y) < 0.99f) ? Vec3{0.0f, 1.0f, 0.0f} : Vec3{1.0f, 0.0f, 0.0f};
                Vec3       side      = Normalize(Cross(reference, dir));
                Vec3       forward   = Cross(dir, side);

                std::vector<Vec3>  ringCenters;
                std::vector<Vec3>  ringDirs;
                std::vector<float> ringRadii;

                const std::uint32_t firstRingVertex = static_cast<std::uint32_t>(m_trunk.vertices.size());

                Vec3 position = origin;
                Vec3 heading  = dir;
                for(int ring = 0; ring <= segments; ++ring) {
                    const float t = static_cast<float>(ring) / static_cast<float>(segments);

                    // 先細り。主幹の根元だけは少し広げて、地面に根を張ったように見せる
                    float ringRadius = radius * (1.0f + (endRatio - 1.0f) * t);
                    if(depth == 0)
                        ringRadius *= 1.0f + 0.4f * std::pow(1.0f - t, 4.0f);

                    for(int k = 0; k <= radial; ++k) {
                        const float angle  = 2.0f * kPi * static_cast<float>(k) / static_cast<float>(radial);
                        const Vec3  normal = side * std::cos(angle) + forward * std::sin(angle);
                        m_trunk.Add(position + normal * ringRadius, normal, static_cast<float>(k) / static_cast<float>(radial));
                    }

                    ringCenters.push_back(position);
                    ringDirs.push_back(heading);
                    ringRadii.push_back(ringRadius);

                    if(ring == segments)
                        break;

                    // 次の節へ。少しずつ向きをずらして曲げ、主幹以外は上へ引き寄せる
                    position = position + heading * segmentLen;
                    Vec3 bent = heading + m_rng.NextVector() * m_params.bend;
                    if(depth > 0)
                        bent = bent + Vec3{0.0f, 1.0f, 0.0f} * (m_params.upwardPull * 0.35f);
                    heading = Normalize(bent);
                    side    = Normalize(side - heading * Dot(side, heading));
                    forward = Cross(heading, side);
                }

                // 節と節の間を四角形でつなぐ
                const std::uint32_t ringSize = static_cast<std::uint32_t>(radial + 1);
                for(int ring = 0; ring < segments; ++ring) {
                    const std::uint32_t a = firstRingVertex + static_cast<std::uint32_t>(ring) * ringSize;
                    const std::uint32_t b = a + ringSize;
                    for(int k = 0; k < radial; ++k) {
                        const std::uint32_t k0 = static_cast<std::uint32_t>(k);
                        m_trunk.indices.insert(m_trunk.indices.end(), {a + k0, b + k0, a + k0 + 1, a + k0 + 1, b + k0, b + k0 + 1});
                    }
                }

                if(isTip) {
                    m_tips.push_back(ringCenters.back());
                    return;
                }

                //-------------------------------------------------------------
                // 子の枝。枝の上の方（先端を含む）から出し、親の向きから spreadAngle だけ開く。
                // 出す向きは黄金角ずつ回して、子同士が同じ側へ重ならないようにする
                //-------------------------------------------------------------
                const int childCount = m_rng.RangeInt(m_params.childCountMin, m_params.childCountMax);
                for(int child = 0; child < childCount; ++child) {
                    const bool  fromTop  = child == 0;    // 1本目は先端から出して、幹の流れを先へ続ける
                    const float attachT  = fromTop ? 1.0f : m_rng.Range(0.55f, 0.95f);
                    const int   ringIdx  = std::clamp(static_cast<int>(std::round(attachT * static_cast<float>(segments))), 0, segments);
                    const Vec3  parent   = ringDirs[static_cast<size_t>(ringIdx)];
                    const float childAz  = azimuth + kGoldenAngle * static_cast<float>(child) + m_rng.Range(-0.4f, 0.4f);
                    const float spread   = m_params.spreadAngle * (fromTop ? 0.45f : 1.0f) * m_rng.Range(0.8f, 1.2f);

                    const Vec3 ref   = (std::abs(parent.y) < 0.99f) ? Vec3{0.0f, 1.0f, 0.0f} : Vec3{1.0f, 0.0f, 0.0f};
                    const Vec3 axisA = Normalize(Cross(ref, parent));
                    const Vec3 axisB = Cross(parent, axisA);
                    const Vec3 perp  = axisA * std::cos(childAz) + axisB * std::sin(childAz);

                    Vec3 childDir = parent * std::cos(spread) + perp * std::sin(spread);
                    childDir      = Normalize(childDir + Vec3{0.0f, 1.0f, 0.0f} * m_params.upwardPull);

                    const float childLength = length * m_params.lengthRatio * m_rng.Range(0.85f, 1.1f);
                    const float childRadius = ringRadii[static_cast<size_t>(ringIdx)] * (fromTop ? 0.85f : m_params.radiusRatio / 0.95f);

                    // 親の芯から少し内側に食い込ませて生やし、付け根に隙間が見えないようにする
                    Branch(ringCenters[static_cast<size_t>(ringIdx)] - childDir * (childRadius * 0.5f), childDir, childLength,
                           std::min(childRadius, ringRadii[static_cast<size_t>(ringIdx)]), depth + 1, childAz);
                }
            }

            //-------------------------------------------------------------
            //! @brief  枝の先に葉の茂みを付ける
            //! @note   葉の絵を貼った小さな板（葉カード）を、茂みの形（少しつぶした球）の中へ散らす。
            //!         板は茂みの中心から外向きを大まかに向くよう立て、向きは乱数でゆらしてばらつかせる。
            //!         法線は板の向きではなく「茂みの中心から外向き」にする。板ごとの陰影がばらばらに
            //!         ならず、茂み全体が上は明るく下と奥は暗い、ふんわりした塊に見える（葉の定番の手）。
            //!         板の裏から見ても法線が同じなので、裏表で陰影が反転することもない
            //-------------------------------------------------------------
            void AddCanopyAt(Vec3 tip) {
                // 茂みの中心は枝先より少し上。枝の先を茂みの中へ隠す
                const Vec3  center = tip + Vec3{0.0f, m_params.canopyRadius * 0.25f, 0.0f} + m_rng.NextVector() * (m_params.canopyRadius * 0.15f);
                const float radius = m_params.canopyRadius * m_rng.Range(0.85f, 1.15f);

                for(int card = 0; card < std::max(m_params.leafCardsPerTip, 1); ++card) {
                    //-------------------------------------------------------------
                    // 置く位置。中心からの向きは一様に、距離は外寄りにして、表面に葉が多い茂みにする
                    // （中まで詰めると、外から見えない葉にまで手間をかけることになる）
                    //-------------------------------------------------------------
                    Vec3 direction = Normalize(m_rng.NextVector());
                    const float distance = radius * (0.35f + 0.65f * std::cbrt(m_rng.Next01()));
                    Vec3 offset    = direction * distance;
                    offset.y      *= m_params.canopySquash;
                    const Vec3 position = center + offset;

                    // 板の向き（板の面の法線）。外向きを乱数でゆらす
                    const Vec3 facing = Normalize(direction + m_rng.NextVector() * 0.7f);
                    const Vec3 ref    = (std::abs(facing.y) < 0.95f) ? Vec3{0.0f, 1.0f, 0.0f} : Vec3{1.0f, 0.0f, 0.0f};
                    Vec3       axisU  = Normalize(Cross(ref, facing));
                    Vec3       axisV  = Cross(facing, axisU);

                    // 板を面の中で回して、葉の向きを揃えない
                    const float roll = m_rng.Range(0.0f, 2.0f * kPi);
                    const Vec3  rotU = axisU * std::cos(roll) + axisV * std::sin(roll);
                    const Vec3  rotV = Cross(facing, rotU);

                    const float half = m_params.leafCardSize * 0.5f * m_rng.Range(0.75f, 1.25f);

                    // アトラス（2×2）のどのマスの絵を使うか
                    const float tileU = static_cast<float>(m_rng.RangeInt(0, 1)) * 0.5f;
                    const float tileV = static_cast<float>(m_rng.RangeInt(0, 1)) * 0.5f;

                    //-------------------------------------------------------------
                    // 4隅を積む。法線は茂みの中心から外向き（少し上へ寄せて、てっぺんをより明るく）
                    //-------------------------------------------------------------
                    const float cornerU[4] = {-1.0f, 1.0f, 1.0f, -1.0f};
                    const float cornerV[4] = {-1.0f, -1.0f, 1.0f, 1.0f};
                    std::uint32_t indices[4];
                    for(int i = 0; i < 4; ++i) {
                        const Vec3 corner = position + rotU * (cornerU[i] * half) + rotV * (cornerV[i] * half);
                        const Vec3 normal = Normalize(Normalize(corner - center) + Vec3{0.0f, 0.35f, 0.0f});
                        indices[i]        = m_canopy.Add(corner, normal, tileU + (cornerU[i] * 0.5f + 0.5f) * 0.5f, tileV + (cornerV[i] * 0.5f + 0.5f) * 0.5f);
                    }
                    m_canopy.indices.insert(m_canopy.indices.end(), {indices[0], indices[1], indices[2], indices[0], indices[2], indices[3]});
                }
            }

            const TreeShapeParams& m_params;
            Rng                    m_rng;
            MeshBuilder            m_trunk;
            MeshBuilder            m_canopy;
            std::vector<Vec3>      m_tips;    //!< 葉の塊を付ける枝の先
        };
    }    // namespace

    //-------------------------------------------------------------
    //! @brief  木を1本組み立てる
    //-------------------------------------------------------------
    TreeMeshes BuildProceduralTree(const TreeShapeParams& params) {
        TreeBuilder builder(params);
        return builder.Build();
    }

    //-------------------------------------------------------------
    //! @brief  葉テクスチャのピクセルを作る
    //-------------------------------------------------------------
    std::vector<std::uint8_t> BuildLeafAtlasPixels(const LeafAtlasParams& params) {
        const int size = std::max(params.size, 8);
        const int tile = size / 2;

        std::vector<std::uint8_t> pixels(static_cast<size_t>(size) * size * 4, 0);
        Rng                       rng(params.seed);

        for(int tileY = 0; tileY < 2; ++tileY) {
            for(int tileX = 0; tileX < 2; ++tileX) {
                for(int leaf = 0; leaf < std::max(params.leavesPerTile, 1); ++leaf) {
                    //-------------------------------------------------------------
                    // 葉1枚。マスの中に中心を置き、長さ・幅・向きを乱数で決める。
                    // マスの縁で切れないよう、葉がマスからはみ出る分は描かない（隣のマスへ漏らさない）
                    //-------------------------------------------------------------
                    const float length = static_cast<float>(tile) * rng.Range(0.42f, 0.62f);
                    const float width  = length * rng.Range(0.32f, 0.42f);
                    const float angle  = rng.Range(0.0f, 2.0f * kPi);
                    const float margin = length * 0.3f;
                    const float cx     = static_cast<float>(tileX * tile) + rng.Range(margin, static_cast<float>(tile) - margin);
                    const float cy     = static_cast<float>(tileY * tile) + rng.Range(margin, static_cast<float>(tile) - margin);
                    const float c      = std::cos(angle);
                    const float s      = std::sin(angle);

                    // 葉ごとの色（暗い側〜明るい側）
                    const float shade = rng.Next01();
                    float       color[3];
                    for(int k = 0; k < 3; ++k)
                        color[k] = params.darkColor[k] + (params.lightColor[k] - params.darkColor[k]) * shade;

                    const int x0 = std::max(tileX * tile, static_cast<int>(cx - length));
                    const int x1 = std::min((tileX + 1) * tile - 1, static_cast<int>(cx + length));
                    const int y0 = std::max(tileY * tile, static_cast<int>(cy - length));
                    const int y1 = std::min((tileY + 1) * tile - 1, static_cast<int>(cy + length));

                    for(int y = y0; y <= y1; ++y) {
                        for(int x = x0; x <= x1; ++x) {
                            // 葉の向きに合わせた座標（along: 葉の長さ方向 -1〜1、across: 幅方向）
                            const float dx     = static_cast<float>(x) + 0.5f - cx;
                            const float dy     = static_cast<float>(y) + 0.5f - cy;
                            const float along  = (dx * c + dy * s) / (length * 0.5f);
                            const float across = (-dx * s + dy * c) / (width * 0.5f);
                            if(std::abs(along) >= 1.0f)
                                continue;

                            // 両端が尖った葉の輪郭（中ほどが一番広い）
                            const float halfWidth = std::pow(1.0f - along * along, 0.7f);
                            if(std::abs(across) >= halfWidth)
                                continue;

                            // 葉脈（中心線）と縁を少し暗くし、付け根側（along<0）もわずかに暗くする
                            const float edge  = std::abs(across) / halfWidth;
                            float       light = 0.85f + 0.15f * (1.0f - edge * edge);
                            if(std::abs(across) < 0.08f)
                                light *= 0.8f;
                            light *= 0.9f + 0.1f * (along * 0.5f + 0.5f);

                            const size_t offset = (static_cast<size_t>(y) * size + x) * 4;
                            for(int k = 0; k < 3; ++k)
                                pixels[offset + k] = static_cast<std::uint8_t>(std::clamp(color[k] * light, 0.0f, 1.0f) * 255.0f + 0.5f);
                            pixels[offset + 3] = 255;
                        }
                    }
                }
            }
        }
        return pixels;
    }

    namespace {
        //-------------------------------------------------------------
        //! @brief  格子点の整数座標から0〜1の乱数を作る（同じ座標・種なら毎回同じ値）
        //-------------------------------------------------------------
        float LatticeHash(int x, int y, std::uint32_t seed) {
            std::uint32_t h = static_cast<std::uint32_t>(x) * 374761393u + static_cast<std::uint32_t>(y) * 668265263u + seed * 2246822519u;
            h               = (h ^ (h >> 13)) * 1274126177u;
            h ^= h >> 16;
            return static_cast<float>(h & 0xffffffu) / static_cast<float>(0xffffff);
        }

        //-------------------------------------------------------------
        //! @brief  横に周期を持つバリューノイズ（0〜1）
        //! @param  x       [in] 横の座標（格子1マス=1）
        //! @param  y       [in] 縦の座標
        //! @param  periodX [in] 横の周期（格子の数）。x と x+periodX は同じ値になる
        //! @note   幹は1周でuが0→1に戻るので、横は周期で作らないと継ぎ目に線が出る
        //-------------------------------------------------------------
        float PeriodicNoise(float x, float y, int periodX, std::uint32_t seed) {
            const float fx = std::floor(x);
            const float fy = std::floor(y);
            const int   ix = static_cast<int>(fx);
            const int   iy = static_cast<int>(fy);
            float       tx = x - fx;
            float       ty = y - fy;
            tx             = tx * tx * (3.0f - 2.0f * tx);
            ty             = ty * ty * (3.0f - 2.0f * ty);

            const int   period = std::max(periodX, 1);
            auto        wrap   = [period](int v) { return ((v % period) + period) % period; };
            const float a      = LatticeHash(wrap(ix), iy, seed);
            const float b      = LatticeHash(wrap(ix + 1), iy, seed);
            const float c      = LatticeHash(wrap(ix), iy + 1, seed);
            const float d      = LatticeHash(wrap(ix + 1), iy + 1, seed);
            return (a + (b - a) * tx) + ((c + (d - c) * tx) - (a + (b - a) * tx)) * ty;
        }

        //-------------------------------------------------------------
        //! @brief  横に周期を持つノイズを数オクターブ重ねたもの（0〜1）
        //-------------------------------------------------------------
        float PeriodicFbm(float x, float y, int periodX, std::uint32_t seed, int octaves) {
            float sum = 0.0f, amp = 0.5f, norm = 0.0f;
            for(int o = 0; o < octaves; ++o) {
                const int scale = 1 << o;
                sum += PeriodicNoise(x * static_cast<float>(scale), y * static_cast<float>(scale), periodX * scale, seed + static_cast<std::uint32_t>(o) * 101u) * amp;
                norm += amp;
                amp *= 0.5f;
            }
            return sum / norm;
        }

        //-------------------------------------------------------------
        //! @struct CellHit
        //! @brief  ボロノイ（セル模様）の1点ぶんの結果
        //-------------------------------------------------------------
        struct CellHit {
            float f1 = 0.0f;    //!< 一番近い種点までの距離
            float f2 = 0.0f;    //!< 二番目に近い種点までの距離（f2-f1 がセルの境目からの距離の目安）
            float id = 0.0f;    //!< 一番近いセルの乱数（0〜1。セルごとの色のばらつきに使う）
        };

        //-------------------------------------------------------------
        //! @brief  横に周期を持つボロノイ
        //! @param  stretchY [in] 縦の距離に掛ける倍率（1未満で縦に長いセルになる）
        //! @note   樹皮の板（裂け目に囲まれた菱形の塊）を作るのに使う。境目が裂け目になり、
        //!         裂け目どうしが分かれたり合流したりする網目になる（平行な縦線より実物に近い）
        //-------------------------------------------------------------
        CellHit PeriodicVoronoi(float x, float y, int periodX, std::uint32_t seed, float stretchY) {
            const int ix     = static_cast<int>(std::floor(x));
            const int iy     = static_cast<int>(std::floor(y));
            const int period = std::max(periodX, 1);
            CellHit   hit;
            hit.f1 = hit.f2 = 1e9f;
            for(int dy = -1; dy <= 1; ++dy) {
                for(int dx = -1; dx <= 1; ++dx) {
                    const int   cx = ix + dx;
                    const int   cy = iy + dy;
                    const int   wx = ((cx % period) + period) % period;
                    const float sx = static_cast<float>(cx) + 0.1f + 0.8f * LatticeHash(wx, cy, seed);
                    const float sy = static_cast<float>(cy) + 0.1f + 0.8f * LatticeHash(wx, cy, seed + 7u);
                    const float ddx = x - sx;
                    const float ddy = (y - sy) * stretchY;
                    const float d   = std::sqrt(ddx * ddx + ddy * ddy);
                    if(d < hit.f1) {
                        hit.f2 = hit.f1;
                        hit.f1 = d;
                        hit.id = LatticeHash(wx, cy, seed + 9u);
                    } else if(d < hit.f2) {
                        hit.f2 = d;
                    }
                }
            }
            return hit;
        }

        [[nodiscard]]
        float Saturate01(float v) {
            return std::clamp(v, 0.0f, 1.0f);
        }

        [[nodiscard]]
        float SmoothStep(float edge0, float edge1, float x) {
            const float t = Saturate01((x - edge0) / (edge1 - edge0));
            return t * t * (3.0f - 2.0f * t);
        }

        [[nodiscard]]
        std::uint8_t ToByte(float v) {
            return static_cast<std::uint8_t>(Saturate01(v) * 255.0f + 0.5f);
        }
    }    // namespace

    //-------------------------------------------------------------
    //! @brief  樹皮テクスチャのピクセルを作る
    //-------------------------------------------------------------
    BarkPixels BuildBarkPixels(const BarkTextureParams& params) {
        const int           width   = std::max(params.width, 8);
        const int           height  = std::max(params.height, 8);
        const int           ridges  = std::max(params.ridgeCount, 1);
        const int           plates  = std::max(params.plateCount, 1);
        const std::uint32_t seed    = params.seed;
        const size_t        texels  = static_cast<size_t>(width) * height;

        //-------------------------------------------------------------
        // 行ごとに独立して計算できるので、行を並列に回す（1回きりの生成だが、
        // 大きいテクスチャを1スレッドで作るとシーン開始時に目立つ止まりになるため）。
        // 各行は自分の行のピクセルにしか書かないので、書き込みは競合しない
        //-------------------------------------------------------------
        std::vector<int> rows(static_cast<size_t>(height));
        for(int y = 0; y < height; ++y)
            rows[static_cast<size_t>(y)] = y;

        //-------------------------------------------------------------
        // 1) 高さ（0=裂け目の底、1=板の頂）。座標は「横=幹1周でridges、縦=木の高さ全体でplates」
        //-------------------------------------------------------------
        std::vector<float> heightField(texels);
        std::vector<float> mossNoise(texels);
        std::vector<float> tintNoise(texels);
        std::vector<float> plateTone(texels);      // 板ごとの乱数（板ごとに色を少しずつ変える）
        std::vector<float> lichenNoise(texels);    // 地衣類（白っぽい斑点）の出方
        std::for_each(std::execution::par, rows.begin(), rows.end(), [&](int y) {
            const float v  = (static_cast<float>(y) + 0.5f) / static_cast<float>(height);
            const float py = v * static_cast<float>(plates);
            for(int x = 0; x < width; ++x) {
                const float u  = (static_cast<float>(x) + 0.5f) / static_cast<float>(width);
                const float px = u * static_cast<float>(ridges);

                //-------------------------------------------------------------
                // 座標を少しゆがませる。セルの境目（裂け目）が直線にならず、うねった線になる
                //-------------------------------------------------------------
                const float warpX = (PeriodicFbm(px * 1.5f, py * 0.6f, ridges * 2, seed + 11u, 2) - 0.5f) * 0.45f;
                const float warpY = (PeriodicFbm(px * 1.5f, py * 0.6f, ridges * 2, seed + 12u, 2) - 0.5f) * 0.45f;
                const float wx    = px + warpX;
                const float wy    = py + warpY;

                //-------------------------------------------------------------
                // 深い裂け目：粗いセル（縦長）の境目。分かれたり合流したりする網目になる
                // 浅い割れ：細かいセルの境目。板の面を小さく割る
                // 横は周期（ridges・ridges*2）なので、幹を1周しても継ぎ目が出ない
                //-------------------------------------------------------------
                const CellHit coarse = PeriodicVoronoi(wx, wy * 0.3f, ridges, seed + 13u, 0.45f);
                const CellHit fine   = PeriodicVoronoi(wx * 2.0f, wy * 0.7f, ridges * 2, seed + 14u, 0.5f);
                const float   crackW = 0.10f + 0.14f * PeriodicNoise(px * 1.2f, py * 0.4f, ridges, seed + 15u);    // 裂け目の幅も場所で変える
                const float   deep   = SmoothStep(0.01f, crackW, coarse.f2 - coarse.f1);
                const float   shallow = SmoothStep(0.0f, 0.07f, fine.f2 - fine.f1);

                // 板は真ん中ほど少し盛り上がる（板の縁が裂け目へ向かって丸く落ちる）
                const float plateRound = 1.0f - std::min(coarse.f1, 1.0f) * 0.35f;
                float       h          = deep * (0.78f + 0.22f * shallow) * (0.75f + 0.25f * plateRound);

                //-------------------------------------------------------------
                // 表面の質感。縦に引き伸ばしたノイズで木の繊維の筋を、細かいノイズでざらつきを、
                // 段差状のノイズで薄く剥がれかけた皮の層を出す
                //-------------------------------------------------------------
                const float fiber = PeriodicFbm(px * 12.0f, py * 0.8f, ridges * 12, seed + 32u, 3) - 0.5f;
                const float grain = PeriodicFbm(px * 6.0f, py * 5.0f, ridges * 6, seed + 31u, 3) - 0.5f;
                const float layer = PeriodicFbm(px * 3.0f, py * 2.0f, ridges * 3, seed + 33u, 2) * 4.0f;
                const float flake = SmoothStep(0.75f, 0.95f, layer - std::floor(layer)) * 0.12f;    // 層の縁の段
                h                 = Saturate01(h * (1.0f + fiber * 0.45f + grain * 0.3f) - flake * h);

                const size_t index  = static_cast<size_t>(y) * width + x;
                heightField[index]  = h;
                mossNoise[index]    = PeriodicFbm(px * 1.2f, py * 0.9f, ridges, seed + 41u, 3);
                tintNoise[index]    = PeriodicFbm(px * 0.8f, py * 0.5f, ridges, seed + 51u, 2);
                plateTone[index]    = coarse.id;
                lichenNoise[index]  = PeriodicFbm(px * 2.5f, py * 1.5f, ridges * 3, seed + 61u, 3);
            }
        });

        BarkPixels result;
        result.albedo.resize(texels * 4);
        result.normal.resize(texels * 4);
        result.ao.resize(texels * 4);

        std::for_each(std::execution::par, rows.begin(), rows.end(), [&](int y) {
            const float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(height);
            for(int x = 0; x < width; ++x) {
                const size_t index = static_cast<size_t>(y) * width + x;
                const float  h     = heightField[index];

                //-------------------------------------------------------------
                // 2) 色。実物の樹皮は「裂け目の奥＝暗く赤みのある茶」「板＝茶」「風雨にさらされた板の頂＝灰色がかる」
                //    の3色に分かれて見える。高さで3つを混ぜ、板ごと・場所ごとに少しずつばらつかせる
                //-------------------------------------------------------------
                const float tone  = 0.88f + 0.24f * plateTone[index];    // 板ごとの明るさの違い
                const float tint  = 0.9f + 0.2f * tintNoise[index];      // 場所ごとのゆるい色むら
                float       base[3];
                for(int k = 0; k < 3; ++k)
                    base[k] = params.bottomColor[k] + (params.topColor[k] - params.bottomColor[k]) * v;
                const float luma = base[0] * 0.2126f + base[1] * 0.7152f + base[2] * 0.0722f;

                const float warm[3]    = {1.15f, 0.95f, 0.8f};    // 裂け目の奥の赤み
                const float weather    = SmoothStep(0.75f, 1.0f, h) * (0.2f + 0.35f * tintNoise[index]);    // 頂ほど灰色に褪せる
                const float plateBlend = SmoothStep(0.05f, 0.55f, h);
                float       color[3];
                for(int k = 0; k < 3; ++k) {
                    const float crack = base[k] * (1.0f - params.fissureDarkness) * warm[k];
                    float       plate = base[k] * tone * tint;
                    plate             = plate + (luma * 1.2f - plate) * weather * 0.5f;
                    color[k]          = crack + (plate - crack) * plateBlend;
                }

                // 地衣類：板の頂にところどころ白っぽい緑灰色の斑点が付く（苔と同じくmossAmountで消せる）
                const float lichenColor[3] = {0.50f, 0.50f, 0.40f};
                const float lichen = SmoothStep(0.74f, 0.82f, lichenNoise[index]) * SmoothStep(0.55f, 0.9f, h) * 0.35f * params.mossAmount;
                for(int k = 0; k < 3; ++k)
                    color[k] = color[k] + (lichenColor[k] * (0.8f + 0.2f * h) - color[k]) * lichen;

                // 苔：根元ほど、裂け目ほど、ノイズの濃い所ほど乗る。境目はノイズで揺らして直線にしない
                const float mossLine = params.mossHeight * (0.7f + 0.6f * mossNoise[index]);
                float       moss     = 1.0f - SmoothStep(mossLine * 0.5f, mossLine, v);
                moss *= SmoothStep(0.35f, 0.65f, mossNoise[index] + (1.0f - h) * 0.35f);
                moss = Saturate01(moss * params.mossAmount);
                for(int k = 0; k < 3; ++k) {
                    const float mossColor = params.mossColor[k] * (0.75f + 0.5f * h);    // 苔も頂の方が明るい
                    color[k]              = color[k] + (mossColor - color[k]) * moss;
                }

                const size_t offset        = index * 4;
                result.albedo[offset + 0] = ToByte(color[0]);
                result.albedo[offset + 1] = ToByte(color[1]);
                result.albedo[offset + 2] = ToByte(color[2]);
                result.albedo[offset + 3] = 255;

                //-------------------------------------------------------------
                // 3) 法線。高さの傾きから作る（横は周期でつなぐ、縦は端で止める）。
                //    x=u方向、y=v方向（GBuffer.ps.hlslのApplyNormalMapがUVの増える向きを基底にする）
                //-------------------------------------------------------------
                const int   xl = (x + width - 1) % width;
                const int   xr = (x + 1) % width;
                const int   yd = std::max(y - 1, 0);
                const int   yu = std::min(y + 1, height - 1);
                const float du = (heightField[static_cast<size_t>(y) * width + xr] - heightField[static_cast<size_t>(y) * width + xl]) * 0.5f;
                const float dv = (heightField[static_cast<size_t>(yu) * width + x] - heightField[static_cast<size_t>(yd) * width + x]) * 0.5f;
                float       nx = -du * params.normalStrength;
                float       ny = -dv * params.normalStrength;
                float       nz = 1.0f;
                const float len = std::sqrt(nx * nx + ny * ny + nz * nz);
                nx /= len;
                ny /= len;
                nz /= len;
                result.normal[offset + 0] = ToByte(nx * 0.5f + 0.5f);
                result.normal[offset + 1] = ToByte(ny * 0.5f + 0.5f);
                result.normal[offset + 2] = ToByte(nz * 0.5f + 0.5f);
                result.normal[offset + 3] = 255;

                //-------------------------------------------------------------
                // 4) AO。裂け目の奥ほど光が届かない
                //-------------------------------------------------------------
                const std::uint8_t ao = ToByte(1.0f - params.aoStrength * (1.0f - h) * (1.0f - h));
                result.ao[offset + 0]  = ao;
                result.ao[offset + 1]  = ao;
                result.ao[offset + 2]  = ao;
                result.ao[offset + 3]  = 255;
            }
        });
        return result;
    }
}    // namespace CombatAndroid::ECS
