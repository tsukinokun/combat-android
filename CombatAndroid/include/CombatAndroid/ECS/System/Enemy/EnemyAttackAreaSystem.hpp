//-------------------------------------------------------------
//! @file   EnemyAttackAreaSystem.hpp
//! @brief  EnemyAttackAreaSystemクラスの宣言
//-------------------------------------------------------------
#pragma once
#include <Tsukino/Core/ECS/System/ISystem.hpp>
#include <Tsukino/Renderer/DX11/MeshBuffer.hpp>
#include <Tsukino/Renderer/DX11/PipelineState.hpp>

#include <memory>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @class  EnemyAttackAreaSystem
    //! @brief  敵が攻撃を振りかぶっている間、攻撃が当たる範囲を足元へ赤く描くシステム
    //! @note   EnemyAttackTelegraphSystem（体の赤いリムライト）と同じ条件・同じ色で出す。
    //!         範囲は薄い面で示し、その内側の濃い塗りが判定の出る瞬間へ向けて広がって、
    //!         縁に届いた瞬間に判定が出る。形は敵の攻撃の仕方で変える：
    //!           頭突き（SmallZombie）      → 前方に置いた円
    //!           薙ぎ払い（BigZombie・Paladin）→ 前方の扇
    //!         大きさはEnemyComponent::attackRangeと判定半径から毎フレーム求めるので、
    //!         エリートの大きさやPaladinの武器の持ち替えにもそのまま追従する。
    //!
    //!         メッシュ・パイプラインはGroundVisualSystemと同じく初回だけ作り、描画コマンドを
    //!         直接積む（新しいアセットもエンジンの変更も要らない）。シェーダーはワールド空間
    //!         スプライトと同じもの（spriteWorldVS / spritePS）を借り、白テクスチャに
    //!         baseColorで色と透明度を乗せる。描画コマンドを積むだけなのでRender優先度に置く
    //-------------------------------------------------------------
    class EnemyAttackAreaSystem : public Tsukino::ECS::ISystem {
    public:
        //-------------------------------------------------------------
        //! @brief 更新処理
        //! @param registry  [in] エンジンのECSレジストリのラッパー
        //! @param deltaTime [in] デルタタイム（表示は攻撃モーションの経過時間で決まるため使わない）
        //-------------------------------------------------------------
        void Update(Tsukino::ECS::Registry& registry, float deltaTime) override;

    private:
        Tsukino::Renderer::MeshBuffer m_discMesh;    //!< 半径1の円（XZ平面、中心が原点）
        Tsukino::Renderer::MeshBuffer m_fanMesh;     //!< 半径1の扇（XZ平面、要が原点で+Z方向へ開く）

        std::shared_ptr<Tsukino::Renderer::PipelineState> m_pipeline;    //!< 半透明・深度は読むだけ
    };
}    // namespace CombatAndroid::ECS
