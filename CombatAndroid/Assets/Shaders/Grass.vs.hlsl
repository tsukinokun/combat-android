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
    float4 bladeParams;        // x: 遠くの草の幅の増し分, y: 高さのばらつき, z: 地面の高さ(Y), w: 種の切替パッチの大きさ
    float4 windParams;         // xyz: 風向き（正規化済み）, w: 常時なびく強さ
    float4 gustParams;         // x: 突風の波長, y: 突風の速さ, z: 突風の強さ, w: そよぎの角速度
    float4 swayParams;         // x: そよぎの強さ, y: 乱数シード, zw: 予約
    float4 speciesHeight;      // xyz: 種0/1/2の高さ, w: 予約
    float4 speciesWidthScale;  // xyz: 種0/1/2の幅倍率, w: 予約
    float4 playerParams;       // xyz: プレイヤー座標, w: かき分け半径
    float4 fadeParams;         // x: 境界フェード開始比率, y: かき分けの強さ, z: 予約, w: 予約
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
    uint hx = HashU32(asuint(cellX) * 0x9e3779b9u);
    uint hz = HashU32(asuint(cellZ) * 0x85ebca6bu);
    uint hs = HashU32(slot * 0xc2b2ae35u + (uint)swayParams.y);

    return HashU32(hx ^ (hz * 0x27d4eb2fu) ^ hs);
}

//--------------------------------------------------------------
//! 草が生えているパッチ（patchSize四方の区画）から種を選びます。
//! @param  [in] worldXZ ワールド座標
//! @return 0〜kSpeciesCount-1 の種番号
//! @note   草1本ごとにバラバラに選ぶと砂嵐のようなノイズに見えるため、
//!         セルより一回り大きいパッチ単位でまとめて同じ種を選ぶ。
//!         MakeCellSeedと同じ「ワールド座標のハッシュ」方式なので、
//!         こちらもカメラが動いても同じ場所には同じ種が生え続ける
//--------------------------------------------------------------
uint MakeSpeciesIndex(float2 worldXZ)
{
    const float patchSize = max(bladeParams.w, 1.0f);

    const int patchX = (int)floor(worldXZ.x / patchSize);
    const int patchZ = (int)floor(worldXZ.y / patchSize);

    const uint hx = HashU32(asuint(patchX) * 0x27d4eb2fu);
    const uint hz = HashU32(asuint(patchZ) * 0xb492b66fu);
    const uint hs = HashU32((uint)swayParams.y * 0x68e31da4u);

    return HashU32(hx ^ (hz * 0x9e3779b9u) ^ hs) % kSpeciesCount;
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

    //----------------------------------------------------------
    // 種の抽選。1本ごとではなくパッチ単位でまとまって選ぶので、
    // 群生のような塊で生える。xyzがそれぞれ種0/1/2に対応するので、
    // one-hotのマスクをdotで掛けて選んだ種の値だけを取り出す
    //----------------------------------------------------------
    const uint   speciesIndex = MakeSpeciesIndex(rootXZ);
    const float3 speciesMask  = float3(speciesIndex == 0u ? 1.0f : 0.0f, speciesIndex == 1u ? 1.0f : 0.0f,
                                       speciesIndex == 2u ? 1.0f : 0.0f);

    //----------------------------------------------------------
    // 境界フェード。カメラからの距離がフィールド半径に近いほど
    // 背を低くして消す。高さ0の草は面積ゼロで1ピクセルも塗らない
    //----------------------------------------------------------
    const float halfField  = fieldSize * 0.5f;
    const float distFromCam = length(rootXZ - cameraPos.xz);
    const float fadeStart  = halfField * fadeParams.x;
    const float edgeFade   = 1.0f - saturate((distFromCam - fadeStart) / max(halfField - fadeStart, 1.0f));

    //----------------------------------------------------------
    // 草ごとの個性
    //----------------------------------------------------------
    const float baseHeight = dot(speciesHeight.xyz, speciesMask);
    const float heightRand = Rand01(seed + 3u) * 2.0f - 1.0f;                       // -1〜1
    const float height     = baseHeight * (1.0f + heightRand * bladeParams.y) * edgeFade;
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
    // 二乗で効かせるので、手前の草の細さはそのまま保たれる
    //----------------------------------------------------------
    const float distNorm      = saturate(distFromCam / max(halfField, 1.0f));
    const float speciesWidth  = dot(speciesWidthScale.xyz, speciesMask);
    const float widthScale    = speciesWidth * (1.0f + distNorm * distNorm * bladeParams.x);

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
