//--------------------------------------------------------------
//! @file   Grass.vs.hlsl
//! @brief  草用頂点シェーダー
//! @author 山﨑愛
//--------------------------------------------------------------
// 9頂点の刃メッシュ1本をインスタンス描画で大量に並べる。
// 1本ごとの位置・向き・高さ・揺れの位相は、その草が生えている
// 「セルのワールド座標」のハッシュから毎フレーム計算するため、
// CPU側にもGPU側にも草1本あたりのメモリを持たない。
//
// ハッシュの種にワールド座標を使うのが最重要で、これにより
// 「同じ場所には常に同じ草が生える」＝カメラが動いても草は地面に
// 固定されたまま、という状態になる。インスタンス番号を種にすると
// カメラの移動に合わせて草が世界を滑ってしまう。
//
// 出力は ModelStatic.vs.hlsl と同じ VSOutput なので、ピクセル
// シェーダーは既存の GBuffer.ps.hlsl をそのまま使える。
// そのおかげでディファードライティング・ポイントライト・影・
// フォグ・モーションブラーが全部そのまま乗る。
#pragma pack_matrix(row_major)

//--------------------------------------------------------------
// 定数バッファ：シーン (b0)
// PBR.hlsliのCBufferSceneと同じ並び。末尾のprevViewProjまで使うので
// 全メンバを宣言する
//--------------------------------------------------------------
cbuffer CBufferScene : register(b0)
{
    matrix view;
    matrix projection;
    matrix viewProj;
    matrix invViewProj;
    matrix lightViewProj;
    float4 lightDir;
    float4 lightColor;
    float4 cameraPos;
    matrix prevViewProj;
    float4 timeParams;      // x: 経過秒, y: 前フレームからの経過秒, z: sin(x), w: cos(x)
    float4 screenParams;    // xy: 解像度, zw: その逆数
    float4 shadowParams;    // x: シャドウマップの一辺, y: その逆数
};

//--------------------------------------------------------------
// 定数バッファ：草 (b12 = CBSlot::User0 のゲーム予約枠)
// CombatAndroid/ECS/System/GrassFieldSystem.hpp の CBufferGrass と
// 1バイト単位で一致させること
//--------------------------------------------------------------
cbuffer CBufferGrass : register(b12)
{
    float4 fieldParams;        // x: フィールドの一辺, y: 1辺のセル数, z: セルあたりの本数, w: 経過時間（秒）
    float4 bladeParams;        // x: 遠くの草の幅の増し分, y: 高さのばらつき, z: 地面の高さ(Y), w: 予約
    float4 windParams;         // xyz: 風向き（正規化済み）, w: 常時なびく強さ
    float4 gustParams;         // x: 突風の波長, y: 突風の速さ, z: 突風の強さ, w: そよぎの角速度
    float4 swayParams;         // x: そよぎの強さ, y: 乱数シード, zw: 予約
    float4 speciesHeight;      // xyz: 種0/1/2の高さ, w: 予約
    float4 speciesWidthScale;  // xyz: 種0/1/2の幅倍率, w: 予約
    float4 playerParams;       // xyz: プレイヤー座標, w: かき分け半径
    float4 fadeParams;         // x: 予約, y: かき分けの強さ, z: 予約, w: 予約
    float4 clumpParams;        // x: 塊の粗セルの一辺, y: 塊の半径の最小, z: 塊の半径の最大, w: 塊が生まれる確率
    float4 clumpShapeParams;   // x: 形の揺らぎ, y: 縁の柔らかさ, z: 塊の外の草の割合, w: 塊の外の草の丈の倍率
    float4 lodParams;          // x: 近景→遠景の切替開始距離, y: 切替終了距離, z: 層（0: 近景, 1: 遠景）, w: 外周で背を縮め始める距離
    float4 coverageParams;     // x: 塊の隙間が埋まり始める距離, y: 埋まりきる距離, z: 幅の増し分が最大になる距離, w: 本数の少なさを補う幅の倍率
};

//--------------------------------------------------------------
// 草の種類数。CombatAndroid/ECS/System/GrassFieldSystem.cpp の
// kSpeciesCount、GetGradientSRVが作るグラデーションテクスチャの段数と
// 一致させること
//--------------------------------------------------------------
static const uint  kSpeciesCount  = 3;
static const float kGradientRows  = 32.0f;    // GrassFieldSystem.cpp の kGradientHeight と一致させること

