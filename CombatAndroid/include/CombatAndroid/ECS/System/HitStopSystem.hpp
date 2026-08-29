//-------------------------------------------------------------
//! @file   HitStopSystem.hpp
//! @brief  ヒットストップ対象エンティティのアニメーション・移動を減速させるシステムの宣言
//-------------------------------------------------------------
#pragma once
#include <Tsukino/Core/ECS/System/ISystem.hpp>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @class  HitStopSystem
    //! @brief  HitStopComponentを持つエンティティ（ヒットに関与したプレイヤー/敵）だけを
    //!         対象に、ヒットストップの残り時間を減算し、アニメーション再生速度
    //!         （AnimationPlayerComponent::playback_speed）と移動入力
    //!         （CharacterControllerComponent::moveInput）を減速させる。
    //!
    //!         画面全体のdeltaTimeは操作しないため、他の敵・カメラ・エフェクト等は
    //!         通常速度のまま動き続ける。
    //!
    //! @note   実行順序は PlayerAnimationSystem / EnemyAnimationSystem（共に
    //!         SystemPriority::Gameplay。ここでその フレームのplayback_speedが
    //!         コンボ段・回避などに応じて確定する）より後、
    //!         Tsukino::BuiltIn::ECS::AnimationSystem（同じくGameplay。
    //!         playback_speedを消費してelapsed_timeを進める）より前であること。
    //-------------------------------------------------------------
    class HitStopSystem : public Tsukino::ECS::ISystem {
    public:
        //-------------------------------------------------------------
        //! @brief 更新処理
        //-------------------------------------------------------------
        void Update(Tsukino::ECS::Registry& registry, float deltaTime) override;
    };
}    // namespace CombatAndroid::ECS
