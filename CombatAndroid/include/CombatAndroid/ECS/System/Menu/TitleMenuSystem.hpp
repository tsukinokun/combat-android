//-------------------------------------------------------------
//! @file   TitleMenuSystem.hpp
//! @brief  TitleMenuSystemクラスの宣言
//-------------------------------------------------------------
#pragma once
#include <Tsukino/Core/ECS/System/ISystem.hpp>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @class  TitleMenuSystem
    //! @brief  タイトル画面のメニュー（はじめる／操作説明／終了）を表示・操作するシステム。
    //!         TitleSceneにだけ登録する
    //! @note   ベスト記録は画面を組むたびにファイルから読み直す。
    //!         リザルトから戻ってきたときに、直前のプレイの更新が反映されている必要があるため
    //-------------------------------------------------------------
    class TitleMenuSystem : public Tsukino::ECS::ISystem {
    public:
        //-------------------------------------------------------------
        //! @brief 更新処理
        //! @param registry  [in] エンジンのECSレジストリのラッパー
        //! @param deltaTime [in] デルタタイム（メニューは時間を使わない）
        //-------------------------------------------------------------
        void Update(Tsukino::ECS::Registry& registry, float deltaTime) override;
    };
}    // namespace CombatAndroid::ECS
