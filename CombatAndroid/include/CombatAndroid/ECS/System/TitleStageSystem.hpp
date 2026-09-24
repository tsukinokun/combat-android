//-------------------------------------------------------------
//! @file   TitleStageSystem.hpp
//! @brief  TitleStageSystemクラスの宣言
//-------------------------------------------------------------
#pragma once
#include <Tsukino/Core/ECS/System/ISystem.hpp>

namespace Tsukino::EngineIntegration {
    struct EngineContext;
}    // namespace Tsukino::EngineIntegration

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    struct TitleStageComponent;

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

    private:
        //-------------------------------------------------------------
        //! @brief  「はじめる」の見せ場（武器がカメラへ飛んで弾ける）を1フレーム進める
        //! @param  registry [in,out] ECSレジストリ
        //! @param  context  [in]     エンジンコンテキスト
        //! @param  stage    [in,out] タイトルの演出の状態
        //! @note   白く覆いきったところでロード画面へ切り替える。ロード画面は同じ白から
        //!         明けるので、切り替わりの継ぎ目は見えない
        //-------------------------------------------------------------
        void UpdateLaunch(Tsukino::ECS::Registry& registry, Tsukino::EngineIntegration::EngineContext& context, TitleStageComponent& stage);
    };
}    // namespace CombatAndroid::ECS
