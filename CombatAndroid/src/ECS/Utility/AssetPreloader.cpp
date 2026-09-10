//-------------------------------------------------------------
//! @file    AssetPreloader.cpp
//! @brief   ゲーム開始前にアセットをまとめてロードする処理の実装
//! @author  山﨑愛
//-------------------------------------------------------------
#include <CombatAndroid/ECS/Utility/AssetPreloader.hpp>

#include <CombatAndroid/ECS/Utility/WeaponSpawner.hpp>
#include <CombatAndroid/ECS/Utility/WeaponTable.hpp>
#include <CombatAndroid/ECS/Utility/SkillTable.hpp>
#include <CombatAndroid/ECS/Utility/EnemySpawner.hpp>

#include <Tsukino/EngineIntegration/EngineContext.hpp>
#include <Tsukino/Engine/Asset/AssetManager.hpp>
#include <Tsukino/Core/Path.hpp>

#include <hlsl++.h>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    namespace {
        //-------------------------------------------------------------
        // 武器・敵・スキルのどのテーブルにも属さない、システム単体の固定アセット。
        // 新しく増えたら（草原・ヒット音のような、テーブル駆動ではない単発の演出を
        // 足したとき）ここへ1行足すこと
        //-------------------------------------------------------------
        constexpr const char* kMiscPreloadPaths[] = {
            "CombatAndroid/Assets/Shaders/Grass.vs.hlsl",
            "CombatAndroid/Assets/Audio/HitImpactBlunt.wav",
            "CombatAndroid/Assets/Audio/HitImpactSharp.wav",
        };

        //-------------------------------------------------------------
        //! @brief 武器1種類ぶんのモデル・攻撃クリップ・エフェクトをロードする
        //-------------------------------------------------------------
        void PreloadWeapon(Tsukino::Asset::AssetManager& assetManager, const WeaponSpawnDefinition& def) {
            assetManager.Load(Tsukino::Core::Path(def.modelPath));

            if(def.playerAttackClipPath)
                assetManager.Load(Tsukino::Core::Path(def.playerAttackClipPath));

            if(def.areaAttackRadius > 0.0f && def.areaAttackEffectPath)
                assetManager.Load(Tsukino::Core::Path(def.areaAttackEffectPath));

            if(def.projectileEffectPath)
                assetManager.Load(Tsukino::Core::Path(def.projectileEffectPath));
        }
    }    // namespace

    //-------------------------------------------------------------
    //! @brief アセットをまとめて事前ロードする
    //-------------------------------------------------------------
    void PreloadAssets(Tsukino::EngineIntegration::EngineContext& context) {
        if(!context.assetManager)
            return;

        Tsukino::Asset::AssetManager& assetManager = *context.assetManager;

        //-------------------------------------------------------------
        // 武器：全種の見た目・攻撃クリップ・エフェクトをWeaponSpawnDefinitionから読む
        //-------------------------------------------------------------
        for(int i = 0; i < static_cast<int>(WeaponId::Count); ++i)
            PreloadWeapon(assetManager, GetWeaponSpawnDefinition(static_cast<WeaponId>(i)));

        //-------------------------------------------------------------
        // スキル：カード背景テクスチャ
        //-------------------------------------------------------------
        for(const SkillTableEntry& entry : GetSkillTable())
            assetManager.Load(Tsukino::Core::Path(entry.backgroundTexturePath));

        //-------------------------------------------------------------
        // 敵：モデル・アニメーションクリップ。生成パラメータを作るだけで
        // 中のLoad呼び出しが走るので、戻り値は使わず捨てる。
        // Paladinは持たせる武器で攻撃クリップが変わるため、全WeaponId分呼ぶ
        //-------------------------------------------------------------
        const hlslpp::float3 dummyPosition(0.0f, 0.0f, 0.0f);

        (void)MakeSmallZombieConfig(context, dummyPosition);
        (void)MakeBigZombieConfig(context, dummyPosition);

        for(int i = 0; i < static_cast<int>(WeaponId::Count); ++i)
            (void)MakePaladinConfig(context, dummyPosition, static_cast<WeaponId>(i));

        //-------------------------------------------------------------
        // どのテーブルにも属さない単発アセット
        //-------------------------------------------------------------
        for(const char* path : kMiscPreloadPaths)
            assetManager.Load(Tsukino::Core::Path(path));
    }
}    // namespace CombatAndroid::ECS
