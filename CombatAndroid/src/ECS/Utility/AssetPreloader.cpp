//-------------------------------------------------------------
//! @file    AssetPreloader.cpp
//! @brief   ゲーム開始前にアセットをまとめてロードする処理の実装
//! @author  山﨑愛
//-------------------------------------------------------------
#include <CombatAndroid/ECS/Utility/AssetPreloader.hpp>

#include <CombatAndroid/ECS/Component/EnemyAnimationSetComponent.hpp>
#include <CombatAndroid/ECS/Component/WeaponComponent.hpp>
#include <CombatAndroid/ECS/Serialization/EnemyComponentSerialization.hpp>
#include <CombatAndroid/ECS/Serialization/WeaponComponentSerialization.hpp>
#include <CombatAndroid/ECS/Utility/GamePrefab.hpp>
#include <CombatAndroid/ECS/Utility/WeaponSpawner.hpp>
#include <CombatAndroid/ECS/Utility/WeaponTable.hpp>
#include <CombatAndroid/ECS/Utility/SkillTable.hpp>
#include <CombatAndroid/ECS/Utility/EnemySpawner.hpp>
#include <CombatAndroid/ECS/Utility/EnemySpawnTable.hpp>
#include <CombatAndroid/ECS/Utility/SoundTable.hpp>
#include <CombatAndroid/ECS/Utility/Bgm.hpp>

#include <Tsukino/BuiltIn/ECS/Component/ModelComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Serialization/ModelComponentSerialization.hpp>
#include <Tsukino/EngineIntegration/EngineContext.hpp>
#include <Tsukino/Engine/Asset/AssetManager.hpp>
#include <Tsukino/Engine/Asset/AssetRef.hpp>
#include <Tsukino/Engine/ECS/Prefab/PrefabFactory.hpp>
#include <Tsukino/Core/IO/FileSystem.hpp>
#include <Tsukino/Core/Path.hpp>

#include <hlsl++.h>

