//-------------------------------------------------------------
//! @file    EnemySpawner.cpp
//! @brief   敵エンティティの生成処理の実装
//! @author  山﨑愛
//-------------------------------------------------------------
#include <CombatAndroid/ECS/Utility/EnemySpawner.hpp>

#include <CombatAndroid/ECS/AI/ZombieBehavior.hpp>
#include <CombatAndroid/ECS/Component/BehaviorTreeComponent.hpp>
#include <CombatAndroid/ECS/Component/EnemyAnimationSetComponent.hpp>
#include <CombatAndroid/ECS/Component/EnemyAttackHitboxComponent.hpp>
#include <CombatAndroid/ECS/Component/EnemyComponent.hpp>
#include <CombatAndroid/ECS/Component/HealthComponent.hpp>
#include <CombatAndroid/UI/UiSortOrder.hpp>

#include <Tsukino/EngineIntegration/EngineContext.hpp>
#include <Tsukino/Engine/Asset/AssetManager.hpp>

#include <Tsukino/BuiltIn/ECS/Component/AnimationControllerComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/AnimationPlayerComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/CollisionComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/ModelComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/RigidBodyComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/SkeletonOutputComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/SpriteComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/TransformComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/WorldAnchorComponent.hpp>

#include <entt/entt.hpp>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @brief 敵を1体生成する
    //-------------------------------------------------------------
    Tsukino::ECS::Entity SpawnBehaviorEnemy(Tsukino::ECS::Registry& registry,
                                            Tsukino::EngineIntegration::EngineContext& context,
                                            const EnemySpawnConfig& config) {
        Tsukino::ECS::Entity enemyEntity = registry.CreateEntity();

        Tsukino::BuiltIn::ECS::TransformComponent& enemyTransform = registry.AddComponent<Tsukino::BuiltIn::ECS::TransformComponent>(enemyEntity);
        enemyTransform.position                                   = config.spawnPosition;
        enemyTransform.rotation                                   = hlslpp::quaternion(0.0f, 0.0f, 0.0f, 1.0f);
        enemyTransform.scale                                      = config.scale;
        enemyTransform.dirty                                      = true;
        enemyTransform.parent                                     = entt::null;

        Tsukino::Asset::AssetHandle            enemyModelHandle = context.assetManager->Load(config.modelPath);
        Tsukino::BuiltIn::ECS::ModelComponent& enemyModel       = registry.AddComponent<Tsukino::BuiltIn::ECS::ModelComponent>(enemyEntity);
        enemyModel.modelHandle                                  = enemyModelHandle;
        enemyModel.visible                                      = true;

        CombatAndroid::ECS::EnemyComponent& enemy = registry.AddComponent<CombatAndroid::ECS::EnemyComponent>(enemyEntity);
        enemy.moveSpeed                           = config.moveSpeed;
        enemy.bodyRadius                          = config.bodyRadius;
        enemy.attackRange                         = config.attackRange;
        enemy.knockbackDamageThreshold            = config.knockbackDamageThreshold;
        enemy.detectRange                         = config.detectRange;
        enemy.expReward                           = config.expReward;

        CombatAndroid::ECS::HealthComponent& enemyHealth = registry.AddComponent<CombatAndroid::ECS::HealthComponent>(enemyEntity);
        enemyHealth.maxHealth                            = config.maxHealth;
        enemyHealth.currentHealth                        = config.maxHealth;

        // 武器のヒット判定（CombatSystemのOverlapCapsule）に拾わせるためのカプセルセンサー。
        // Kinematicにすることで、EnemyBehaviorSystemが毎フレーム書き換えるTransformへPhysicsSystemが
        // 追従してくれる（Static/RigidbodyComponent無しだと初期位置に固定されたままになる）。
        // isSensor=trueなので物理的な押し出し（ブロッキング）は発生しない
        Tsukino::BuiltIn::ECS::RigidbodyComponent& enemyRigidbody = registry.AddComponent<Tsukino::BuiltIn::ECS::RigidbodyComponent>(enemyEntity);
        enemyRigidbody.type                                       = Tsukino::BuiltIn::ECS::RigidbodyType::Kinematic;

        Tsukino::BuiltIn::ECS::CollisionComponent& enemyCollision = registry.AddComponent<Tsukino::BuiltIn::ECS::CollisionComponent>(enemyEntity);
        enemyCollision.type                                       = Tsukino::BuiltIn::ECS::ColliderType::Capsule;
        enemyCollision.extent                                     = hlslpp::float3(config.bodyRadius, config.bodyHalfHeight, 0.0f);
        enemyCollision.isSensor                                   = true;
        // Transform位置＝足元とみなし、カプセル中心をそこから上へオフセットする
        // （CharacterControllerComponent::centerOffsetと同じ考え方）
        enemyCollision.offsetPosition = hlslpp::float3(0.0f, config.bodyHalfHeight + config.bodyRadius, 0.0f);

        //-------------------------------------------------------------
        // 頭上HPバー（背景＋残量の2エンティティ）。カプセル上端（2*(bodyHalfHeight+bodyRadius)）より
        // 少し上に浮かせる。WorldAnchorSystemが毎フレームスクリーン座標へ投影し、
        // HealthBarSystemが残量に応じて見た目を更新する（被弾時のみ表示）
        //
        // 単色テクスチャは全ての敵で使い回す。AssetManagerがパスでキャッシュするため、
        // 2体目以降のLoadはハンドルを引くだけで再ロードは発生しない
        //-------------------------------------------------------------
        Tsukino::Asset::AssetHandle hpBarTextureHandle =
            context.assetManager->Load(Tsukino::Core::Path("CombatAndroid/Assets/Textures/UI/WhitePixel.png"));

        hlslpp::float3 hpBarWorldOffset = hlslpp::float3(0.0f, (config.bodyHalfHeight + config.bodyRadius) * 2.0f + 20.0f, 0.0f);

        Tsukino::ECS::Entity hpBarBackgroundEntity = registry.CreateEntity();
        {
            Tsukino::BuiltIn::ECS::TransformComponent& t = registry.AddComponent<Tsukino::BuiltIn::ECS::TransformComponent>(hpBarBackgroundEntity);
            t.scale                                      = hlslpp::float3(0.0f, 0.0f, 0.0f);    // 非表示状態で開始（被弾時にHealthBarSystemが表示する）

            Tsukino::BuiltIn::ECS::WorldAnchorComponent& anchor =
                registry.AddComponent<Tsukino::BuiltIn::ECS::WorldAnchorComponent>(hpBarBackgroundEntity);
            anchor.target      = enemyEntity;
            anchor.worldOffset = hpBarWorldOffset;

            Tsukino::BuiltIn::ECS::SpriteComponent& sprite = registry.AddComponent<Tsukino::BuiltIn::ECS::SpriteComponent>(hpBarBackgroundEntity);
            sprite.textureHandle                           = hpBarTextureHandle;
            sprite.tintColor                               = hlslpp::float4(0.15f, 0.15f, 0.15f, 0.9f);    // 暗いグレー
            sprite.sortOrder                               = CombatAndroid::UI::kEnemyHpBarBackground;
        }

        Tsukino::ECS::Entity hpBarFillEntity = registry.CreateEntity();
        {
            Tsukino::BuiltIn::ECS::TransformComponent& t = registry.AddComponent<Tsukino::BuiltIn::ECS::TransformComponent>(hpBarFillEntity);
            t.scale                                      = hlslpp::float3(0.0f, 0.0f, 0.0f);

            Tsukino::BuiltIn::ECS::WorldAnchorComponent& anchor = registry.AddComponent<Tsukino::BuiltIn::ECS::WorldAnchorComponent>(hpBarFillEntity);
            anchor.target                                       = enemyEntity;
            anchor.worldOffset                                  = hpBarWorldOffset;

            Tsukino::BuiltIn::ECS::SpriteComponent& sprite = registry.AddComponent<Tsukino::BuiltIn::ECS::SpriteComponent>(hpBarFillEntity);
            sprite.textureHandle                           = hpBarTextureHandle;
            sprite.tintColor                               = hlslpp::float4(0.0f, 1.0f, 0.0f, 1.0f);    // 満タン時は緑
            sprite.sortOrder                               = CombatAndroid::UI::kEnemyHpBarFill;
        }

        enemyHealth.hpBarBackgroundEntity = hpBarBackgroundEntity;
        enemyHealth.hpBarFillEntity       = hpBarFillEntity;

        //-------------------------------------------------------------
        // アニメーション再生・制御用コンポーネント（初期状態はIdle。以後はEnemyAnimationSystemが管理する）
        //-------------------------------------------------------------
        Tsukino::BuiltIn::ECS::AnimationPlayerComponent& animPlayer =
            registry.AddComponent<Tsukino::BuiltIn::ECS::AnimationPlayerComponent>(enemyEntity);
        animPlayer.current_clip_id       = config.walkClip;
        animPlayer.animation_index       = 1;    // Mixamo製FBXはindex 0が1tickのスタブ、index 1が実モーション
        animPlayer.elapsed_time          = config.initialAnimationTime;
        animPlayer.playback_speed        = 1.0f;
        animPlayer.is_looping            = true;
        animPlayer.is_playing            = true;
        animPlayer.in_place              = true;    // その場足踏み（移動はEnemyBehaviorSystemがTransformを直接書く）
        animPlayer.root_motion_node_name = "mixamorig:Hips";

        // クリップの切り替え（AnimationSystemが読む「次に再生するクリップ」の受け皿）
        registry.AddComponent<Tsukino::BuiltIn::ECS::AnimationControllerComponent>(enemyEntity);

        // 計算されたボーン行列の出力先（スキニング用）コンポーネント。
        // これが無いとAnimationSystemのView<AnimationPlayerComponent, SkeletonOutputComponent>に
        // 乗らずアニメーションが再生されない
        registry.AddComponent<Tsukino::BuiltIn::ECS::SkeletonOutputComponent>(enemyEntity);

        // EnemyAnimationSystemが参照する、ステートごとのアニメーションクリップ一式
        CombatAndroid::ECS::EnemyAnimationSetComponent& animSet =
            registry.AddComponent<CombatAndroid::ECS::EnemyAnimationSetComponent>(enemyEntity);
        animSet.walkClip      = config.walkClip;
        animSet.attackClip    = config.attackClip;
        animSet.knockbackClip = config.knockbackClip;
        animSet.deathClip     = config.deathClip;

        // 敵の攻撃当たり判定。指定ボーン（頭部・腕など、敵ごとに異なる攻撃部位）に
        // EnemyAttackHitboxComponent::radiusの判定球（またはendBoneName設定時はカプセル）を出し、
        // Attackステートのhit窓（hitStartTime〜+hitDuration）の間だけプレイヤーへダメージを与える
        CombatAndroid::ECS::EnemyAttackHitboxComponent& hitbox =
            registry.AddComponent<CombatAndroid::ECS::EnemyAttackHitboxComponent>(enemyEntity);
        hitbox.boneName           = config.boneName;
        hitbox.boneLocalOffset    = config.hitboxLocalOffset;
        hitbox.endBoneName        = config.endBoneName;
        hitbox.endBoneLocalOffset = config.endBoneLocalOffset;
        hitbox.radius             = config.hitboxRadius;
        hitbox.damage             = config.hitboxDamage;
        hitbox.hitStartTime       = config.hitStartTime;
        hitbox.hitDuration        = config.hitDuration;

        // ビヘイビアツリー本体（歩く→射程内で攻撃、被弾でノックバック、死亡でフェードアウト）
        CombatAndroid::ECS::BehaviorTreeComponent& behaviorTree =
            registry.AddComponent<CombatAndroid::ECS::BehaviorTreeComponent>(enemyEntity);
        behaviorTree.root = CombatAndroid::ECS::BuildZombieTree();

        return enemyEntity;
    }

    //-------------------------------------------------------------
    //! @brief SmallZombie 1体分の生成パラメータを作る
    //-------------------------------------------------------------
    EnemySpawnConfig MakeSmallZombieConfig(Tsukino::EngineIntegration::EngineContext& context, const hlslpp::float3& spawnPosition) {
        Tsukino::Asset::AssetManager& assetManager = *context.assetManager;

        // SmallZombie.fbxの実寸を計測したところ身長はY=約-1.7〜201（約203ユニット）で、
        // プレイヤー（Y=0〜100の100ユニット）のほぼ2倍のスケールでモデリングされていた。
        // 旧値（scale=2.0）はこれを踏まえずプレイヤーと同じ感覚でスケールを置いていたため、
        // 実際の身長がプレイヤー（scale 2.1×100=210）の約2倍（406）になっていた。
        // プレイヤーと同じ「身長210」を狙ってscale=210/203≒1.04に補正し、カプセルも
        // プレイヤーと同じradius=35, halfHeight=70（合計210）に合わせる。
        // ノックバック・死亡クリップは両方ともMixamoの標準ヒューマノイドリグ（mixamorig:）で
        // 作られているため、専用クリップの無いBigZombie側にもそのまま流用している
        EnemySpawnConfig config{};
        config.spawnPosition            = spawnPosition;
        config.moveSpeed                = 100.0f;
        config.maxHealth                = 40.0f;
        config.modelPath                = Tsukino::Core::Path("CombatAndroid/Assets/Models/SmallZombie.fbx");
        config.scale                    = hlslpp::float3(1.04f, 1.04f, 1.04f);
        config.bodyRadius               = 35.0f;
        config.bodyHalfHeight           = 70.0f;
        // 攻撃射程＝MoveToPlayerが足を止める距離でもある（ZombieBehavior参照）。頭部は体の中心軸上にあり
        // 手ボーンより判定中心が後退するため、接触距離（bodyRadius35+プレイヤー半径35＝70）のすぐ外まで
        // 詰めさせて、振りかぶりの間にプレイヤーが多少下がっても当たる余裕を持たせる
        config.attackRange              = 78.0f;
        config.knockbackDamageThreshold = 40.0f;
        config.expReward                = 10.0f;
        config.walkClip                 = assetManager.Load(Tsukino::Core::Path("CombatAndroid/Assets/Anims/SmallZombie/Unarmed Walk Forward.fbx"));
        config.attackClip               = assetManager.Load(Tsukino::Core::Path("CombatAndroid/Assets/Anims/SmallZombie/Zombie Attack.fbx"));
        config.knockbackClip            = assetManager.Load(Tsukino::Core::Path("CombatAndroid/Assets/Anims/SmallZombie/Zombie Reaction Hit.fbx"));
        config.deathClip                = assetManager.Load(Tsukino::Core::Path("CombatAndroid/Assets/Anims/SmallZombie/Stunned.fbx"));
        // SmallZombieの攻撃（Zombie Attack＝噛みつき）は頭から突っ込むモーションのため、
        // 判定は手ではなく頭部ボーンへ球1つで出す（endBoneNameは空のまま＝球モード）
        config.boneName                 = "mixamorig:Head";
        config.hitboxRadius             = 30.0f;
        // 判定窓は既定値（0.40〜0.60秒）のままだと振り下ろしの実タイミングとずれて当たらないことがあったため、
        // 広めに取って実機で見ながら詰める（EnemyAttackHitboxComponentのコメント参照）
        config.hitStartTime             = 0.25f;
        config.hitDuration              = 0.60f;

        return config;
    }

    //-------------------------------------------------------------
    //! @brief BigZombie 1体分の生成パラメータを作る
    //-------------------------------------------------------------
    EnemySpawnConfig MakeBigZombieConfig(Tsukino::EngineIntegration::EngineContext& context, const hlslpp::float3& spawnPosition) {
        Tsukino::Asset::AssetManager& assetManager = *context.assetManager;

        // BigZombie.fbxの実寸を計測したところ身長はY=約0〜204（約204ユニット）で、
        // SmallZombie同様プレイヤーの約2倍のスケールでモデリングされていた。
        // 旧値（scale=2.2）はこれを踏まえておらず、実際の身長がプレイヤー（210）の
        // 約2倍（449）になっていた。「Bigゾンビ＝プレイヤーよりひとまわり大きい」という
        // 意図（旧scale比 2.2/2.1）を保ったまま、scale=220/204≒1.08に補正して
        // 身長220（プレイヤー比+約5%）に合わせる。カプセルもradius=37, halfHeight=73（合計220）
        // に縮小し、見た目とコリジョンの整合を取る
        // Idle用クリップが無いため、待機はMutant Walkingをin_place再生（その場足踏み）にして流用する
        EnemySpawnConfig config{};
        config.spawnPosition  = spawnPosition;
        config.moveSpeed      = 70.0f;
        config.maxHealth      = 150.0f;
        config.modelPath      = Tsukino::Core::Path("CombatAndroid/Assets/Models/BigZombie.fbx");
        config.scale          = hlslpp::float3(1.08f, 1.08f, 1.08f);
        config.bodyRadius     = 37.0f;
        config.bodyHalfHeight = 73.0f;
        // SmallZombieと同じ理由で接触距離（bodyRadius37+プレイヤー半径35＝72）のすぐ外まで詰めさせる。
        // BigZombieの攻撃（Mutant Swiping）は右腕を振り抜くモーションのため、手が体軸から
        // 大きく前へ出る。距離100でもカプセル芯線（肩〜手）の最近点はプレイヤーへ十分届く
        config.attackRange              = 100.0f;
        config.knockbackDamageThreshold = 60.0f;
        config.expReward                = 30.0f;
        config.walkClip                 = assetManager.Load(Tsukino::Core::Path("CombatAndroid/Assets/Anims/BigZombie/Mutant Walking.fbx"));
        config.attackClip               = assetManager.Load(Tsukino::Core::Path("CombatAndroid/Assets/Anims/BigZombie/Mutant Swiping.fbx"));
        config.knockbackClip            = assetManager.Load(Tsukino::Core::Path("CombatAndroid/Assets/Anims/SmallZombie/Zombie Reaction Hit.fbx"));
        config.deathClip                = assetManager.Load(Tsukino::Core::Path("CombatAndroid/Assets/Anims/SmallZombie/Stunned.fbx"));
        // 判定は右腕（肩〜手）へ出す。RightArm（肩）1点の球だと判定中心が体側へ寄りすぎて
        // 振り抜きを表現できないため、RightArm→RightHandを芯線とするカプセルにする
        config.boneName                 = "mixamorig:RightArm";
        config.endBoneName              = "mixamorig:RightHand";
        config.hitboxRadius             = 25.0f;
        config.hitboxDamage             = 15.0f;
        // SmallZombieと同じ理由で判定窓を広めに取る（EnemyAttackHitboxComponentのコメント参照）
        config.hitStartTime             = 0.30f;
        config.hitDuration              = 0.60f;

        return config;
    }
}    // namespace CombatAndroid::ECS
