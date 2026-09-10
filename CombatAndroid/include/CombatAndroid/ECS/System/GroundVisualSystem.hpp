//-------------------------------------------------------------
//! @file   GroundVisualSystem.hpp
//! @brief  GroundVisualSystemクラスの宣言
//-------------------------------------------------------------
#pragma once
#include <Tsukino/Core/ECS/System/ISystem.hpp>
#include <Tsukino/Renderer/DX11/MeshBuffer.hpp>
#include <Tsukino/Renderer/DX11/UserConstantBuffer.hpp>
#include <Tsukino/Engine/Asset/AssetHandle.hpp>

#include <hlsl++.h>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @struct CBufferGround
    //! @brief  地面の頂点シェーダーへ渡すパラメータ
    //! @note   Ground.vs.hlsl の CBufferGround と1バイト単位で一致させること。
    //!         エンジンの定数バッファ（b0〜b9）ではなく、ゲーム予約枠の
    //!         CBSlot::User0（b12）へバインドされる
    //-------------------------------------------------------------
    struct CBufferGround {
        hlslpp::float4 tileParams;    //!< x: タイル1枚ぶんのワールド距離, yzw: 予約
    };

    //-------------------------------------------------------------
    //! @class  GroundVisualSystem
    //! @brief  GroundFollowComponentを持つエンティティ（地面）の上に、
    //!         土のテクスチャを貼った平らな板を描画するシステム。
    //! @note   草原（GrassFieldSystem）で隠しきれない地肌がそのまま見えて
    //!         いたのを、タイル張りの土テクスチャで「地肌らしい地肌」に
    //!         する目的で追加した。板は静的な1枚板（4頂点・6インデックス）
    //!         で、GroundFollowSystemが動かす地面エンティティのX/Zへ
    //!         毎フレーム追従させる（Yは地面コライダーの上面に合わせて
    //!         常に0固定。コライダー中心のY=-5とは独立に扱う）。
    //!
    //!         頂点シェーダーはゲームAssetsのGround.vs.hlslを使う。板自体が
    //!         プレイヤーへ追従して動くため、UVをメッシュへ焼き込む方式
    //!         （エンジン組み込みのstaticModelVS）だとテクスチャが板へ
    //!         くっついたまま移動し「歩いても模様が流れない」不自然な
    //!         見た目になる。Ground.vs.hlslはUVをワールドXZ座標から
    //!         算出するため、板が追従してもテクスチャは世界に対して
    //!         固定されて見える（草のワールド座標ハッシュと同じ考え方）
    //-------------------------------------------------------------
    class GroundVisualSystem : public Tsukino::ECS::ISystem {
    public:
        //-------------------------------------------------------------
        //! @brief 更新処理
        //-------------------------------------------------------------
        void Update(Tsukino::ECS::Registry& registry, float deltaTime) override;

    private:
        //! 土テクスチャを貼った板のメッシュ（4頂点・6インデックス）。
        //! 形状は固定なので初回のUpdateで1回だけ作る
        Tsukino::Renderer::MeshBuffer m_groundMesh;

        //! 土テクスチャのハンドル。初回Updateで遅延ロードする
        Tsukino::Asset::AssetHandle m_dirtTextureHandle;

        //! 頂点シェーダーへ渡すパラメータ用のバッファ（CBSlot::User0）
        Tsukino::Renderer::UserConstantBuffer m_paramBuffer;
    };
}    // namespace CombatAndroid::ECS