#include <string>

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
            "CombatAndroid/Assets/Shaders/Ground.vs.hlsl",
            "CombatAndroid/Assets/Audio/HitImpactBlunt.wav",
            "CombatAndroid/Assets/Audio/HitImpactSharp.wav",
            "CombatAndroid/Assets/Audio/ChargeFire.wav",
            "CombatAndroid/Assets/Textures/Ground/DirtGround.bmp",
            "CombatAndroid/Assets/Textures/UI/ExpOrb.png",
            "CombatAndroid/Assets/Textures/UI/WhitePixel.png",

            // タイトル画面の武器が抜けるときの土煙（TitleStageSystem）
            "CombatAndroid/Assets/Effect/greatswordAttackCombo3.efkefc",

            // プレイヤー（CombatAndroidScene::OnInitializeがPrefab: Playerから生成する）。ロード画面で先に読んでおけば、
            // 戦闘シーンの初期化はキャッシュから引くだけになり、切り替えの瞬間に止まらない
            "CombatAndroid/Assets/Models/Player.fbx",
            "CombatAndroid/Assets/Anims/Player/Idle.fbx",
            "CombatAndroid/Assets/Anims/Player/Run.fbx",
            "CombatAndroid/Assets/Anims/Player/Fast Run.fbx",
            "CombatAndroid/Assets/Anims/Player/Sprinting Forward Roll.fbx",
            "CombatAndroid/Assets/Anims/Player/Hammer Attack.fbx",
            "CombatAndroid/Assets/Anims/Player/Falling Back Death.fbx",
        };

        //-------------------------------------------------------------
        //! @brief  PrefabのComponent JSONを1つ読み、中のアセットパスを列へ足す
        //! @param  context  [in]     エンジンコンテキスト
        //! @param  prefab   [in]     Assets/Prefabs/ からの相対名（例: "Weapon/Warhammer"）
        //! @param  typeName [in]     Component名（JSONのルートキー兼ファイル名）
        //! @param  collect  [in]     読んだComponentからパスを取り出す関数
        //! @note   PrefabFactory::Loadはパスを読むだけでアセットは解決しない。
        //!         パス集めはここ（メインスレッド）で済ませ、実際のLoadはロード画面のワーカーに回す
        //-------------------------------------------------------------
        template <class T, class Collect>
        void CollectPrefabAssetPaths(Tsukino::EngineIntegration::EngineContext& context, const std::string& prefab, const char* typeName,
                                     Collect collect) {
            T component{};
            if(context.prefabFactory->Load(GetPrefabDirectory(prefab) + "/" + typeName + ".json", typeName, component))
                collect(component);
        }
    }    // namespace

    //-------------------------------------------------------------
    //! @brief アセットをまとめて事前ロードする
    //-------------------------------------------------------------
    void PreloadAssets(Tsukino::EngineIntegration::EngineContext& context) {
        for(const std::function<void()>& step : BuildPreloadSteps(context))
            step();
    }

    //-------------------------------------------------------------
    //! @brief PreloadAssetsの中身を、1件ずつの読み込みの列として返す
    //-------------------------------------------------------------
    std::vector<std::function<void()>> BuildPreloadSteps(Tsukino::EngineIntegration::EngineContext& context) {
        std::vector<std::function<void()>> steps;
        if(!context.assetManager)
            return steps;

        Tsukino::Asset::AssetManager* assetManager = context.assetManager;
        auto                          loadPath     = [&steps, assetManager](const char* path) {
            if(path)
                steps.push_back([assetManager, path] { (void)assetManager->Load(Tsukino::Core::Path(path)); });
        };

        // Prefab JSONから拾ったパス（空なら「未設定」なので読まない）
        auto loadRef = [&steps, assetManager](const Tsukino::Asset::AssetRef& ref) {
            if(!ref.path.empty())
                steps.push_back([assetManager, path = ref.path] { (void)assetManager->Load(Tsukino::Core::Path(path)); });
        };
        auto loadModel = [&](const std::string& prefab) {
            CollectPrefabAssetPaths<Tsukino::BuiltIn::ECS::ModelComponent>(
                context, prefab, "ModelComponent", [&](const Tsukino::BuiltIn::ECS::ModelComponent& model) { loadRef(model.modelHandle); });
        };

        //-------------------------------------------------------------
        // 武器：全種の見た目・攻撃クリップ・エフェクトを武器Prefab（Assets/Prefabs/Weapon/<名前>/）から読む
        //-------------------------------------------------------------
        for(int i = 0; i < static_cast<int>(WeaponId::Count); ++i) {
            const std::string prefab = GetWeaponPrefabName(static_cast<WeaponId>(i));
            loadModel(prefab);
            CollectPrefabAssetPaths<WeaponComponent>(context, prefab, "WeaponComponent", [&](const WeaponComponent& weapon) {
                loadRef(weapon.attackClip);
                loadRef(weapon.areaAttackEffectAsset);
                loadRef(weapon.projectileEffectAsset);
            });
        }

        //-------------------------------------------------------------
        // スキル：カード背景とHUDアイコン
        //-------------------------------------------------------------
        for(const SkillTableEntry& entry : GetSkillTable()) {
            loadPath(entry.backgroundTexturePath.c_str());
            loadPath(entry.iconTexturePath.c_str());
        }

        //-------------------------------------------------------------
        // 敵：モデル・アニメーションクリップを敵Prefab（Assets/Prefabs/Enemy/<名前>/）から読む。
        // Paladinは持たせる武器で攻撃クリップが変わるため、武器ごとの値（PaladinWeaponAttacks.json）も全WeaponId分読む
        //-------------------------------------------------------------
        for(const EnemySpawnTableEntry& entry : GetEnemySpawnTable()) {
            // 生成パラメータはPrefab名を詰めるだけ（ここではアセットを読まない）なので、Prefab名を引くのに使ってよい
            const std::string prefab = entry.makeConfig(context, hlslpp::float3(0.0f, 0.0f, 0.0f)).prefabName;
            loadModel(prefab);
            CollectPrefabAssetPaths<EnemyAnimationSetComponent>(context, prefab, "EnemyAnimationSetComponent",
                                                                [&](const EnemyAnimationSetComponent& set) {
                                                                    loadRef(set.walkClip);
                                                                    loadRef(set.attackClip);
                                                                    loadRef(set.knockbackClip);
                                                                    loadRef(set.deathClip);
                                                                });
        }

        for(int i = 0; i < static_cast<int>(WeaponId::Count); ++i) {
            Tsukino::Asset::AssetRef clip;
            clip.path = GetPaladinWeaponAttack(static_cast<WeaponId>(i)).attackClipPath;
            loadRef(clip);
        }

        //-------------------------------------------------------------
        // 効果音：表に載っているものを全て読む（初回再生時の変換待ちを無くす）
        //-------------------------------------------------------------
        for(const SoundTableEntry& entry : GetSoundTable())
            loadPath(entry.path.c_str());

        //-------------------------------------------------------------
        // どのテーブルにも属さない単発アセット
        //-------------------------------------------------------------
        for(const char* path : kMiscPreloadPaths)
            loadPath(path);

        //-------------------------------------------------------------
        // タイトル・戦闘中のBGM。利用者が置く素材なので、置かれていなければ読まない
        // （無いファイルのLoadは失敗が覚えておかれず、毎回インポートを試みて遅い）
        //-------------------------------------------------------------
        for(const char* bgm : {kTitleBgmPath, kBattleBgmPath}) {
            steps.push_back([assetManager, bgm] {
                const Tsukino::Core::Path bgmPath(bgm);
                if(Tsukino::IO::FileSystem::Exists(Tsukino::IO::FileSystem::GetAssetRootPath() / bgmPath))
                    (void)assetManager->Load(bgmPath);
            });
        }

        return steps;
    }
}    // namespace CombatAndroid::ECS