//--------------------------------------------------------------
// 入力（刃メッシュの頂点）
//--------------------------------------------------------------
// 刃はクロスビルボード（根元の中心線で直交する板2枚）。真横に近い角度から
// 見たときに板が厚み0の線へ潰れて奥の地面が素通しに見えるのを防ぐための構成
// （GrassFieldSystem.cpp の BuildBladeMeshData 参照）。
// 面0はpositionのx（幅）とnormal=(0,0,1)、面1はz（幅）とnormal=(1,0,0)を持ち、
// 頂点1つはどちらか片方の面にしか属さない（もう一方の幅成分は必ず0）
struct VSInput
{
    float3 position : POSITION;    // x: 面0の幅方向(-0.5〜0.5、面1では常に0)
                                    // y: 0〜1 の根元→先端
                                    // z: 面1の幅方向(-0.5〜0.5、面0では常に0)
    float3 normal   : NORMAL;      // 刃の面法線（ローカル）。x=1なら面1、z=1なら面0
    float2 uv       : TEXCOORD0;   // u: 幅方向 0〜1, v: 根元→先端 0〜1
};

//--------------------------------------------------------------
// 出力
// ModelStatic.vs.hlsl の VSOutput と完全に同じにすること。
// これが揃っている限り GBuffer.ps.hlsl を書き換えずに使える
//--------------------------------------------------------------
struct VSOutput
{
    float4 position : SV_POSITION;
    float3 worldPos : POSITION;
    float3 normal   : NORMAL;
    float2 uv       : TEXCOORD0;
    float4 curClip  : TEXCOORD1;
    float4 prevClip : TEXCOORD2;
};

static const float TWO_PI = 6.28318530718f;

