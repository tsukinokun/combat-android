//-------------------------------------------------------------
//! @file   TutorialSystem.hpp
//! @brief  TutorialSystemクラスの宣言
//-------------------------------------------------------------
#pragma once
#include <Tsukino/Core/ECS/System/ISystem.hpp>
#include <Tsukino/Core/ECS/Registry/Registry.hpp>

namespace Tsukino::EngineIntegration {
    struct EngineContext;
}    // namespace Tsukino::EngineIntegration

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @brief  操作の案内の状態とUI一式を作る
    //! @param  registry [in,out] ECSレジストリ
    //! @param  context  [in]     エンジンコンテキスト
    //! @note   CombatAndroidSceneがタイトル経由で作られたときだけ呼ぶ（リトライでは呼ばない）
    //-------------------------------------------------------------
    void CreateTutorial(Tsukino::ECS::Registry& registry, Tsukino::EngineIntegration::EngineContext& context);

    //-------------------------------------------------------------
    //! @class  TutorialSystem
    //! @brief  タイトルから始めた走行の最初に、操作を1つずつ案内するシステム
    //! @note   画面下に「キー＋説明」を1手順ずつ出し、プレイヤーがその操作をしたら
    //!         「OK」を一瞬出して次へ進む。できなくても一定時間で次へ進むので、
    //!         案内で足止めはしない。手順の中身とできたかの判定はTutorialSystem.cppの表にある。
    //!         スキル選択・ポーズ・リザルト中は止めて隠す（IsGameplayFrozen）。
    //!         TutorialComponentが無い（リトライで始めた）ときは何もしない
    //-------------------------------------------------------------
    class TutorialSystem : public Tsukino::ECS::ISystem {
    public:
        //-------------------------------------------------------------
        //! @brief 更新処理
        //! @param registry  [in] エンジンのECSレジストリのラッパー
        //! @param deltaTime [in] デルタタイム（メニュー中は0が来る）
        //-------------------------------------------------------------
        void Update(Tsukino::ECS::Registry& registry, float deltaTime) override;
    };
}    // namespace CombatAndroid::ECS
