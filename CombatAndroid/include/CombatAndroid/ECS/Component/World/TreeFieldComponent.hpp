//-------------------------------------------------------------
//! @file   TreeFieldComponent.hpp
//! @brief  TreeFieldComponent構造体の宣言
//-------------------------------------------------------------
#pragma once
#include <hlsl++.h>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @struct TreeFieldComponent
    //! @brief  フィールドに生やすプログラム生成の木の調整値（Prefab: Environment/<シーン>/Trees）
    //! @note   木の形はProceduralTree（種から幹・枝・葉の塊を組み立てる）で作り、何種類かを使い回す。
    //!         置き場所はワールドを cellSize の升目に区切り、升目の座標から決まる乱数で決めるので、
    //!         カメラが動いても木は地面に固定され、同じ場所には何度来ても同じ木が生えている。
    //!         描画と当たり判定の実行時状態はTreeFieldSystemが持ち、ここは値だけを持つ
    //-------------------------------------------------------------
    struct TreeFieldComponent {
        bool         enabled = true;        //!< falseなら木を出さない
        unsigned int seed    = 20261012;    //!< 形と配置の乱数の種

        //----------------------------------------------------------
        // 配置
        //----------------------------------------------------------
        int   variantCount     = 6;         //!< 作っておく木の形の種類数（多いほど同じ形が目に付きにくいが、作るのに時間がかかる）
        float cellSize         = 700.0f;    //!< 木を置く升目の一辺。1升に多くて1本
        float spawnChance      = 0.35f;     //!< 1升に木が生える確率
        float cellJitter       = 0.8f;      //!< 升目の中で位置をずらす量（升目の一辺に対する比。1で升目いっぱい）
        float drawRadius       = 3000.0f;   //!< プレイヤーからこの距離までの木を描く（霧で見えなくなる距離より少し先まで）
        float fadeBand         = 700.0f;    //!< 描画範囲の端からこの幅で、木を網点で少しずつ現す（急に出てこないように）
        float startClearRadius = 700.0f;    //!< 走行開始地点（原点）からこの距離には生やさない（開始直後に木へ埋まらないように）
        float scaleMin         = 0.8f;      //!< 1本ごとの大きさの倍率の下限
        float scaleMax         = 1.3f;      //!< 同じく上限

        //----------------------------------------------------------
        // 当たり判定（幹だけ。葉の塊はすり抜ける）
        //----------------------------------------------------------
        float colliderRadius      = 1400.0f;    //!< プレイヤーからこの距離までの木にだけ当たり判定を作る
        float colliderRadiusScale = 1.15f;      //!< 幹の根元の半径に対する当たり判定の太さ
        float colliderHeight      = 220.0f;     //!< 当たり判定の高さ（大きさの倍率を掛ける前）

        //----------------------------------------------------------
        // 形（種類ごとに、この範囲から乱数で選ぶ）
        //----------------------------------------------------------
        float trunkHeightMin  = 150.0f;    //!< 主幹の長さ
        float trunkHeightMax  = 220.0f;
        float trunkRadiusMin  = 24.0f;     //!< 主幹の根元の半径
        float trunkRadiusMax  = 33.0f;
        int   branchDepth     = 3;         //!< 枝分かれの深さ
        int   childCountMin   = 2;         //!< 1本の枝から出る子の数
        int   childCountMax   = 3;
        float spreadAngleMin  = 0.55f;     //!< 子の枝が開く角度（ラジアン）
        float spreadAngleMax  = 0.85f;
        float canopyRadiusMin = 64.0f;     //!< 枝先1つぶんの茂みの半径
        float canopyRadiusMax = 85.0f;
        int   leafCardsPerTip = 40;        //!< 枝の先1つあたりの葉カード（葉の絵を貼った小さな板）の枚数
        float leafCardSize    = 46.0f;     //!< 葉カード1枚の一辺

        //----------------------------------------------------------
        // 風の揺れ（TreeSway.hlsli）。風向きと突風は、草（GrassFieldComponent）があればそちらに揃える
        //----------------------------------------------------------
        float          swayStrength        = 7.0f;      //!< 木全体のしなりの振れ幅（swayHeightRefの高さでの、ワールド単位）
        float          swaySpeed           = 1.1f;      //!< しなりの速さ（ラジアン/秒）
        float          swayHeightRef       = 450.0f;    //!< しなりの強さの基準にする高さ（これより高い所ほど大きく動く）
        float          leafFlutterStrength = 2.5f;      //!< 葉のそよぎの振れ幅
        float          leafFlutterSpeed    = 6.0f;      //!< 葉のそよぎの速さ（ラジアン/秒）
        float          leafNormalMinFacing = 0.3f;      //!< 葉の法線をカメラへ寄せる下限（N・V）。茂みの縁の葉が空や太陽を映して白く飛ぶのを防ぐ。上げるほど陰影が平たくなる（0で無効）
        hlslpp::float3 windDirection       = hlslpp::float3(1.0f, 0.0f, 0.3f);    //!< 草が無いときの風向き（水平成分だけ使う）
        float          gustWavelength      = 650.0f;    //!< 草が無いときの突風の波長
        float          gustSpeed           = 320.0f;    //!< 草が無いときの突風の速さ
        float          gustStrength        = 0.3f;      //!< 草が無いときの突風の強さ

        //----------------------------------------------------------
        // 攻撃への反応（TreeHitEvent・範囲攻撃の衝撃波で、木を大きく揺らして葉を落とす）
        //----------------------------------------------------------
        float hitShakeImpulse    = 160.0f;  //!< 攻撃が木に当たったときに、しなりへ与える初速（基準の高さでの量/秒）
        float waveShakeImpulse   = 120.0f;  //!< 範囲攻撃の衝撃波の中心で、しなりへ与える初速（縁へ行くほど弱まる）
        float shakeFrequency     = 1.4f;    //!< 攻撃の揺れの振動数（Hz。大きい木ほど遅く揺れる感じを出すため低め）
        float shakeDamping       = 0.18f;   //!< 攻撃の揺れの減衰比（小さいほど長く揺り戻す）
        float shakeFlutterBoost  = 0.25f;   //!< しなりのずれ1あたりに、葉のそよぎを強める量
        int   leavesPerHit       = 14;      //!< 攻撃が1回当たったときに落ちる葉の枚数（強さを掛ける）
        int   maxFallingLeaves   = 300;     //!< 同時に舞っていられる葉の上限（超えたら古い葉から消す）
        float fallingLeafSize    = 14.0f;   //!< 落ちる葉1枚の一辺
        float leafFallSpeed      = 70.0f;   //!< 葉が落ちる速さの上限（空気抵抗でこれ以上速くならない）
        float leafRestTime       = 4.0f;    //!< 葉が地面に着いてから消えるまでの秒数

        //----------------------------------------------------------
        // 色（下→上のグラデーション）
        //----------------------------------------------------------
        hlslpp::float3 barkBottomColor = hlslpp::float3(0.20f, 0.13f, 0.08f);    //!< 幹の根元の色
        hlslpp::float3 barkTopColor    = hlslpp::float3(0.34f, 0.25f, 0.16f);    //!< 枝先の色
        hlslpp::float3 leafBottomColor = hlslpp::float3(0.05f, 0.16f, 0.05f);    //!< 葉の色の暗い側（葉ごとにこの間で選ぶ）
        hlslpp::float3 leafTopColor    = hlslpp::float3(0.24f, 0.48f, 0.12f);    //!< 葉の色の明るい側

        //----------------------------------------------------------
        // 樹皮の模様（幹のテクスチャはBuildBarkPixelsがここから作る。色の土台は上のbarkBottom/TopColor）
        //----------------------------------------------------------
        int            barkRidgeCount      = 22;                                     //!< 幹1周あたりに並ぶ樹皮の板の数（多いほど細かい）
        int            barkPlateCount      = 70;                                     //!< 木の高さ全体で縦に並ぶ樹皮の板の数
        float          barkFissureDarkness = 0.75f;                                  //!< 裂け目の底の暗さ（0で暗くしない、1で真っ黒）
        float          barkNormalStrength  = 2.5f;                                   //!< 凹凸の強さ（法線マップの傾きの倍率）
        float          barkAoStrength      = 0.6f;                                   //!< 裂け目の陰の濃さ（0で陰を付けない）
        hlslpp::float3 barkMossColor       = hlslpp::float3(0.16f, 0.24f, 0.07f);    //!< 根元の苔の色
        float          barkMossHeight      = 0.12f;                                  //!< 苔が生える高さ（木の高さに対する割合）
        float          barkMossAmount      = 0.7f;                                   //!< 苔の濃さ（0で苔なし）
    };
}    // namespace CombatAndroid::ECS
