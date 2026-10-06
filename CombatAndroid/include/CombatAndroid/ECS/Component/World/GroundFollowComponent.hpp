//-------------------------------------------------------------
//! @file   GroundFollowComponent.hpp
//! @brief  GroundFollowComponent構造体の宣言
//-------------------------------------------------------------
#pragma once
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @struct GroundFollowComponent
    //! @brief  このコンポーネントを持つエンティティの位置(x/z)を、毎フレーム
    //!         プレイヤーへ追従させる目印。地面エンティティに付与する。
    //! @note   草原（GrassFieldSystem）はカメラを中心に再配置されるため見た目は
    //!         無限に地面が続くが、地面のコリジョンは有限サイズの箱でしか表現できない。
    //!         箱をどれだけ大きくしてもプレイヤーがいつか端を越えて落下してしまうため、
    //!         箱自体をプレイヤーに追従させることで実質無限の地面にする。
    //!
    //!         付与先のエンティティは RigidbodyComponent::type を Kinematic に
    //!         しておくこと。PhysicsSystem が Kinematic エンティティの
    //!         TransformComponent を毎フレーム自動で物理へ同期するため、
    //!         このコンポーネント側は位置を書き換えるだけでよい
    //!         （Staticのままだと物理側の位置が更新されない）。
    //-------------------------------------------------------------
    struct GroundFollowComponent {
        float groundHeight = -5.0f;    //!< 追従中も固定するY座標（地面の中心の高さ）
    };
}    // namespace CombatAndroid::ECS
