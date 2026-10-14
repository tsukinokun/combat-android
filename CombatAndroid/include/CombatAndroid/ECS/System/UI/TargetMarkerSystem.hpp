//-------------------------------------------------------------
//! @file   TargetMarkerSystem.hpp
//! @brief  ソフトターゲットの敵を画面上で四隅のカギ括弧で囲むシステムの宣言
//-------------------------------------------------------------
#pragma once
#include <Tsukino/Core/ECS/System/ISystem.hpp>
#include <Tsukino/Core/ECS/Entity/Entity.hpp>

#include <array>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @class  TargetMarkerSystem
    //! @brief  SoftTargetComponent::targetの敵の体を、画面に重ねた四隅のカギ括弧（「 」）で囲む。
    //!         振り向きが効く範囲の外（候補）では薄く細い印、範囲内（assistReady）では濃く太い印にして、
    //!         「今攻撃すればこの敵に向く」が一目で分かるようにする。
    //!         ターゲットが変わった瞬間は枠を大きく出して縮め、吸い付くように見せる
    //! @note   枠の大きさは敵のカプセル（CollisionComponent）の足元と頭頂を画面へ写して決めるので、
    //!         敵の大きさが違っても体を囲める。実行順は今フレームのカメラ行列が確定するCameraの後、
    //!         UIの位置を行列へ焼き込むTransformUIの前（SystemPriority::TargetMarker）
    //-------------------------------------------------------------
    class TargetMarkerSystem : public Tsukino::ECS::ISystem {
    public:
        //-------------------------------------------------------------
        //! @brief 更新処理
        //-------------------------------------------------------------
        void Update(Tsukino::ECS::Registry& registry, float deltaTime) override;

    private:
        //! カギ括弧を作る細い矩形（4隅×縦横2本）。初回Updateで生成する
        std::array<Tsukino::ECS::Entity, 8> m_bars{};
        bool                                m_created = false;

        Tsukino::ECS::Entity m_shownTarget  = entt::null;    //!< 前のフレームに囲んでいた敵（変わったら吸い付きの動きをやり直す）
        float                m_acquireTimer = 0.0f;          //!< ターゲットが変わってからの経過秒（吸い付きの動き）
        float                m_readyBlend   = 0.0f;          //!< 薄い印(0)〜濃い印(1)の間の今の値（急に切り替わらないよう寄せる）
    };
}    // namespace CombatAndroid::ECS
