//-------------------------------------------------------------
//! @file    CombatAndroidScene.hpp
//! @brief   CombatAndroidのメインゲームシーンの宣言
//! @author  山﨑愛
//-------------------------------------------------------------
#pragma once
#include <Tsukino/EngineIntegration/Scene/GameSceneBase.hpp>

// 前方宣言（RegisterSystemsの引数にしか使わないため、ヘッダの依存を増やさない）
namespace Tsukino::EngineIntegration {
    struct EngineContext;
}
namespace Tsukino::ECS {
    class EventBus;
}
// 名前空間 : CombatAndroid
namespace CombatAndroid {
    //-------------------------------------------------------------
    //! @class   CombatAndroidScene
    //! @brief   CombatAndroidのメインゲームシーン
    //-------------------------------------------------------------
    class CombatAndroidScene : public Tsukino::EngineIntegration::GameSceneBase {
    public:
        //-------------------------------------------------------------
        //! @brief  コンストラクタ
        //-------------------------------------------------------------
        CombatAndroidScene() = default;

        //-------------------------------------------------------------
        //! @brief  デストラクタ
        //-------------------------------------------------------------
        ~CombatAndroidScene() override = default;

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
        //! @brief  シーンに全てのシステムを登録する
        //! @detail 実装は CombatAndroidSceneSystems.cpp。実行順は
        //!         CombatAndroid/ECS/SystemPriority.hpp を参照。
        //! @param  context  [in] エンジンコンテキスト
        //! @param  eventBus [in] シーンのイベントバス
        //-------------------------------------------------------------
        void RegisterSystems(Tsukino::EngineIntegration::EngineContext* context, Tsukino::ECS::EventBus& eventBus);

    private:
        //-------------------------------------------------------------
        //! @enum class GameState
        //-------------------------------------------------------------
        enum class GameState {
            Playing,    //!< プレイ中
            TimeUp      //!< 時間切れ。スペースキー待機中
        };

        GameState m_gameState = GameState::Playing;    //!< ゲームの状態
    };
}    // namespace CombatAndroid
