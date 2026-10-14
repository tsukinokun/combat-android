//-------------------------------------------------------------
//! @file   AttackTurnAssistSystem.hpp
//! @brief  攻撃時にターゲットの方向へプレイヤーを振り向かせるシステムの宣言
//-------------------------------------------------------------
#pragma once
#include <Tsukino/Core/ECS/System/ISystem.hpp>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @class  AttackTurnAssistSystem
    //! @brief  RequestAttackTurnAssist（攻撃の段に入った瞬間に呼ばれる）が書いた振り向きを、
    //!         SoftTargetParams::turnDuration秒かけてプレイヤーのrotationへ反映する。向き（Yaw）だけを変え、位置は動かさない。
    //!         回避に入ったら振り向きを打ち切る（回避は入力方向へ即座に向く）
    //! @note   実行順は攻撃の段に入るPlayerAnimationSystem（Gameplay）の直後、武器の当たり判定を出す
    //!         CombatSystem（WeaponAttach）の前（SystemPriority::AttackTurnAssist）。段に入ったフレームのうちに
    //!         回り始めるので、振り始めと同時に出る当たり判定も振り向いた向きで出る。
    //!         攻撃中はPlayerSystemが向きを書かないので、rotationの取り合いにはならない
    //-------------------------------------------------------------
    class AttackTurnAssistSystem : public Tsukino::ECS::ISystem {
    public:
        //-------------------------------------------------------------
        //! @brief 更新処理
        //-------------------------------------------------------------
        void Update(Tsukino::ECS::Registry& registry, float deltaTime) override;
    };
}    // namespace CombatAndroid::ECS
