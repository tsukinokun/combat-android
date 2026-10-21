//-------------------------------------------------------------
//! @file   SkillSelectSystem.hpp
//! @brief  SkillSelectSystemクラスの宣言
//! @author 山﨑愛
//-------------------------------------------------------------
#pragma once
#include <Tsukino/Core/ECS/System/ISystem.hpp>

#include <random>
// 前方宣言
namespace Tsukino::ECS {
    class Registry;
}
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @brief  今スキル選択で進行を止めているかを問い合わせる関数
    //! @param  registry [in] ECSレジストリ
    //! @return メニュー表示中、またはレベルアップの予約が残っているならtrue
    //! @note   pendingLevelUpsまで見るのは、レベルアップを積むExpOrbSystemが
    //!         本Systemより後の優先度で走るため。予約が入った次のフレームの頭から
    //!         確実に止められるようにしている。
    //!         シーン（deltaTimeを0にする）と、入力・moveInputを止める各Systemが使う
    //-------------------------------------------------------------
    [[nodiscard]]
    bool IsSkillSelectActive(Tsukino::ECS::Registry& registry);

    //-------------------------------------------------------------
    //! @struct SkillSelectFanExtent
    //! @brief  横に並べたカード全体の広がり（画面ピクセル）
    //-------------------------------------------------------------
    struct SkillSelectFanExtent {
        float halfWidth     = 0.0f;    //!< 画面中心から、端のカードの外側の辺までの横の距離
        float centerOffsetY = 0.0f;    //!< 画面中心から見た、真ん中のカードの中心Y
        float bottomOffsetY = 0.0f;    //!< 画面中心から見た、カードの下端のY（扇で下がった端のカードも含む）
    };

    //-------------------------------------------------------------
    //! @brief  今提示しているカード全体の広がりを得る
    //! @param  registry [in] ECSレジストリ
    //! @return 枚数とAssets/Tables/Systems/SkillSelect.json の寸法から求めた広がり
    //! @note   カードの左右・下に操作の案内を並べるInputPromptSystemが使う
    //-------------------------------------------------------------
    [[nodiscard]]
    SkillSelectFanExtent GetSkillSelectFanExtent(Tsukino::ECS::Registry& registry);

    //-------------------------------------------------------------
    //! @class  SkillSelectSystem
    //! @brief  レベルアップ時にスキルの選択肢を提示し、メニューの上下左右（キー・ホイール・十字キー・左スティック）での選択と
    //!         決定（F・Enter・A・クリック）を処理するシステム
    //! @note   PlayerSystem（Movement優先度）が同じフレームの入力を消費する前に
    //!         割り込む必要があるため、SkillSelect優先度（Transformの直後）で登録する
    //-------------------------------------------------------------
    class SkillSelectSystem : public Tsukino::ECS::ISystem {
    public:
        //-------------------------------------------------------------
        //! @brief 更新処理
        //-------------------------------------------------------------
        void Update(Tsukino::ECS::Registry& registry, float deltaTime) override;

    private:
        //! 選択肢の抽選に使う乱数生成器（EnemySpawnDirectorSystemと同じ流儀）
        std::mt19937 m_rng{std::random_device{}()};
    };
}    // namespace CombatAndroid::ECS
