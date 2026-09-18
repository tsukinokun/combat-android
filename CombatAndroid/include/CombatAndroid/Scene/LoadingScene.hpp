//-------------------------------------------------------------
//! @file    LoadingScene.hpp
//! @brief   戦闘シーンの前に挟むロード画面のシーンの宣言
//-------------------------------------------------------------
#pragma once
#include <Tsukino/EngineIntegration/Scene/GameSceneBase.hpp>
#include <Tsukino/Core/ECS/Entity/Entity.hpp>

#include <atomic>
#include <functional>
#include <thread>
#include <vector>

// 名前空間 : CombatAndroid
namespace CombatAndroid {
    //-------------------------------------------------------------
    //! @class   LoadingScene
    //! @brief   タイトルの「はじめる」から戦闘シーンへ移る間に入るロード画面。
    //!          戦闘で使うアセット（AssetPreloaderの列）を裏スレッドで読み、その間は
    //!          「NOW LOADING」・回る印・進捗バーを動かし続ける。読み終えたら戦闘シーンへ切り替える
    //! @note    シーン切り替えは同期（GameSceneManagerが次シーンのInitializeをその場で呼ぶ）なので、
    //!          読み込みを戦闘シーンの初期化で行うと画面が止まる。ここで先に読んでおけば、
    //!          AssetManagerのキャッシュに残るため、戦闘シーン側のPreloadAssetsは表を引くだけになる
    //-------------------------------------------------------------
    class LoadingScene : public Tsukino::EngineIntegration::GameSceneBase {
    public:
        //-------------------------------------------------------------
        //! @brief  コンストラクタ
        //! @param  showTutorial [in] 読み終えた後の戦闘シーンで操作の案内を出すか
        //-------------------------------------------------------------
        explicit LoadingScene(bool showTutorial)
            : m_showTutorial(showTutorial) {
        }

        //-------------------------------------------------------------
        //! @brief  デストラクタ（読み込み中なら止めて待つ）
        //-------------------------------------------------------------
        ~LoadingScene() override;

        //-------------------------------------------------------------
        //! @brief  シーンの更新
        //! @param  api       [in] エンジンから提供されるAPIへの参照
        //! @param  deltaTime [in] 前フレームからの経過時間
        //-------------------------------------------------------------
        void OnUpdate(Tsukino::EngineIntegration::EngineAPI& api, float deltaTime) override;

        //-------------------------------------------------------------
        //! @brief  シーンの終了処理（読み込み中なら止めて待つ）
        //-------------------------------------------------------------
        void OnExit() override;

    private:
        //-------------------------------------------------------------
        //! @brief  シーン固有の初期化処理
        //! @param  api [in] エンジンから提供されるAPIへの参照
        //-------------------------------------------------------------
        void OnInitialize(Tsukino::EngineIntegration::EngineAPI& api) override;

        //-------------------------------------------------------------
        //! @brief  読み込みの裏スレッドを止めて合流する
        //! @note   残りのステップは飛ばす（途中で抜けても、読み終えたアセットはキャッシュに残る）
        //-------------------------------------------------------------
        void StopWorker();

        //-------------------------------------------------------------
        //! @brief  画面（文字・回る印・進捗バー）を今の状態で組み直す
        //-------------------------------------------------------------
        void RefreshUi();

        bool m_showTutorial = false;    //!< 戦闘シーンで操作の案内を出すか

        std::vector<std::function<void()>> m_steps;             //!< 裏スレッドで実行する読み込みの列
        std::thread                        m_worker;            //!< 読み込みの裏スレッド
        std::atomic<int>                   m_completedSteps{0}; //!< 終わったステップ数（進捗バー用）
        std::atomic<bool>                  m_finished{false};   //!< 全ステップが終わったか
        std::atomic<bool>                  m_cancel{false};     //!< 途中で止めるよう頼まれたか

        float m_elapsed          = 0.0f;     //!< 表示してからの経過秒数（アニメーションと最低表示時間）
        float m_displayedProgress = 0.0f;    //!< 画面に出している進捗（実際の進捗へ滑らかに追いつかせる）
        bool  m_changeRequested  = false;    //!< 戦闘シーンへの切り替えを頼んだか

        Tsukino::ECS::Entity m_backdropEntity    = entt::null;    //!< 背景の板
        Tsukino::ECS::Entity m_labelEntity       = entt::null;    //!< 「NOW LOADING...」
        Tsukino::ECS::Entity m_percentEntity     = entt::null;    //!< 「42%」
        Tsukino::ECS::Entity m_barTrackEntity    = entt::null;    //!< 進捗バーの溝
        Tsukino::ECS::Entity m_barFillEntity     = entt::null;    //!< 進捗バーの中身
        Tsukino::ECS::Entity m_spinnerEntities[3] = {entt::null, entt::null, entt::null};    //!< 回る印（3枚の板）
    };
}    // namespace CombatAndroid
