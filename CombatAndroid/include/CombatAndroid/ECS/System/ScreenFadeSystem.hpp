//-------------------------------------------------------------
//! @file   ScreenFadeSystem.hpp
//! @brief  ScreenFadeSystemクラスの宣言
//-------------------------------------------------------------
#pragma once
#include <Tsukino/Core/ECS/System/ISystem.hpp>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @class  ScreenFadeSystem
    //! @brief  場面の切り替わりの黒フェードを進めるシステム。全シーンに登録する
    //! @note   シーンに入った直後は黒から明け、RequestSceneChangeWithFadeが呼ばれたら
    //!         黒く覆い、覆いきったところでGameSceneManagerへ切り替えを頼む。
    //!         登録の優先度はTransformUIより前にすること（黒い板の位置を書いた後に
    //!         worldMatrixへ反映される必要がある）
    //-------------------------------------------------------------
    class ScreenFadeSystem : public Tsukino::ECS::ISystem {
    public:
        //-------------------------------------------------------------
        //! @brief 更新処理
        //! @param registry  [in] エンジンのECSレジストリのラッパー
        //! @param deltaTime [in] デルタタイム
        //! @note  ポーズ中・リザルト中はdeltaTimeに0が来るため、経過時間は
        //!        実時間（EngineContext::deltaTime）で測る
        //-------------------------------------------------------------
        void Update(Tsukino::ECS::Registry& registry, float deltaTime) override;
    };
}    // namespace CombatAndroid::ECS
