//-------------------------------------------------------------
//! @file   WeaponTetherSystem.hpp
//! @brief  WeaponTetherSystemクラスの宣言
//-------------------------------------------------------------
#pragma once
#include <Tsukino/Core/ECS/System/ISystem.hpp>
#include <Tsukino/Engine/Asset/AssetHandle.hpp>
#include <Tsukino/Renderer/DX11/MeshBuffer.hpp>

#include <entt/entt.hpp>
#include <hlsl++.h>

#include <memory>
#include <unordered_map>

namespace Tsukino::Renderer {
    struct PipelineState;
}

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @class  WeaponTetherSystem
    //! @brief  プレイヤーの背中に小さな光の点を置き、そこから浮いている武器の柄へ光の糸を伸ばすシステム
    //! @note   武器が浮いている理由を絵で見せる（背中から引っ張って操っている）。
    //!         糸は背中の点から柄への2次ベジェ曲線で、下へ垂らしたたわみに加え、
    //!         武器がばねで遅れて動いた分だけ動きと逆へしなる（CombatSystemの浮遊のばね追従）。
    //!         攻撃で手に持っている間（WeaponComponent::attackBlend）は消え、浮遊へ戻ると現れる。
    //!         拾って浮遊武器になった瞬間は、背中から柄へ伸びていくように出す。
    //!
    //!         曲線を区切り、区切りごとにカメラを向いた細い板を加算合成で描く（EnemyAttackAreaSystemと同じく
    //!         ワールド空間スプライトのシェーダーを借り、描画コマンドを直接積む）。
    //!         武器の位置が決まるCombatSystem（WeaponAttach）と、カメラの位置が決まるCameraより後のRenderに置く。
    //!         調整値は Assets/Tables/Systems/WeaponTether.json
    //-------------------------------------------------------------
    class WeaponTetherSystem : public Tsukino::ECS::ISystem {
    public:
        //-------------------------------------------------------------
        //! @brief 更新処理
        //! @param registry  [in] エンジンのECSレジストリのラッパー
        //! @param deltaTime [in] デルタタイム（糸の伸び・しなりを進める。メニュー中は0で止まる）
        //-------------------------------------------------------------
        void Update(Tsukino::ECS::Registry& registry, float deltaTime) override;

    private:
        //-------------------------------------------------------------
        //! @struct TetherState
        //! @brief  武器1本ぶんの糸の状態
        //-------------------------------------------------------------
        struct TetherState {
            float          appear      = 0.0f;                               //!< 背中から柄へ伸びた割合（0〜1）
            hlslpp::float3 prevOffset  = hlslpp::float3(0.0f, 0.0f, 0.0f);   //!< 前フレームの「背中の点→柄」
            hlslpp::float3 bend        = hlslpp::float3(0.0f, 0.0f, 0.0f);   //!< 遅れによるしなり（なめらかにしたもの）
            bool           hasPrev     = false;                              //!< prevOffsetが有効か
            bool           seen        = false;                              //!< このフレームに見つかったか（見つからなければ消す）
        };

        Tsukino::Renderer::MeshBuffer                     m_stripMesh;     //!< 幅1・長さ1の板（XZ平面、手前の辺の中央が原点で+Zへ伸びる。Uは横、Vは長さ方向）
        std::shared_ptr<Tsukino::Renderer::PipelineState> m_pipeline;      //!< 加算合成・深度は読むだけ（光）
        std::shared_ptr<Tsukino::Renderer::PipelineState> m_shadePipeline; //!< 普通の半透明・深度は読むだけ（光の下に敷く暗い帯）
        std::unordered_map<entt::entity, TetherState>     m_tethers;       //!< 武器ごとの糸の状態
        Tsukino::u32                                      m_spineNode = UINT32_MAX;    //!< 背中のボーンのノード番号（解決済みキャッシュ）
        Tsukino::Asset::AssetHandle                       m_spineResolvedModel;        //!< 背中のボーンを解決したときのモデル
        float                                             m_time = 0.0f;               //!< 揺らぎ・脈打ち用の経過時間（メニュー中は止まる）
    };
}    // namespace CombatAndroid::ECS
