//-------------------------------------------------------------
//! @file    TitleScene.hpp
//! @brief   タイトル画面のシーンの宣言
//-------------------------------------------------------------
#pragma once
#include <Tsukino/EngineIntegration/Scene/GameSceneBase.hpp>
#include <Tsukino/Core/ECS/Registry/Registry.hpp>

namespace Tsukino::EngineIntegration {
    struct EngineContext;
}    // namespace Tsukino::EngineIntegration

// 名前空間 : CombatAndroid
namespace CombatAndroid {
    //-------------------------------------------------------------
    //! @class   TitleScene
    //! @brief   起動直後と、ポーズ・リザルトから「タイトルへ」を選んだときに入るシーン。
    //!          ゲーム名・メニュー（はじめる／操作説明／終了）・ベスト記録を出す。
    //!          メニューの操作はTitleMenuSystemが受け持つ
    //-------------------------------------------------------------
    class TitleScene : public Tsukino::EngineIntegration::GameSceneBase {
    public:
        //-------------------------------------------------------------
        //! @brief  コンストラクタ
        //-------------------------------------------------------------
        TitleScene() = default;

        //-------------------------------------------------------------
        //! @brief  デストラクタ
        //-------------------------------------------------------------
        ~TitleScene() override = default;

        //-------------------------------------------------------------
        //! @brief  シーンの更新
        //! @param  api       [in] エンジンから提供されるAPIへの参照
        //! @param  deltaTime [in] 前フレームからの経過時間
        //-------------------------------------------------------------
        void OnUpdate(Tsukino::EngineIntegration::EngineAPI& api, float deltaTime) override;

        //-------------------------------------------------------------
        //! @brief  シーンの終了処理
        //-------------------------------------------------------------
        void OnExit() override;

    private:
        //-------------------------------------------------------------
        //! @brief  シーン固有の初期化処理
        //! @param  api [in] エンジンから提供されるAPIへの参照
        //-------------------------------------------------------------
        void OnInitialize(Tsukino::EngineIntegration::EngineAPI& api) override;

        //-------------------------------------------------------------
        //! @brief  背景の3D（カメラ・夕日・空・霧・地面・草・武器）を組む
        //! @param  registry [in,out] ECSレジストリ
        //! @param  context  [in]     エンジンコンテキスト
        //! @note   演出そのものはTitleStageSystemが進める。ここは置くだけ
        //-------------------------------------------------------------
        void BuildStage(Tsukino::ECS::Registry& registry, Tsukino::EngineIntegration::EngineContext& context);
    };
}    // namespace CombatAndroid
