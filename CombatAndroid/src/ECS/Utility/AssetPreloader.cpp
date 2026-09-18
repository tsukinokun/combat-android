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
#include <CombatAndroid/ECS/Utility/SoundTable.hpp>
#include <CombatAndroid/ECS/Utility/Bgm.hpp>

#include <Tsukino/EngineIntegration/EngineContext.hpp>
#include <Tsukino/Engine/Asset/AssetManager.hpp>
#include <Tsukino/Core/IO/FileSystem.hpp>
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
            "CombatAndroid/Assets/Shaders/Ground.vs.hlsl",
            "CombatAndroid/Assets/Audio/HitImpactBlunt.wav",
            "CombatAndroid/Assets/Audio/HitImpactSharp.wav",
            "CombatAndroid/Assets/Audio/ChargeFire.wav",
            "CombatAndroid/Assets/Textures/Ground/DirtGround.bmp",
            "CombatAndroid/Assets/Textures/UI/ExpOrb.png",
            "CombatAndroid/Assets/Textures/UI/WhitePixel.png",

            // プレイヤー（CombatAndroidScene::OnInitializeが直接読む）。ロード画面で先に読んでおけば、
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

        //-------------------------------------------------------------
        // 武器：全種の見た目・攻撃クリップ・エフェクトをWeaponSpawnDefinitionから読む
        //-------------------------------------------------------------
        for(int i = 0; i < static_cast<int>(WeaponId::Count); ++i) {
            const WeaponSpawnDefinition* definition = &GetWeaponSpawnDefinition(static_cast<WeaponId>(i));
            steps.push_back([assetManager, definition] { PreloadWeapon(*assetManager, *definition); });
        }

        //-------------------------------------------------------------
        // スキル：カード背景とHUDアイコン
        //-------------------------------------------------------------
        for(const SkillTableEntry& entry : GetSkillTable()) {
            loadPath(entry.backgroundTexturePath);
            loadPath(entry.iconTexturePath);
        }

        //-------------------------------------------------------------
        // 敵：モデル・アニメーションクリップ。生成パラメータを作るだけで
        // 中のLoad呼び出しが走るので、戻り値は使わず捨てる。
        // Paladinは持たせる武器で攻撃クリップが変わるため、全WeaponId分呼ぶ
        //-------------------------------------------------------------
        Tsukino::EngineIntegration::EngineContext* contextPtr = &context;
        const hlslpp::float3                       dummyPosition(0.0f, 0.0f, 0.0f);

        steps.push_back([contextPtr, dummyPosition] { (void)MakeSmallZombieConfig(*contextPtr, dummyPosition); });
        steps.push_back([contextPtr, dummyPosition] { (void)MakeBigZombieConfig(*contextPtr, dummyPosition); });

        for(int i = 0; i < static_cast<int>(WeaponId::Count); ++i) {
            const WeaponId weaponId = static_cast<WeaponId>(i);
            steps.push_back([contextPtr, dummyPosition, weaponId] { (void)MakePaladinConfig(*contextPtr, dummyPosition, weaponId); });
        }

        //-------------------------------------------------------------
        // 効果音：表に載っているものを全て読む（初回再生時の変換待ちを無くす）
        //-------------------------------------------------------------
        for(const SoundTableEntry& entry : GetSoundTable())
            loadPath(entry.path);

        //-------------------------------------------------------------
        // どのテーブルにも属さない単発アセット
        //-------------------------------------------------------------
        for(const char* path : kMiscPreloadPaths)
            loadPath(path);

        //-------------------------------------------------------------
        // 戦闘中のBGM。利用者が置く素材なので、置かれていなければ読まない
        // （無いファイルのLoadは失敗が覚えておかれず、毎回インポートを試みて遅い）
        //-------------------------------------------------------------
        steps.push_back([assetManager] {
            const Tsukino::Core::Path bgmPath(kBattleBgmPath);
            if(Tsukino::IO::FileSystem::Exists(Tsukino::IO::FileSystem::GetAssetRootPath() / bgmPath))
                (void)assetManager->Load(bgmPath);
        });

        return steps;
    }
}    // namespace CombatAndroid::ECS
