//-------------------------------------------------------------
//! @file   PlayerSkillComponent.hpp
//! @brief  PlayerSkillComponent構造体の宣言
//! @author 山﨑愛
//-------------------------------------------------------------
#pragma once
#include <CombatAndroid/ECS/Utility/SkillTable.hpp>

#include <array>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @struct PlayerSkillComponent
    //! @brief  プレイヤーが取得済みのスキルと、そこから導出した実効値
    //! @note   死亡してリトライするとGameOverSystemがシーンごと作り直すため、
    //!         既定値（未取得・倍率1.0・回復0）がそのままリセットになる。
    //!         セーブ／ロードは持たない
    //-------------------------------------------------------------
    struct PlayerSkillComponent {
        //! 各スキルの取得段階。0=未取得、上限はkMaxSkillLevel。添字はSkillIdをintにキャストした値
        std::array<int, static_cast<size_t>(SkillId::Count)> levels{};

        //-------------------------------------------------------------
        // 以下はテーブルから導出したキャッシュ。スキル取得のたびに
        // RecalculateSkillStatsが levels から丸ごと計算し直す。
        // 参照側（CombatSystemはヒットごと・敵ごと・フレームごとに読む）で
        // 毎回テーブルを走査しないための値であって、状態の実体はlevelsの方
        //-------------------------------------------------------------
        float expGainMultiplier = 1.0f;    //!< 強欲：ExpOrbSystemが吸収時のexpValueに掛ける
        float healPerSoul       = 0.0f;    //!< 暴食：ExpOrbSystemが吸収時に回復するHP

        //! 憤怒（上げる）と怠惰（下げる）の両方が書き込む唯一の値。
        //! そのためRecalculateSkillStats側は代入ではなく乗算で積んでいる（テーブルの並び順に依存させないため）
        float attackMultiplier = 1.0f;    //!< CombatSystemが与ダメージに掛ける

        float damageTakenMultiplier = 1.0f;    //!< 傲慢：CombatSystemが被ダメージに掛ける（1.0未満で軽減）
        float lifeStealRatio        = 0.0f;    //!< 嫉妬：CombatSystemが与ダメージに掛けてHPへ変換する割合
        float moveSpeedMultiplier   = 1.0f;    //!< 色欲：PlayerSystemが移動速度に掛ける
        float healPerSecond         = 0.0f;    //!< 怠惰：PlayerSystemが毎秒回復させるHP
    };

    //-------------------------------------------------------------
    //! @brief  levelsから実効値のキャッシュを計算し直す関数
    //! @param  skills [in,out] 対象のコンポーネント。levelsを読み、残りのfloat3つを書く
    //! @note   加算で積み上げるのではなく毎回テーブルから引き直しているのは、
    //!         テーブルの数値を後から調整しても取得済みの効果がずれないようにするため。
    //!         定義はSkillTable.cpp側（テーブルの中身を知っている唯一の場所に置く）
    //-------------------------------------------------------------
    void RecalculateSkillStats(PlayerSkillComponent& skills);
}    // namespace CombatAndroid::ECS
