//-------------------------------------------------------------
//! @file   PaladinWeaponSwitchSystem.hpp
//! @brief  PaladinWeaponSwitchSystemクラスの宣言
//-------------------------------------------------------------
#pragma once
#include <Tsukino/Core/ECS/System/ISystem.hpp>

#include <random>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @class  PaladinWeaponSwitchSystem
    //! @brief  複数の武器を持つエリートのPaladinが、攻撃を終えるたびに次に使う武器を選び直すシステム
    //! @note   選び方はプレイヤーとの距離で使い分ける。
    //!           離れている → 間合いの長いグレートソードで踏み込む
    //!           密着している → 一撃の重いウォーハンマーで叩く
    //!           その間     → バトルアックス
    //!         持っている種類は個体ごとに違う（2本以上）ので、ちょうどの武器が無ければ
    //!         間合いが一番近い手持ちで代える。
    //!         同じ武器が3回続きそうなときは別の武器に替え、単調にならないようにする。
    //!         持ち替えるのは攻撃ステートを抜けた瞬間（振り終わり・のけぞり）だけで、
    //!         次の攻撃の間合い（MoveToPlayerが足を止める距離）も新しい武器のものになる。
    //!
    //!         EnemyAnimationSystemが今フレームのcurrentStateを確定させた後に読む必要があるため、
    //!         同じGameplay優先度で、EnemyAnimationSystemより後に登録する
    //-------------------------------------------------------------
    class PaladinWeaponSwitchSystem : public Tsukino::ECS::ISystem {
    public:
        //-------------------------------------------------------------
        //! @brief 更新処理
        //! @param registry  [in] エンジンのECSレジストリのラッパー
        //! @param deltaTime [in] デルタタイム（持ち替えはステートの変化だけで決まるため使わない）
        //-------------------------------------------------------------
        void Update(Tsukino::ECS::Registry& registry, float deltaTime) override;

    private:
        std::mt19937 m_rng{std::random_device{}()};    //!< 代わりの武器・同じ武器が続いたときの替え先の抽選
    };
}    // namespace CombatAndroid::ECS
