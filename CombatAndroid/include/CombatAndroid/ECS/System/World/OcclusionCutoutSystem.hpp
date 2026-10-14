//-------------------------------------------------------------
//! @file   OcclusionCutoutSystem.hpp
//! @brief  プレイヤーを遮る物を、プレイヤーの周りだけ網点で抜くシステムの宣言
//-------------------------------------------------------------
#pragma once
#include <Tsukino/Core/ECS/System/ISystem.hpp>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @class  OcclusionCutoutSystem
    //! @brief  毎フレーム、プレイヤーの胸の高さを画面へ写し、その周りの円とプレイヤーまでの距離を
    //!         エンジンの遮り抜き（FrameConstants::SetOcclusionCutout）へ渡す。
    //!         プレイヤーより手前にあって円に入る画素だけが、中心ほど強く網点で抜ける。
    //!         抜かれるのはDrawCommand::occlusionCutoutが立っている物だけで、木（TreeFieldSystemが立てる）と、
    //!         OcclusionCutoutComponentを持つモデル（敵・武器のPrefab）が対象。草とプレイヤー自身は対象外。
    //!         プレイヤーが持っている武器は抜かないよう、武器のOcclusionCutoutComponent::enabledを毎フレーム合わせる
    //! @note   実行順は今フレームのカメラ行列が確定するCameraの後、ModelSystem・TreeFieldSystemが
    //!         描画コマンドを積む前（SystemPriority::OcclusionCutout）
    //-------------------------------------------------------------
    class OcclusionCutoutSystem : public Tsukino::ECS::ISystem {
    public:
        //-------------------------------------------------------------
        //! @brief 更新処理
        //-------------------------------------------------------------
        void Update(Tsukino::ECS::Registry& registry, float deltaTime) override;
    };
}    // namespace CombatAndroid::ECS