//--------------------------------------------------------------
//! 32ビット整数をビット撹拌して別の整数へ写します。
//! @param  [in] x 撹拌する値
//! @return 撹拌後の値
//! @note   連番を入れても隣どうしの相関が残らないことが重要。
//!         AmbientParticle.vs.hlsl と同じ関数
//--------------------------------------------------------------
uint HashU32(uint x)
{
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

//--------------------------------------------------------------
//! 整数から 0.0〜1.0 の擬似乱数を作ります。
//! @param  [in] x 種となる整数
//! @return 0.0以上1.0未満の値
//! @note   下位24ビットだけを使う。floatの仮数部が24ビットなので、
//!         これで割ると分布に偏りが出ない
//--------------------------------------------------------------
float Rand01(uint x)
{
    return float(HashU32(x) & 0x00ffffffu) * (1.0f / 16777216.0f);
}

//--------------------------------------------------------------
//! セルのワールド座標から乱数の種を作ります。
//! @param  [in] cellX セルのX番号（ワールド基準の整数）
//! @param  [in] cellZ セルのZ番号（ワールド基準の整数）
//! @param  [in] slot  セル内の何本目か
//! @return その草に固有の種
//! @note   負の座標でも破綻しないよう、intのビット列をそのまま
//!         uintとして読み替えてから混ぜている。
//!         ここでワールド座標を使うことが「草が地面に固定される」
//!         唯一の理由なので、インスタンス番号に置き換えてはいけない
//--------------------------------------------------------------
uint MakeCellSeed(int cellX, int cellZ, uint slot)
{
    // 遠景の格子はセルの大きさが違うだけで番号の振り方は同じなので、
    // 層ごとに種をずらさないと近景と遠景で草の個性が同じ並びで繰り返される。
    // 近景（層0）では加算がゼロになり、近景の配置は層を足す前と変わらない
    const uint layer = (uint)(lodParams.z + 0.5f);

    uint hx = HashU32(asuint(cellX) * 0x9e3779b9u);
    uint hz = HashU32(asuint(cellZ) * 0x85ebca6bu);
    uint hs = HashU32(slot * 0xc2b2ae35u + (uint)swayParams.y + layer * 0x9e3779b9u);

    return HashU32(hx ^ (hz * 0x27d4eb2fu) ^ hs);
}

//--------------------------------------------------------------
//! 草むら（塊）の探索結果
//--------------------------------------------------------------
struct ClumpInfo
{
    float normDist;        // 一番近い塊の縁を1とした距離。1未満なら塊の中
    uint  speciesIndex;    // その塊の種番号
};

//--------------------------------------------------------------
//! その場所に一番近い草むら（塊）を探します。
//! @param  [in] worldXZ ワールド座標
//! @return 一番近い塊までの正規化距離と、その塊の種番号
//! @note   ワールドを clumpParams.x 四方の粗い格子に分け、各セルが確率で
//!         塊を1個持つ。中心・半径・形・種はどれもセルのワールド座標の
//!         ハッシュから決めるので、カメラが動いても塊は地面に固定される。
//!
//!         正方形の区画ごとに種を選ぶと田んぼの碁盤目に見えるため、
//!         中心をセル内でばらけさせ、半径を塊ごとに変え、角度方向にも
//!         半径を揺らして不定形にしている。
//!
//!         周囲3x3のセルしか見ないのは、CPU側（GrassFieldSystem.cpp）が
//!         セルの一辺を「塊が届く最大距離（半径の最大 x (1 + 形の揺らぎ)）」
//!         に合わせているから。2セル以上離れたセルの塊の中心はどの点からも
//!         セル1辺より遠く、届かない。この前提が崩れると、セルの境界で
//!         塊が直線的に切れる
//--------------------------------------------------------------
ClumpInfo FindClump(float2 worldXZ)
{
    const float cellSize    = max(clumpParams.x, 1.0f);
    const float radiusMin   = clumpParams.y;
    const float radiusMax   = clumpParams.z;
    const float spawnChance = clumpParams.w;
    const float shapeNoise  = clumpShapeParams.x;
    const float reachMax    = radiusMax * (1.0f + shapeNoise);

    const int baseX = (int)floor(worldXZ.x / cellSize);
    const int baseZ = (int)floor(worldXZ.y / cellSize);

    // 草の配置（MakeCellSeed）と相関しないよう、別の定数で混ぜる
    const uint seedMix = HashU32((uint)swayParams.y * 0x3c6ef372u);

    ClumpInfo result;
    result.normDist     = 1.0e9f;
    result.speciesIndex = 0u;

    for(int dz = -1; dz <= 1; ++dz)
    {
        for(int dx = -1; dx <= 1; ++dx)
        {
            const int  cellX = baseX + dx;
            const int  cellZ = baseZ + dz;
            const uint hx    = HashU32(asuint(cellX) * 0x165667b1u);
            const uint hz    = HashU32(asuint(cellZ) * 0xd3a2646cu);
            const uint h     = HashU32(hx ^ (hz * 0xfd7046c5u) ^ seedMix);

            // このセルに塊が無い
            if(Rand01(h) >= spawnChance)
                continue;

            const float2 center = (float2((float)cellX, (float)cellZ) + float2(Rand01(h + 1u), Rand01(h + 2u))) * cellSize;
            const float2 offset = worldXZ - center;
            const float  dist   = length(offset);

            // どう揺らしても届かない塊は、角度の計算をせずに飛ばす
            if(dist >= reachMax)
                continue;

            // 半径は二乗で偏らせ、小さい塊を多めにする
            const float u      = Rand01(h + 3u);
            const float radius = lerp(radiusMin, radiusMax, u * u);

            // 角度方向に波を2つ重ねて、円ではない不定形にする
            const float angle = atan2(offset.y, offset.x);
            const float wobble = 0.6f * sin(2.0f * angle + Rand01(h + 4u) * TWO_PI)
                               + 0.4f * sin(3.0f * angle + Rand01(h + 5u) * TWO_PI);
            const float shapedRadius = max(radius * (1.0f + shapeNoise * wobble), 1.0f);

            const float normDist = dist / shapedRadius;
            if(normDist < result.normDist)
            {
                result.normDist     = normDist;
                result.speciesIndex = HashU32(h + 6u) % kSpeciesCount;
            }
        }
    }

    return result;
}

//--------------------------------------------------------------
//! 描かない草の頂点を作ります。
//! @return 全メンバを0で埋め、位置をクリップ範囲外の1点にした頂点
//! @note   9頂点すべてが同じ1点になるので三角形の面積がゼロになり、
//!         1ピクセルも塗られない。近景と遠景の受け持ち範囲の外にある草は、
//!         草むらの探索（周囲3x3のハッシュ）を回す前にここで打ち切る
//--------------------------------------------------------------
VSOutput MakeCulledVertex()
{
    VSOutput output = (VSOutput)0;
    output.position = float4(0.0f, 0.0f, -1.0f, 1.0f);
    output.curClip  = output.position;
    output.prevClip = output.position;
    return output;
}

//--------------------------------------------------------------
//! メイン関数
//! @param  [in] input      刃メッシュの頂点
//! @param  [in] instanceID 何本目の草か
//! @return 変換後の頂点データ
//--------------------------------------------------------------
VSOutput VSMain(VSInput input, uint instanceID : SV_InstanceID)
{
    VSOutput output;

    const float fieldSize    = fieldParams.x;
    const float gridDim      = fieldParams.y;
    const float bladesPerCell = fieldParams.z;
    // 時間はゲーム側から渡す。エンジンがb0で配る timeParams.x は
    // ゲームの時間スケールを見ない実時間なので、ヒットストップ中に
    // 草だけ揺れ続けてしまう（霧や火の粉は止まるので不揃いになる）
    const float time         = fieldParams.w;

    const float cellSize = fieldSize / gridDim;

    //----------------------------------------------------------
    // インスタンス番号をセル(i, j)とセル内の通し番号へ分解する
    //----------------------------------------------------------
    const uint perCell   = max((uint)bladesPerCell, 1u);
    const uint cellIndex = instanceID / perCell;
    const uint slot      = instanceID % perCell;

    const uint gridDimU = max((uint)gridDim, 1u);
    const uint gridX    = cellIndex % gridDimU;
    const uint gridZ    = (cellIndex / gridDimU) % gridDimU;

    //----------------------------------------------------------
    // セルの原点をカメラ位置でスナップして求める。
    // カメラが1セル進むと、格子の端のセルが反対側へ回り込み、
    // そこにはその場所ぶんの新しい草が生える
    //----------------------------------------------------------
    const float2 cameraCell = floor(cameraPos.xz / cellSize);
    const float  halfGrid   = floor(gridDim * 0.5f);

    const int cellX = (int)(cameraCell.x + (float)gridX - halfGrid);
    const int cellZ = (int)(cameraCell.y + (float)gridZ - halfGrid);

    const uint seed = MakeCellSeed(cellX, cellZ, slot);

    //----------------------------------------------------------
    // セル内でのばらつき。格子が目に見えないよう、セルの中で
    // 一様にばらけさせる
    //----------------------------------------------------------
    const float2 jitter    = float2(Rand01(seed + 1u), Rand01(seed + 2u));
    const float2 rootXZ    = (float2((float)cellX, (float)cellZ) + jitter) * cellSize;

    const float halfField   = fieldSize * 0.5f;
    const float distFromCam = length(rootXZ - cameraPos.xz);

    //----------------------------------------------------------
    // 近景と遠景の受け持ち。
    // 草は「密で細い近景」と「疎で太い遠景」の2回に分けて描く。1回で地平線まで
    // 敷くと、画面上で数ピクセルにしかならない遠くの草にまで近景と同じ本数を
    // 割くことになり、本数が何倍にも膨らむ。
    //
    // 切替の距離帯では、近景は奥ほど、遠景は手前ほど1本ごとの乱数で間引く。
    // 背を縮めて入れ替えると帯の中で草原が一段低く沈んで見えるので、背は保ったまま
    // 本数だけを滑らかに入れ替える
    //----------------------------------------------------------
    const float lodT    = saturate((distFromCam - lodParams.x) / max(lodParams.y - lodParams.x, 1.0f));
    const float lodKeep = (lodParams.z > 0.5f) ? lodT : (1.0f - lodT);

    if(Rand01(seed + 8u) >= lodKeep)
    {
        return MakeCulledVertex();
    }

    //----------------------------------------------------------
    // 草むら（塊）の判定。
    // 塊の芯ほど残りやすく背が高く、縁に向かって間引かれながら低くなる。
    // 間引きを1本ごとの乱数で決めるので、縁が円や直線にならずギザギザになる。
    // 塊の外でも fillerDensity の割合だけ短い草を残し、土の地面にまばらに生やす。
    //
    // 種は一番近い塊のものを使う。塊の外の草も含めて1本ごとにバラバラに
    // 選ぶと砂嵐のようなノイズに見えるため
    //----------------------------------------------------------
    const ClumpInfo clump        = FindClump(rootXZ);
    const float     edgeSoftness = max(clumpShapeParams.y, 0.01f);
    const float     clumpInside  = 1.0f - saturate((clump.normDist - (1.0f - edgeSoftness)) / edgeSoftness);

    // 遠くほど塊の隙間を埋める。遠景では塊の形はもう見分けられず、隙間の土だけが
    // 霧に溶けた帯として目立つので、地平線まで草で覆われて見えるようにする。
    // 種は塊のものを使い続けるので、色のむらは遠くでも残る
    const float horizonFill = smoothstep(coverageParams.x, coverageParams.y, distFromCam);
    const float inside      = lerp(clumpInside, 1.0f, horizonFill);

    const bool  inClump  = Rand01(seed + 6u) < inside;
    const bool  isFiller = !inClump && (Rand01(seed + 7u) < clumpShapeParams.z);
    const float presence = inClump ? lerp(0.6f, 1.0f, inside) : (isFiller ? clumpShapeParams.w : 0.0f);

    // 生えない草は高さと幅の両方を0にして、9頂点すべてを根元の1点へ潰す。
    // 面積ゼロなので1ピクセルも塗らない（高さだけ0だと地面に寝た三角形が残る）
    const float alive = presence > 0.0f ? 1.0f : 0.0f;

    //----------------------------------------------------------
    // xyzがそれぞれ種0/1/2に対応するので、
    // one-hotのマスクをdotで掛けて選んだ種の値だけを取り出す
    //----------------------------------------------------------
    const uint   speciesIndex = clump.speciesIndex;
    const float3 speciesMask  = float3(speciesIndex == 0u ? 1.0f : 0.0f, speciesIndex == 1u ? 1.0f : 0.0f,
                                       speciesIndex == 2u ? 1.0f : 0.0f);

    //----------------------------------------------------------
    // 境界フェード。カメラからの距離がフィールド半径に近いほど
    // 背を低くして消す。遠景を描くときは遠景の外周だけで効き、
    // 近景は切替の距離帯で本数ごと遠景へ引き継ぐので縮めない
    //----------------------------------------------------------
    const float fadeStart = min(lodParams.w, halfField);
    const float edgeFade  = 1.0f - saturate((distFromCam - fadeStart) / max(halfField - fadeStart, 1.0f));

    //----------------------------------------------------------
    // 草ごとの個性
    //----------------------------------------------------------
    const float baseHeight = dot(speciesHeight.xyz, speciesMask);
    const float heightRand = Rand01(seed + 3u) * 2.0f - 1.0f;                       // -1〜1
    const float height     = baseHeight * (1.0f + heightRand * bladeParams.y) * edgeFade * presence;
    const float yaw        = Rand01(seed + 4u) * TWO_PI;                            // 刃の向き
    const float phase      = Rand01(seed + 5u) * TWO_PI;                            // 揺れの位相

    //----------------------------------------------------------
    // 風。3成分を足して「どれだけ倒れるか」を1つのスカラーにする
    //----------------------------------------------------------
    const float2 windDirXZ = normalize(windParams.xz);

    // 突風の波。位相をワールド座標から決めるので、風の波が草原を
    // 走っていくように見える（全体が一斉に揺れない）
    const float gustPhase = dot(rootXZ, windDirXZ) / max(gustParams.x, 1.0f) - time * (gustParams.y / max(gustParams.x, 1.0f));
    const float gust      = sin(gustPhase) * 0.5f + 0.5f;

    // 草ごとの細かい震え
    const float sway = sin(time * gustParams.w + phase) * swayParams.x;

    float  bendAmount = windParams.w + gust * gustParams.z + sway;
    float2 bendDir    = windDirXZ;

    //----------------------------------------------------------
    // プレイヤーのかき分け。近いほど強く、外向きに倒す
    //----------------------------------------------------------
    if(playerParams.w > 0.0f)
    {
        const float2 toBlade = rootXZ - playerParams.xz;
        const float  dist    = length(toBlade);
        const float  push    = saturate(1.0f - dist / playerParams.w);

        if(push > 0.0f)
        {
            // 二乗で落とすと、半径の外側では滑らかにゼロへ収束する
            const float pushAmount = push * push * fadeParams.y;
            const float2 pushDir   = (dist > 0.001f) ? (toBlade / dist) : float2(1.0f, 0.0f);

            bendDir    = normalize(bendDir * bendAmount + pushDir * pushAmount);
            bendAmount = bendAmount + pushAmount;
        }
    }

    bendAmount = min(bendAmount, 1.4f);    // 倒れすぎて地面へめり込むのを防ぐ

    //----------------------------------------------------------
    // 刃のローカル座標をワールドへ組み立てる
    //----------------------------------------------------------
    const float t = input.uv.y;    // 0（根元）〜1（先端）

    // 刃の面を yaw で回す。幅方向のベクトルと、面の法線
    float sinYaw, cosYaw;
    sincos(yaw, sinYaw, cosYaw);

    const float2 sideDir   = float2(cosYaw, sinYaw);
    const float2 facingDir = float2(-sinYaw, cosYaw);

    // 先端ほど大きく曲げる。根元は動かないので地面から浮かない
    const float bendCurve = bendAmount * t * t;

    float3 worldPos;
    //----------------------------------------------------------
    // 遠くの草ほど幅を広げる。
    // フィールドを広げると同じ本数が薄く散らばって密度が落ちるが、
    // 遠くの草は画面上で数ピクセルしかないため、幅を増やしてやると
    // 隙間が埋まって草原が途切れずに続いて見える。
    // 二乗で効かせるので、手前の草の細さはそのまま保たれる。
    //
    // 基準の距離は近景と遠景で共通にする（層ごとのフィールド半径で割ると、
    // 切替の距離帯で同じ距離なのに近景と遠景の幅が食い違う）。
    // 遠景はさらに、近景より本数が少ないぶんを幅で補う
    //----------------------------------------------------------
    const float distNorm      = saturate(distFromCam / max(coverageParams.z, 1.0f));
    const float speciesWidth  = dot(speciesWidthScale.xyz, speciesMask);
    const float widthScale    = speciesWidth * (1.0f + distNorm * distNorm * bladeParams.x) * coverageParams.w * alive;

    // 幅は刃メッシュの頂点に焼き込み済みの基準幅に、種の倍率と距離による倍率を掛ける。
    // クロスビルボードなので面0(幅はinput.position.x、sideDir方向)と
    // 面1(幅はinput.position.z、facingDir方向)の両方を足す。1頂点につき
    // どちらか片方は必ず0なので、実質的にどちらか一方だけが効く
    worldPos.xz = rootXZ + sideDir * (input.position.x * widthScale) + facingDir * (input.position.z * widthScale)
                + bendDir * (bendCurve * height);

    // 曲がったぶんだけ背が縮む（弧長を保つ近似）
    worldPos.y = bladeParams.z + t * height * (1.0f - bendCurve * bendCurve * 0.35f);

    //----------------------------------------------------------
    // 法線。刃の面向きを基準に、曲がりに合わせて前へ倒す。
    // 平らな板のままだと全部の草が同じ明るさになって書き割りに見えるので、
    // 幅方向に沿って法線を少し外へ開き（丸め）、1本の中に陰影を作る。
    //
    // クロスビルボードなので、頂点がどちらの面に属すかをinput.normalのx/z成分
    // （どちらかが1、もう片方が0のone-hot）で読み分ける。面0(normal.z=1)は
    // 従来通りfacingDir向き・幅方向sideDir、面1(normal.x=1)はその90度回転版
    // （sideDir向き・幅方向facingDir）になる
    //----------------------------------------------------------
    const float2 faceNormalXZ = input.normal.x * sideDir + input.normal.z * facingDir;
    const float2 widthAxisXZ  = input.normal.z * sideDir + input.normal.x * facingDir;

    const float3 faceNormal = float3(faceNormalXZ.x, 0.0f, faceNormalXZ.y);
    const float3 bendNormal = normalize(faceNormal + float3(0.0f, bendCurve, 0.0f));
    const float3 roundOut   = float3(widthAxisXZ.x, 0.0f, widthAxisXZ.y) * (input.uv.x * 2.0f - 1.0f) * 0.5f;

    // アルベドのグラデーションテクスチャは種ごとの帯が縦に並んでいるので、
    // vを自分の帯（speciesIndex番目）の中へ押し込む。texel中心をサンプルする
    // ようにすると、t=0/1でも隣の帯の色とブレンドされずに済む
    const float gradientV = (speciesIndex * kGradientRows + t * (kGradientRows - 1.0f) + 0.5f) / (kGradientRows * (float)kSpeciesCount);

    output.normal   = normalize(bendNormal + roundOut);
    output.worldPos = worldPos;
    output.uv       = float2(input.uv.x, gradientV);

    const float4 clipPos = mul(float4(worldPos, 1.0f), viewProj);

    output.position = clipPos;
    output.curClip  = clipPos;

    // 草は毎フレーム形が変わるが、前フレームの姿勢を持っていないので
    // 速度ゼロとして扱う（モーションブラーで尾を引かせない）
    output.prevClip = clipPos;

    return output;
}
