//--------------------------------------------------------------
//! @file   GrassFieldSystem.hpp
//! @brief  草原システムの宣言
//! @author 山﨑愛
//--------------------------------------------------------------
#pragma once
#include <Tsukino/Core/ECS/System/ISystem.hpp>
#include <Tsukino/Renderer/DX11/MeshBuffer.hpp>
#include <Tsukino/Renderer/DX11/UserConstantBuffer.hpp>

#include <hlsl++.h>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //--------------------------------------------------------------
    //! 草の本数の上限
    //! @note 1本 = 9頂点・21インデックスのインスタンス描画なので、
    //!       上限では 1,376,256 インデックスの単一Drawになる
    //--------------------------------------------------------------
    inline constexpr Tsukino::u32 kMaxGrassBlades = 65536;

    //--------------------------------------------------------------
    //! @struct CBufferGrass
    //! @brief  草の頂点シェーダーへ渡すパラメータ
    //! @note   Grass.vs.hlsl の CBufferGrass と1バイト単位で一致させること
    //!         （全メンバfloat4で144バイト）。種の数（3）は両ファイルで
    //!         決め打ちしており、speciesHeight/speciesWidthScale の xyz が
    //!         それぞれの種に対応する。種を増やす場合はここと
    //!         Grass.vs.hlsl、GetGradientSRV（GrassFieldSystem.cpp）の
    //!         グラデーションテクスチャの段数を合わせて直すこと。
    //!
    //!         エンジンの定数バッファ（b0〜b9）ではなく、ゲーム予約枠の
    //!         CBSlot::User0（b12）へバインドされる。エンジンは草という
    //!         演出を知らないので、この構造体もゲーム側が持つ。
    //!
    //!         なお経過時間はエンジンが b0（CBufferScene::timeParams）で
    //!         全シェーダーへ配っているため、ここには持たせない
    //--------------------------------------------------------------
    struct CBufferGrass {
        hlslpp::float4 fieldParams;         //!< x: フィールドの一辺, y: 1辺のセル数, z: セルあたりの本数, w: 経過時間（秒）
        hlslpp::float4 bladeParams;         //!< x: 遠くの草の幅の増し分, y: 高さのばらつき, z: 地面の高さ(Y), w: 種の切替パッチの大きさ
        hlslpp::float4 windParams;          //!< xyz: 風向き（正規化済み）, w: 常時なびく強さ
        hlslpp::float4 gustParams;          //!< x: 突風の波長, y: 突風の速さ, z: 突風の強さ, w: そよぎの角速度
        hlslpp::float4 swayParams;          //!< x: そよぎの強さ, y: 乱数シード, zw: 予約
        hlslpp::float4 speciesHeight;       //!< xyz: 種0/1/2の高さ, w: 予約
        hlslpp::float4 speciesWidthScale;   //!< xyz: 種0/1/2の幅倍率, w: 予約
        hlslpp::float4 playerParams;        //!< xyz: プレイヤーのワールド座標, w: かき分け半径（0で無効）
        hlslpp::float4 fadeParams;          //!< x: 境界フェード開始比率, y: かき分けの強さ, zw: 予約
    };

    //--------------------------------------------------------------
    //! GrassFieldComponentのパラメータをRendererへ転送し、草を描画するシステム
    //! @note 草1本ぶんの刃メッシュを初回に1つだけ作り、以後はそれを
    //!       インスタンス描画で使い回す。1本ごとの位置・向き・高さ・
    //!       揺れの位相は頂点シェーダーがハッシュから計算するため、
    //!       このシステムはインスタンスバッファを一切持たない。
    //!
    //!       カメラの位置は頂点シェーダーがb0（CBufferScene）から直接
    //!       読むので、ここではCameraComponentを参照しない。
    //!       プレイヤー位置だけは b11 に載せる必要があるため収集する。
    //--------------------------------------------------------------
    class GrassFieldSystem : public Tsukino::ECS::ISystem {
    public:
        //--------------------------------------------------------------
        //! 更新処理を行います。
        //! @param [in,out] registry  対象のレジストリ
        //! @param [in]     deltaTime 前フレームからの経過秒
        //--------------------------------------------------------------
        void Update(Tsukino::ECS::Registry& registry, float deltaTime) override;

    private:
        //! 草を揺らすための経過時間
        //! @note ヒットストップなどで deltaTime が 0 のときは草も止まる。
        //!       エンジンが b0 で配る時間（CBufferScene::timeParams）は
        //!       ゲームの時間スケールを見ない実時間なので、
        //!       「ヒットストップ中は草も止まる」挙動を保つためこちらを使う
        float m_time = 0.0f;

        //! 頂点シェーダーへ渡すパラメータ用のバッファ（CBSlot::User0）
        //! @note エンジンの定数バッファではなく、ゲーム予約枠へバインドする
        //!       ゲーム所有のバッファ。初回のUpdateで1回だけ作る
        Tsukino::Renderer::UserConstantBuffer m_paramBuffer;

        //! 刃1本ぶんのメッシュ（9頂点・21インデックス）
        //! @note 全部の草がこの1本を共有する。作り直す必要があるのは
        //!       形状パラメータが変わったときだけなので、下の値と
        //!       比べて変化したときにだけ作り直す
        Tsukino::Renderer::MeshBuffer m_bladeMesh;

        //! m_bladeMesh を作ったときの根元の幅
        //! @note 高さと曲がりは頂点シェーダー側で掛けるため、
        //!       メッシュの作り直しが要るのは幅が変わったときだけ
        float m_builtBladeWidth = -1.0f;

        //! 草の本数の上限超過を1回だけ警告するためのフラグ
        bool m_countOverflowWarned = false;
    };
}    // namespace CombatAndroid::ECS
