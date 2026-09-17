//-------------------------------------------------------------
//! @file   RunResultSystem.hpp
//! @brief  RunResultSystemクラスの宣言
//-------------------------------------------------------------
#pragma once
#include <CombatAndroid/ECS/Event/EnemyDiedEvent.hpp>

#include <Tsukino/Core/ECS/System/ISystem.hpp>
#include <Tsukino/Core/ECS/Event/EventBus.hpp>
#include <Tsukino/Core/ECS/Event/ScopedConnection.hpp>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @brief  走行が終わった（死亡・クリア）かを問い合わせる関数
    //! @param  registry [in] ECSレジストリ
    //! @return true: 死亡したかクリアした
    //! @note   ポーズメニューとスキル選択が「終わった後は開かない」判定に使う
    //-------------------------------------------------------------
    [[nodiscard]]
    bool IsRunEnded(Tsukino::ECS::Registry& registry);

    //-------------------------------------------------------------
    //! @brief  リザルトで進行を止めているかを問い合わせる関数
    //! @param  registry [in] ECSレジストリ
    //! @return true: クリアした（その瞬間から止める）か、リザルトを表示している
    //! @note   死亡した場合は、死亡モーションを見せる間は止めず、リザルトを出してから止める
    //-------------------------------------------------------------
    [[nodiscard]]
    bool IsRunResultFreezing(Tsukino::ECS::Registry& registry);

    //-------------------------------------------------------------
    //! @class  RunResultSystem
    //! @brief  走行の終わりを判定してリザルトを出すシステム。
    //!         プレイヤーが死ぬか、kRunClearSeconds を生き延びたら走行を終え、
    //!         少し待ってから成績（生存時間・撃破数・到達レベル・危険度・取ったスキル）と
    //!         ベスト記録を表示する。記録はその時点で保存し、メニューでリトライかタイトルへ戻る
    //! @note   撃破数は EnemyDiedEvent を数えて出す
    //-------------------------------------------------------------
    class RunResultSystem : public Tsukino::ECS::ISystem {
    public:
        //-------------------------------------------------------------
        //! @brief 更新処理
        //! @param registry  [in] エンジンのECSレジストリのラッパー
        //! @param deltaTime [in] デルタタイム（待ち時間は実時間で数えるため使わない）
        //-------------------------------------------------------------
        void Update(Tsukino::ECS::Registry& registry, float deltaTime) override;

        //-------------------------------------------------------------
        //! @brief EnemyDiedEventの購読を開始する
        //! @param eventBus [in] シーンが所有するイベントバス
        //! @note  EventBusはSystemManagerより先に宣言されており購読者より長生きするため、
        //!        解除はm_diedConnectionのデストラクタに任せてよい
        //-------------------------------------------------------------
        void Initialize(Tsukino::ECS::EventBus& eventBus);

    private:
        int                            m_pendingKills = 0;    //!< 次のUpdateで撃破数へ足す数
        Tsukino::ECS::ScopedConnection m_diedConnection;      //!< EnemyDiedEventの購読
    };
}    // namespace CombatAndroid::ECS
