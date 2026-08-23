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
            sprite.sortOrder                               = 0;                                           // 残量バーより先に描く
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
            sprite.sortOrder                               = 1;                                        // 背景の上に描く
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

        // 敵の攻撃当たり判定。手ボーンにEnemyAttackHitboxComponent::radiusの判定球を出し、
        // Attackステートのhit窓（hitStartTime〜+hitDuration）の間だけプレイヤーへダメージを与える
        CombatAndroid::ECS::EnemyAttackHitboxComponent& hitbox =
            registry.AddComponent<CombatAndroid::ECS::EnemyAttackHitboxComponent>(enemyEntity);
        hitbox.handBoneName    = config.handBoneName;
        hitbox.boneLocalOffset = config.hitboxLocalOffset;
        hitbox.radius          = config.hitboxRadius;
        hitbox.damage          = config.hitboxDamage;
        hitbox.hitStartTime    = config.hitStartTime;
        hitbox.hitDuration     = config.hitDuration;

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

        // 実寸未計測のため暫定スケール・カプセル寸法で置き、実機のコリジョンワイヤーフレーム
        // （_DEBUG常時ON）を見ながら詰める前提の初期値。
        // ノックバック・死亡クリップは両方ともMixamoの標準ヒューマノイドリグ（mixamorig:）で
        // 作られているため、専用クリップの無いBigZombie側にもそのまま流用している
        EnemySpawnConfig config{};
        config.spawnPosition            = spawnPosition;
        config.moveSpeed                = 100.0f;
        config.maxHealth                = 40.0f;
        config.modelPath                = Tsukino::Core::Path("CombatAndroid/Assets/Models/SmallZombie.fbx");
        config.scale                    = hlslpp::float3(2.0f, 2.0f, 2.0f);
        config.bodyRadius               = 40.0f;
        config.bodyHalfHeight           = 70.0f;
        config.attackRange              = 120.0f;
        config.knockbackDamageThreshold = 40.0f;
        config.walkClip                 = assetManager.Load(Tsukino::Core::Path("CombatAndroid/Assets/Anims/SmallZombie/Unarmed Walk Forward.fbx"));
        config.attackClip               = assetManager.Load(Tsukino::Core::Path("CombatAndroid/Assets/Anims/SmallZombie/Zombie Attack.fbx"));
        config.knockbackClip            = assetManager.Load(Tsukino::Core::Path("CombatAndroid/Assets/Anims/SmallZombie/Zombie Reaction Hit.fbx"));
        config.deathClip                = assetManager.Load(Tsukino::Core::Path("CombatAndroid/Assets/Anims/SmallZombie/Stunned.fbx"));

        return config;
    }

    //-------------------------------------------------------------
    //! @brief BigZombie 1体分の生成パラメータを作る
    //-------------------------------------------------------------
    EnemySpawnConfig MakeBigZombieConfig(Tsukino::EngineIntegration::EngineContext& context, const hlslpp::float3& spawnPosition) {
        Tsukino::Asset::AssetManager& assetManager = *context.assetManager;

        // カプセルサイズは見た目のスケール(2.2倍)に合わせて拡大している。
        // Idle用クリップが無いため、待機はMutant Walkingをin_place再生（その場足踏み）にして流用する
        EnemySpawnConfig config{};
        config.spawnPosition  = spawnPosition;
        config.moveSpeed      = 70.0f;
        config.maxHealth      = 150.0f;
        config.modelPath      = Tsukino::Core::Path("CombatAndroid/Assets/Models/BigZombie.fbx");
        config.scale          = hlslpp::float3(2.2f, 2.2f, 2.2f);
        config.bodyRadius     = 70.0f;
        config.bodyHalfHeight = 110.0f;
        // 攻撃射程はbodyRadius(70)+playerRadiusより広く取り、振りかぶる前に手判定より先に
        // 別の判定が成立することのないようにする
        config.attackRange              = 150.0f;
        config.knockbackDamageThreshold = 60.0f;
        config.walkClip                 = assetManager.Load(Tsukino::Core::Path("CombatAndroid/Assets/Anims/BigZombie/Mutant Walking.fbx"));
        config.attackClip               = assetManager.Load(Tsukino::Core::Path("CombatAndroid/Assets/Anims/BigZombie/Mutant Swiping.fbx"));
        config.knockbackClip            = assetManager.Load(Tsukino::Core::Path("CombatAndroid/Assets/Anims/SmallZombie/Zombie Reaction Hit.fbx"));
        config.deathClip                = assetManager.Load(Tsukino::Core::Path("CombatAndroid/Assets/Anims/SmallZombie/Stunned.fbx"));
        config.hitboxRadius             = 60.0f;
        config.hitboxDamage             = 15.0f;

        return config;
    }
}    // namespace CombatAndroid::ECS
