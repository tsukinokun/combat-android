//-------------------------------------------------------------
//! @file   CutsceneActorComponent.hpp
//! @brief  CutsceneActorComponent構造体の宣言
//-------------------------------------------------------------
#pragma once
#include <Tsukino/Engine/Asset/AssetHandle.hpp>

#include <vector>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @struct CutsceneActorStep
    //! @brief  役者の動き1つぶん（このクリップをこの秒数流し、その間この速さで前へ進む）
    //-------------------------------------------------------------
    struct CutsceneActorStep {
        Tsukino::Asset::AssetHandle clip;                      //!< 流すクリップ
        int                         animationIndex = -1;      //!< クリップの中のアニメーション番号。負ならPrefabのanimation_index
        bool                        looping        = true;    //!< ループ再生するか
        float                       duration       = 1.0f;    //!< この動きを続ける秒数
        float                       forwardSpeed   = 0.0f;    //!< 向いている方へ進む速さ（ユニット/秒）
        float                       riseSpeed      = 0.0f;    //!< 上へ動く速さ（ユニット/秒）。跳んで画面の外へ去るのに使う
        float                       moveDelay      = 0.0f;    //!< この動きに入ってから動き始めるまでの秒（跳ぶ前のしゃがみ）
    };

    //-------------------------------------------------------------
    //! @struct CutsceneActorComponent
    //! @brief  ムービーにだけ出る、戦わない見た目の役者（暴れる蜘蛛・見下ろす伯爵）の動きの台本
    //! @note   SpawnCutsceneActor（CutsceneActorSystem.hpp）が付け、CutsceneActorSystemが進める。
    //!         敵の部品（EnemyComponent等）を持たないので、ほかのSystemからは敵として扱われない
    //-------------------------------------------------------------
    struct CutsceneActorComponent {
        std::vector<CutsceneActorStep> steps;    //!< 動きの台本（最後まで行ったら頭から繰り返す）
        int   currentStep = -1;      //!< 今の動きの番号（-1で未開始）
        float stepTimer   = 0.0f;    //!< 今の動きに入ってからの秒
        float elapsed     = 0.0f;    //!< 出てからの秒

        float fadeStart    = -1.0f;    //!< 出てからこの秒数で霧に消え始める。負なら消えない
        float fadeDuration = 1.0f;     //!< 消えきるまでの秒数
        int   despawnAtShot = -1;      //!< ムービーがこのショット番号に入ったら片付ける。負ならムービーの終わりまで残す
        float blendTime     = 0.2f;    //!< 動きを切り替えるときのクロスフェードの秒数
    };
}    // namespace CombatAndroid::ECS
