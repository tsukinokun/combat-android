//-------------------------------------------------------------
//! @file    AssetPreloader.hpp
//! @brief   ゲーム開始前にアセットをまとめてロードする処理の宣言
//! @author  山﨑愛
//-------------------------------------------------------------
#pragma once

// 前方宣言
namespace Tsukino::EngineIntegration {
    struct EngineContext;
}

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @brief  武器・敵・スキル・システム単体のアセットをまとめて事前ロードする関数
    //! @param  context [in] エンジンコンテキスト（AssetManagerの取得に使う）
    //! @note   AssetManager::Load はパスでキャッシュされるため、ここで一度読んでおけば
    //!         以降の「使う瞬間に遅延ロードする」既存コード（EnemySpawner/WeaponSpawner/
    //!         SkillSelectSystem/GrassFieldSystem/HitSoundSystem等）はキャッシュヒットになり、
    //!         戦闘中や初回表示のタイミングで重いインポート処理が走らなくなる。
    //!
    //!         新しいアセットを追加するときの置き場所:
    //!         - 武器の見た目・モーション・エフェクト → WeaponSpawner.cpp の
    //!           kWeaponSpawnTable に1行足せば、この関数が自動的に拾う
    //!         - スキルのアイコン → SkillTable.cpp の kSkillTable に1行足せば同様
    //!         - 敵のモデル・アニメーション → EnemySpawner.cpp の各 MakeXxxConfig に
    //!           パスを足せば同様（呼び出し元を増やす必要はない）
    //!         - 上記どれにも属さないシステム単体のアセット（シェーダー・効果音等）は
    //!           この関数の実装内にある固定リストへ1行足すこと
    //-------------------------------------------------------------
    void PreloadAssets(Tsukino::EngineIntegration::EngineContext& context);
}    // namespace CombatAndroid::ECS
