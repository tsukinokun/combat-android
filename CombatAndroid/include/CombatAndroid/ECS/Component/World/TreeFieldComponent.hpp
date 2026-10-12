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
        // 色（下→上のグラデーション）
        //----------------------------------------------------------
        hlslpp::float3 barkBottomColor = hlslpp::float3(0.16f, 0.10f, 0.06f);    //!< 幹の根元の色
        hlslpp::float3 barkTopColor    = hlslpp::float3(0.30f, 0.21f, 0.13f);    //!< 枝先の色
        hlslpp::float3 leafBottomColor = hlslpp::float3(0.05f, 0.16f, 0.05f);    //!< 葉の色の暗い側（葉ごとにこの間で選ぶ）
        hlslpp::float3 leafTopColor    = hlslpp::float3(0.24f, 0.48f, 0.12f);    //!< 葉の色の明るい側
    };
}    // namespace CombatAndroid::ECS
