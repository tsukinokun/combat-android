//-------------------------------------------------------------
//! @file   TreeFieldSystem.hpp
//! @brief  TreeFieldSystemクラスの宣言
//-------------------------------------------------------------
#pragma once
#include <Tsukino/Core/ECS/System/ISystem.hpp>
#include <Tsukino/Renderer/DX11/MeshBuffer.hpp>

#include <entt/entt.hpp>

#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @class  TreeFieldSystem
    //! @brief  フィールドにプログラム生成の木を生やし、描画と幹の当たり判定を受け持つシステム
    //! @note   最初の更新でTreeFieldComponentの値から木の形を何種類か作り（ProceduralTree）、以後は使い回す。
    //!         毎フレーム、プレイヤーの周りの升目を調べて、升目の座標から決まる乱数で木を置き、
    //!         1本ずつ幹と葉の描画コマンドをGBufferパスへ積む。1本ずつ描くのは、エンジンの影の描き直しが
    //!         「1描画＝1つのワールド行列」で、まとめ描画（インスタンス描画）だと影が正しく落ちないため。
    //!         当たり判定はプレイヤーの近くの木にだけ、幹のカプセルを持つエンティティを作り、離れたら消す
    //-------------------------------------------------------------
    class TreeFieldSystem : public Tsukino::ECS::ISystem {
    public:
        //-------------------------------------------------------------
        //! @brief 更新処理
        //! @param registry  [in] エンジンのECSレジストリのラッパー
        //! @param deltaTime [in] デルタタイム（使わない。木は動かない）
        //-------------------------------------------------------------
        void Update(Tsukino::ECS::Registry& registry, float deltaTime) override;

    private:
        //-------------------------------------------------------------
        //! @struct TreeVariant
        //! @brief  作っておいた木の形1種類ぶん
        //-------------------------------------------------------------
        struct TreeVariant {
            Tsukino::Renderer::MeshBuffer trunk;                //!< 幹と枝
            Tsukino::Renderer::MeshBuffer canopy;               //!< 葉の塊
            float                         trunkRadius = 0.0f;   //!< 主幹の根元の半径（当たり判定の太さに使う）
        };

        std::vector<TreeVariant> m_variants;          //!< 木の形（最初の更新で作る。作った後は数を変えない＝描画コマンドが指す先を動かさない）
        bool                     m_buildFailed = false;    //!< 形を作れなかった（以後は何もしない）

        //! 当たり判定のエンティティ。キーは升目の座標（木は1升に多くて1本）
        std::unordered_map<std::uint64_t, Tsukino::ECS::Entity> m_colliders;

        //! このフレームに当たり判定が要る升目（毎フレーム作り直す。確保し直さないよう使い回す）
        std::unordered_set<std::uint64_t> m_wantedColliders;
    };
}    // namespace CombatAndroid::ECS
