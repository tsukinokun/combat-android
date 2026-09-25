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
#include <CombatAndroid/ECS/Component/EnemyHeldWeaponComponent.hpp>
#include <CombatAndroid/ECS/Component/HealthComponent.hpp>
#include <CombatAndroid/ECS/Component/WeaponComponent.hpp>
#include <CombatAndroid/ECS/Serialization/PaladinWeaponAttackSerialization.hpp>
#include <CombatAndroid/ECS/Utility/GamePrefab.hpp>
#include <CombatAndroid/ECS/Utility/WeaponSpawner.hpp>

#include <Tsukino/Core/Log.hpp>
#include <Tsukino/EngineIntegration/EngineContext.hpp>
#include <Tsukino/Engine/Asset/AssetManager.hpp>

#include <Tsukino/BuiltIn/ECS/Component/AnimationPlayerComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/CollisionComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/TransformComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/WorldAnchorComponent.hpp>

#include <entt/entt.hpp>

#include <cereal/archives/json.hpp>

#include <array>
#include <fstream>
#include <iterator>
#include <random>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    namespace {
        //-------------------------------------------------------------
        // Paladinは武器を持って湧く敵で、持っている武器によって攻撃モーション・間合い・威力が変わる。
        // その武器ごとの値は Assets/Prefabs/Enemy/PaladinWeaponAttacks.json が持つ（キーは武器名）。
        // 攻撃クリップは常に1本（EnemyAnimationSetComponent::attackClip）で、エリートが持ち替えるときは
        // その1本と判定の値を差し替える。EnemyAnimState側へ攻撃ステートを増やす必要はない。
        //
        // 手ボーンから武器先端までの距離（hitboxReach）は、プレイヤーが振るときの当たり判定の長さ
        // （WeaponComponent::range＝グリップから刃先までの到達距離）と同じ値にしてある。
        // attackRangeは接触距離（bodyRadius37＋プレイヤー半径35＝72）よりだいぶ外だが、判定が
        // 「手→武器先端のカプセル」で前へ大きく張り出すため届く。リーチの長い武器ほど手前で足を止めさせ、
        // 間合いの違いを見た目に出している
        //-------------------------------------------------------------
        constexpr const char* kPaladinWeaponAttackFile = "CombatAndroid/Assets/Prefabs/Enemy/PaladinWeaponAttacks.json";

        //-------------------------------------------------------------
        //! @brief  Paladinの武器ごとの攻撃パラメータのJSONを読む関数
        //! @return WeaponIdの並び順のパラメータ
        //-------------------------------------------------------------
        std::array<PaladinWeaponAttack, static_cast<size_t>(WeaponId::Count)> LoadPaladinWeaponAttacks() {
            std::array<PaladinWeaponAttack, static_cast<size_t>(WeaponId::Count)> attacks{};

            for(size_t i = 0; i < attacks.size(); ++i)
                attacks[i].weaponId = static_cast<WeaponId>(i);

            std::ifstream is(kPaladinWeaponAttackFile);
            if(!is.is_open()) {
                Tsukino::Core::Log::Error(std::string("Paladin weapon attacks not found: ") + kPaladinWeaponAttackFile);
                return attacks;
            }

            try {
                cereal::JSONInputArchive archive(is);
                for(size_t i = 0; i < attacks.size(); ++i)
                    archive(cereal::make_nvp(GetWeaponKey(static_cast<WeaponId>(i)), attacks[i]));
            } catch(const cereal::Exception& exception) {
                Tsukino::Core::Log::Error(std::string("Paladin weapon attacks are broken: ") + kPaladinWeaponAttackFile + " (" + exception.what() + ")");
            }

            return attacks;
        }

        //-------------------------------------------------------------
        //! @brief  敵の頭上HPバーを1本生成する関数
        //! @param  registry    [in,out] エンティティレジストリ
        //! @param  context     [in]     エンジンコンテキスト
        //! @param  prefabName  [in]     HPバーのPrefab（背景 or 残量）
        //! @param  enemyEntity [in]     貼り付け先の敵
        //! @param  capsuleTop  [in]     敵の足元からカプセル上端までの高さ
        //! @return 生成したエンティティ
        //! @note   Prefabのworldオフセット（カプセル上端からの余白）へ、個体ごとのカプセルの高さを足す
        //-------------------------------------------------------------
        Tsukino::ECS::Entity SpawnHpBar(Tsukino::ECS::Registry& registry, Tsukino::EngineIntegration::EngineContext& context,
                                        const char* prefabName, Tsukino::ECS::Entity enemyEntity, float capsuleTop) {
            Tsukino::ECS::Entity barEntity = InstantiatePrefab(registry, context, prefabName);

            Tsukino::BuiltIn::ECS::WorldAnchorComponent& anchor = registry.GetComponent<Tsukino::BuiltIn::ECS::WorldAnchorComponent>(barEntity);
            anchor.target = enemyEntity;
            anchor.worldOffset.y += capsuleTop;

            return barEntity;
        }

        //-------------------------------------------------------------
        //! @brief  Prefabの値へ危険度・エリートの倍率を掛ける関数
        //! @note   大きさの倍率は、見た目（Transform）・体の当たり（Enemy/Collision）・
        //!         攻撃範囲・判定半径へまとめて掛ける
        //-------------------------------------------------------------
        void ApplyScales(Tsukino::ECS::Registry& registry, Tsukino::ECS::Entity enemyEntity, const EnemySpawnConfig& config) {
            EnemyComponent&             enemy  = registry.GetComponent<EnemyComponent>(enemyEntity);
            HealthComponent&            health = registry.GetComponent<HealthComponent>(enemyEntity);
            EnemyAttackHitboxComponent& hitbox = registry.GetComponent<EnemyAttackHitboxComponent>(enemyEntity);

            if(config.detectRange > 0.0f)
                enemy.detectRange = config.detectRange;

            health.maxHealth *= config.healthScale;
            health.currentHealth = health.maxHealth;

            enemy.expReward *= config.expScale;
            enemy.knockbackDamageThreshold *= config.knockbackThresholdScale;
            enemy.moveSpeed *= config.moveSpeedScale;
            hitbox.damage *= config.attackScale;

            if(config.sizeScale != 1.0f) {
                Tsukino::BuiltIn::ECS::TransformComponent& transform = registry.GetComponent<Tsukino::BuiltIn::ECS::TransformComponent>(enemyEntity);
                Tsukino::BuiltIn::ECS::CollisionComponent& collision = registry.GetComponent<Tsukino::BuiltIn::ECS::CollisionComponent>(enemyEntity);

                transform.scale          = transform.scale * config.sizeScale;
                collision.extent         = collision.extent * config.sizeScale;
                collision.offsetPosition = collision.offsetPosition * config.sizeScale;
                enemy.bodyRadius *= config.sizeScale;
                enemy.attackRange *= config.sizeScale;
                hitbox.radius *= config.sizeScale;
            }
        }
    }    // namespace

    //-------------------------------------------------------------
    //! @brief 敵を1体生成する
    //-------------------------------------------------------------
    Tsukino::ECS::Entity SpawnBehaviorEnemy(Tsukino::ECS::Registry& registry,
                                            Tsukino::EngineIntegration::EngineContext& context,
                                            const EnemySpawnConfig& config) {
        //-------------------------------------------------------------
        // 本体。見た目・当たり（Kinematicのカプセルセンサー）・HP・攻撃判定・アニメーション一式は
        // Prefab（Assets/Prefabs/Enemy/<名前>/）が持つ。
        // センサーはTransform位置＝足元とみなし、カプセル中心をそこから上へオフセットしてある
        //-------------------------------------------------------------
        Tsukino::ECS::Entity enemyEntity = InstantiatePrefab(registry, context, config.prefabName);

        Tsukino::BuiltIn::ECS::TransformComponent& enemyTransform = registry.GetComponent<Tsukino::BuiltIn::ECS::TransformComponent>(enemyEntity);
        enemyTransform.position = config.spawnPosition;
        enemyTransform.dirty    = true;

        // 武器を持つ敵は、攻撃モーション・間合い・判定を武器のものへ差し替えてから倍率を掛ける
        // （危険度・エリートの補正は武器ごとの素の値に対して掛かる）
        if(config.hasHeldWeapon)
            ApplyHeldWeaponAttack(registry, context, enemyEntity, config.heldWeaponId, 1.0f, 1.0f);

        ApplyScales(registry, enemyEntity, config);

        //-------------------------------------------------------------
        // 頭上HPバー（背景＋残量の2エンティティ）。カプセル上端（中心の高さの2倍）より上に浮かせる。
        // WorldAnchorSystemが毎フレームスクリーン座標へ投影し、HealthBarSystemが残量に応じて見た目を更新する
        //-------------------------------------------------------------
        const float capsuleTop =
            static_cast<float>(registry.GetComponent<Tsukino::BuiltIn::ECS::CollisionComponent>(enemyEntity).offsetPosition.y) * 2.0f;

        const Tsukino::ECS::Entity backgroundEntity = SpawnHpBar(registry, context, "Enemy/HpBarBackground", enemyEntity, capsuleTop);
        const Tsukino::ECS::Entity fillEntity       = SpawnHpBar(registry, context, "Enemy/HpBarFill", enemyEntity, capsuleTop);

        // HPバーを作るとComponentの格納先が動き得るので、ここで参照を取り直して結ぶ
        HealthComponent& health      = registry.GetComponent<HealthComponent>(enemyEntity);
        health.hpBarBackgroundEntity = backgroundEntity;
        health.hpBarFillEntity       = fillEntity;

        //-------------------------------------------------------------
        // アニメーション（初期状態は歩き。以後はEnemyAnimationSystemが管理する）と
        // ビヘイビアツリー本体（歩く→射程内で攻撃、被弾でノックバック、死亡でフェードアウト）
        //-------------------------------------------------------------
        Tsukino::BuiltIn::ECS::AnimationPlayerComponent& animPlayer =
            registry.GetComponent<Tsukino::BuiltIn::ECS::AnimationPlayerComponent>(enemyEntity);
        animPlayer.current_clip_id = registry.GetComponent<EnemyAnimationSetComponent>(enemyEntity).walkClip;
        animPlayer.elapsed_time    = config.initialAnimationTime;

        registry.GetComponent<BehaviorTreeComponent>(enemyEntity).root = BuildZombieTree();

        //-------------------------------------------------------------
        // 手に持つ武器（Paladin等）。プレイヤーの武器とまったく同じ経路で作り、
        // WeaponComponent::ownerをこの敵にすることでCombatSystemが右手ボーンへ追従させる。
        //
        // この武器がプレイヤーを殴ることは無い：武器のダメージ判定はWeaponComponent::isActive
        // でしか開くことができず、それを開けるのはattackRequested経路（PlayerAnimationSystem）
        // だけだからである。敵からプレイヤーへのダメージは他の敵と同じく
        // EnemyAttackHitboxComponent（手→武器先端のカプセル）が担当する
        //-------------------------------------------------------------
        if(config.hasHeldWeapon) {
            Tsukino::ECS::Entity heldWeaponEntity = SpawnWeapon(registry, context, config.heldWeaponId, config.spawnPosition, enemyEntity);

            // 持ち方はプレイヤーとまったく同じにする（非攻撃時は肩の斜め上で浮遊、攻撃中は手ボーンへ追従）。
            // 常時手ボーン追従にすると、待機・歩行クリップの腕の振りに合わせて武器が暴れる
            registry.GetComponent<WeaponComponent>(heldWeaponEntity).floatEnabled = true;

            EnemyHeldWeaponComponent& heldWeaponRef = registry.AddComponent<EnemyHeldWeaponComponent>(enemyEntity);
            heldWeaponRef.weaponEntity              = heldWeaponEntity;
            heldWeaponRef.weaponId                  = config.heldWeaponId;
        }

        return enemyEntity;
    }

    //-------------------------------------------------------------
    //! @brief SmallZombie 1体分の生成パラメータを作る
    //-------------------------------------------------------------
    EnemySpawnConfig MakeSmallZombieConfig(Tsukino::EngineIntegration::EngineContext& /*context*/, const hlslpp::float3& spawnPosition) {
        EnemySpawnConfig config{};
        config.prefabName    = "Enemy/SmallZombie";
        config.spawnPosition = spawnPosition;
        return config;
    }

    //-------------------------------------------------------------
    //! @brief BigZombie 1体分の生成パラメータを作る
    //-------------------------------------------------------------
    EnemySpawnConfig MakeBigZombieConfig(Tsukino::EngineIntegration::EngineContext& /*context*/, const hlslpp::float3& spawnPosition) {
        EnemySpawnConfig config{};
        config.prefabName    = "Enemy/BigZombie";
        config.spawnPosition = spawnPosition;
        return config;
    }

    //-------------------------------------------------------------
    //! @brief 武器の種類からPaladinの攻撃パラメータを引く
    //-------------------------------------------------------------
    const PaladinWeaponAttack& GetPaladinWeaponAttack(WeaponId weaponId) {
        // 初回の呼び出しで1度だけ読む。関数内staticの初期化はスレッド安全
        static const auto attacks = LoadPaladinWeaponAttacks();

        int index = static_cast<int>(weaponId);
        if(index < 0 || index >= static_cast<int>(WeaponId::Count))
            index = 0;

        return attacks[static_cast<size_t>(index)];
    }

    //-------------------------------------------------------------
    //! @brief 敵の攻撃モーション・間合い・判定を、持っている武器のものへ書き換える
    //-------------------------------------------------------------
    void ApplyHeldWeaponAttack(Tsukino::ECS::Registry& registry, Tsukino::EngineIntegration::EngineContext& context,
                               Tsukino::ECS::Entity enemyEntity, WeaponId weaponId, float sizeScale, float damageScale) {
        const PaladinWeaponAttack& attack = GetPaladinWeaponAttack(weaponId);

        // クリップは先読み済み（AssetPreloader）なので、ここでのLoadはキャッシュから引くだけ
        registry.GetComponent<EnemyAnimationSetComponent>(enemyEntity).attackClip =
            context.assetManager->Load(Tsukino::Core::Path(attack.attackClipPath));
        registry.GetComponent<EnemyComponent>(enemyEntity).attackRange = attack.attackRange * sizeScale;

        //-------------------------------------------------------------
        // 判定は「手→武器先端」のカプセル。始点と終点に同じボーン（Prefab）を指定し、
        // endBoneLocalOffsetで武器の長さぶんだけ伸ばすことで、専用の先端ボーンが無くても
        // 武器の形を判定に反映できる。
        // オフセットが手ボーンのローカル+X方向なのは、武器の姿勢が
        // 「手ボーンの姿勢 × 握り補正（WeaponComponent::attackGripRotationOffset）」で決まり、
        // 武器メッシュの刃方向であるローカル+Yを握り補正で回すと+Xを向くため。
        // なおboneLocalOffset/endBoneLocalOffsetには敵のscaleが掛からない（CombatSystem参照）ので、
        // ここの値はそのままワールド単位（1ユニット≒1cm）になる
        //-------------------------------------------------------------
        EnemyAttackHitboxComponent& hitbox = registry.GetComponent<EnemyAttackHitboxComponent>(enemyEntity);
        hitbox.endBoneLocalOffset          = hlslpp::float3(attack.hitboxReach, 0.0f, 0.0f);
        hitbox.radius                      = attack.hitboxRadius * sizeScale;
        hitbox.damage                      = attack.hitboxDamage * damageScale;
        hitbox.hitStartTime                = attack.hitStartTime;
        hitbox.hitDuration                 = attack.hitDuration;
    }

    //-------------------------------------------------------------
    //! @brief Paladin 1体分の生成パラメータを作る（武器を明示指定する版）
    //-------------------------------------------------------------
    EnemySpawnConfig MakePaladinConfig(Tsukino::EngineIntegration::EngineContext& /*context*/,
                                       const hlslpp::float3& spawnPosition,
                                       WeaponId weaponId) {
        EnemySpawnConfig config{};
        config.prefabName    = "Enemy/Paladin";
        config.spawnPosition = spawnPosition;

        // 抽選された武器を実際に手へ持たせる（SpawnBehaviorEnemyが武器エンティティを作り、攻撃を武器のものにする）
        config.hasHeldWeapon = true;
        config.heldWeaponId  = weaponId;
        return config;
    }

    //-------------------------------------------------------------
    //! @brief Paladin 1体分の生成パラメータを作る（武器をランダムに選ぶ版）
    //-------------------------------------------------------------
    EnemySpawnConfig MakePaladinConfig(Tsukino::EngineIntegration::EngineContext& context, const hlslpp::float3& spawnPosition) {
        // EnemyConfigFactoryは乱数生成器を引数に取らないため、ここだけは
        // ファイルローカルな生成器を持つ（各Systemが自前のmt19937を持っているのと同じ流儀）
        static std::mt19937 s_weaponRng{std::random_device{}()};

        std::uniform_int_distribution<int> distribution(0, static_cast<int>(WeaponId::Count) - 1);

        return MakePaladinConfig(context, spawnPosition, static_cast<WeaponId>(distribution(s_weaponRng)));
    }
}    // namespace CombatAndroid::ECS
