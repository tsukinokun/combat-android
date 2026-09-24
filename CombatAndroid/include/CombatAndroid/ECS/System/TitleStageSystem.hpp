//-------------------------------------------------------------
//! @file   TitleStageSystem.hpp
//! @brief  TitleStageSystemクラスの宣言
//-------------------------------------------------------------
#pragma once
#include <Tsukino/Core/ECS/System/ISystem.hpp>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @class  TitleStageSystem
    //! @brief  タイトル画面の3D演出を進めるシステム。TitleSceneにだけ登録する
    //! @note   TitleStageComponentが持つ武器を、決めた時刻に1本ずつ地面から
    //!         抜けさせ（土煙＋効果音）、抜けきったら宙でゆっくり回して漂わせる。
    //!         カメラも止まって見えないよう、基準位置の周りを小さく往復させる
    //-------------------------------------------------------------
    class TitleStageSystem : public Tsukino::ECS::ISystem {
    public:
        //-------------------------------------------------------------
        //! @brief 更新処理
        //! @param registry  [in] エンジンのECSレジストリのラッパー
        //! @param deltaTime [in] デルタタイム
        //-------------------------------------------------------------
        void Update(Tsukino::ECS::Registry& registry, float deltaTime) override;
    };
}    // namespace CombatAndroid::ECS
