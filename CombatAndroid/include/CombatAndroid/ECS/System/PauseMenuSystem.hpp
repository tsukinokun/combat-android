//-------------------------------------------------------------
//! @file   PauseMenuSystem.hpp
//! @brief  PauseMenuSystemクラスの宣言
//-------------------------------------------------------------
#pragma once
#include <Tsukino/Core/ECS/System/ISystem.hpp>
// 前方宣言
namespace Tsukino::ECS {
    class Registry;
}
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @brief  今ポーズで進行を止めているかを問い合わせる関数
    //! @param  registry [in] ECSレジストリ
    //! @return メニュー表示中、または閉じた直後の1フレームならtrue
    //-------------------------------------------------------------
    [[nodiscard]]
    bool IsPauseMenuActive(Tsukino::ECS::Registry& registry);

    //-------------------------------------------------------------
    //! @class  PauseMenuSystem
    //! @brief  走行中にEscでポーズメニュー（再開／リトライ／タイトルへ）を開くシステム。
    //!         開いている間はゲームの進行を止め（IsGameplayFrozen）、カーソルを解放する
    //! @note   PlayerSystem（Movement優先度）が同じフレームの入力を消費する前に
    //!         割り込む必要があるため、SkillSelectの直後の優先度で登録する。
    //!         スキル選択中と、走行が終わった後（死亡・クリア）は開かない
    //-------------------------------------------------------------
    class PauseMenuSystem : public Tsukino::ECS::ISystem {
    public:
        //-------------------------------------------------------------
        //! @brief 更新処理
        //! @param registry  [in] エンジンのECSレジストリのラッパー
        //! @param deltaTime [in] デルタタイム（表示中は0。メニューは時間を使わない）
        //-------------------------------------------------------------
        void Update(Tsukino::ECS::Registry& registry, float deltaTime) override;
    };
}    // namespace CombatAndroid::ECS
