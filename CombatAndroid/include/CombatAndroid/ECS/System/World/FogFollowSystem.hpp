//-------------------------------------------------------------
//! @file   FogFollowSystem.hpp
//! @brief  FogFollowSystemクラスの宣言
//-------------------------------------------------------------
#pragma once
#include <Tsukino/Core/ECS/System/ISystem.hpp>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @class  FogFollowSystem
    //! @brief  FogComponentの距離フォグの基準点を、毎フレームプレイヤーの位置へ
    //!         書き込むシステム。
    //! @note   プレイヤーは CharacterControllerComponent を持つ最初のエンティティを
    //!         追う（GroundFollowSystem/GrassFieldSystemのプレイヤー位置取得と同じ流儀）。
    //!         TPSカメラはプレイヤーの周りを旋回するため、フォグの距離をカメラ位置基準
    //!         のままにすると、同じ位置の敵でも旋回角度によってフォグへの入り方が
    //!         変わってしまう。プレイヤー位置を基準点として渡すことでこれを防ぐ。
    //!
    //!         実行順序はプレイヤーのMovementが確定した後、かつ
    //!         Tsukino::BuiltIn::ECS::FogSystem（CBufferFogへ転送する）より前で
    //!         あること（詳細はSystemPriority.hpp参照）
    //-------------------------------------------------------------
    class FogFollowSystem : public Tsukino::ECS::ISystem {
    public:
        //-------------------------------------------------------------
        //! @brief 更新処理
        //-------------------------------------------------------------
        void Update(Tsukino::ECS::Registry& registry, float deltaTime) override;
    };
}    // namespace CombatAndroid::ECS
