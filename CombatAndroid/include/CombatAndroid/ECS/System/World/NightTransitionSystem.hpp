//-------------------------------------------------------------
//! @file   NightTransitionSystem.hpp
//! @brief  NightTransitionSystemクラスの宣言
//-------------------------------------------------------------
#pragma once
#include <Tsukino/Core/ECS/System/ISystem.hpp>

#include <hlsl++.h>

namespace Tsukino::EngineIntegration {
    struct EngineContext;    // 前方宣言
}

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @class  NightTransitionSystem
    //! @brief  ボス戦の昼夜の切り替えを進めるシステム。
    //!         夜は、エンジンの球形の色調補正（SphereColorGradeComponent）の球をボスの位置から広げて
    //!         世界を塗り替え、並行して太陽・空・霧の値を夜へ寄せる。明けるときは補正を薄めながら値を昼へ戻す
    //! @note   時間は実時間で進める（出現のカットシーンで世界が止まっている間に夜が広がるため）。
    //!         調整値は Assets/Tables/Systems/NightTransition.json
    //-------------------------------------------------------------
    class NightTransitionSystem : public Tsukino::ECS::ISystem {
    public:
        //-------------------------------------------------------------
        //! @brief 更新処理
        //-------------------------------------------------------------
        void Update(Tsukino::ECS::Registry& registry, float deltaTime) override;
    };

    //-------------------------------------------------------------
    //! @brief  夜への切り替えを始める
    //! @param  registry [in,out] ECSレジストリ
    //! @param  center   [in]     夜が広がり始める中心（ワールド）
    //! @note   既に夜なら中心だけ替えて何もしない
    //-------------------------------------------------------------
    void RequestNightfall(Tsukino::ECS::Registry& registry, const hlslpp::float3& center);

    //-------------------------------------------------------------
    //! @brief  夜明けを始める
    //! @param  registry [in,out] ECSレジストリ
    //-------------------------------------------------------------
    void RequestDaybreak(Tsukino::ECS::Registry& registry);

    //-------------------------------------------------------------
    //! @brief  夜霧の濃さの倍率を変える（ボスの第2形態で霧を濃くする）
    //! @param  registry [in,out] ECSレジストリ
    //! @param  scale    [in]     NightTransition.json の fogDensity に掛ける倍率
    //-------------------------------------------------------------
    void SetNightFogScale(Tsukino::ECS::Registry& registry, float scale);

    //-------------------------------------------------------------
    //! @brief  環境光（空から焼いたIBL）を次の描画で焼き直させる
    //! @param  context [in] エンジンコンテキスト
    //! @note   Rendererはシーンをまたいで1つで、IBLは頼まれない限り焼き直されない。
    //!         夜の空で焼いたものを次のシーンへ持ち越さないよう、環境を作るとき（InstantiateEnvironment）にも呼ぶ
    //-------------------------------------------------------------
    void RequestAmbientRecapture(Tsukino::EngineIntegration::EngineContext& context);
}    // namespace CombatAndroid::ECS
