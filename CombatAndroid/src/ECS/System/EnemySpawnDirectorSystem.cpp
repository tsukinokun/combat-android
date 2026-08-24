//-------------------------------------------------------------
//! @file    EnemySpawnDirectorSystem.cpp
//! @brief   サバイバー型の敵湧き潰しシステムの実装
//! @author  山﨑愛
//-------------------------------------------------------------
#include <CombatAndroid/ECS/System/EnemySpawnDirectorSystem.hpp>

#include <CombatAndroid/ECS/Component/EnemyComponent.hpp>
#include <CombatAndroid/ECS/Component/HealthComponent.hpp>
#include <CombatAndroid/ECS/Component/PlayerComponent.hpp>
#include <CombatAndroid/ECS/Component/SpawnedEnemyComponent.hpp>
#include <CombatAndroid/ECS/Utility/EnemySpawnTable.hpp>
#include <CombatAndroid/ECS/Utility/EnemySpawner.hpp>

#include <Tsukino/EngineIntegration/EngineContext.hpp>

#include <Tsukino/BuiltIn/ECS/Component/TransformComponent.hpp>

#include <entt/entt.hpp>

#include <algorithm>
#include <cmath>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    namespace {
        //! 円周率
        constexpr float kPi = 3.14159265f;
    }    // namespace

    //-------------------------------------------------------------
    //! @brief システムの更新
    //-------------------------------------------------------------
    void EnemySpawnDirectorSystem::Update(Tsukino::ECS::Registry& registry, float deltaTime) {
        Tsukino::EngineIntegration::EngineContext* ctx = registry.GetContext<Tsukino::EngineIntegration::EngineContext*>();
        if(!ctx)
            return;

        //---------------------------------------------------------
        // プレイヤーを引く（単一プレイヤー前提。EnemyBehaviorSystemと同じ方針）
        //---------------------------------------------------------
        entt::entity   playerEntity   = entt::null;
        hlslpp::float3 playerPosition = hlslpp::float3(0.0f, 0.0f, 0.0f);

        auto playerView = registry.View<PlayerComponent, Tsukino::BuiltIn::ECS::TransformComponent>();
        for(auto entity : playerView) {
            playerEntity   = entity;
            playerPosition = playerView.get<Tsukino::BuiltIn::ECS::TransformComponent>(entity).position;
            break;
        }

        if(playerEntity == entt::null)
            return;

        // プレイヤーが死んでいる間は湧かせない（間引きは続ける）
        const HealthComponent* playerHealth = registry.try_get<HealthComponent>(playerEntity);
        const bool             playerDead   = playerHealth && playerHealth->isDead;

        m_elapsedSeconds += deltaTime;

        //---------------------------------------------------------
        // (1) 間引き → (2) 数える → (3) 湧かせる の順で行う。
        // 生成・破棄は必ずViewの反復の外側（ここ）でのみ行う。
        // 反復中にエンティティを増減させるとEnTTのイテレータが壊れる
        //---------------------------------------------------------
        CullDistantEnemies(registry, playerPosition);

        if(playerDead || m_elapsedSeconds < kWarmupSeconds)
            return;

        m_spawnTimer -= deltaTime;
        if(m_spawnTimer > 0.0f)
            return;

        // 経過時間で湧き間隔を詰める（線形）
        const float rampT = std::clamp(m_elapsedSeconds / kIntervalRampSeconds, 0.0f, 1.0f);
        m_spawnTimer       = kIntervalStart + (kIntervalEnd - kIntervalStart) * rampT;

        //---------------------------------------------------------
        // 上限判定はシーン手置き・負荷試験が湧かせた分も含めた総数で行う。
        // こうしておくと、負荷試験（F1）で数百体出ている間は本Systemが
        // 自動的に湧きを止め、計測の邪魔をしない
        //---------------------------------------------------------
        int liveCount = CountLiveEnemies(registry);
        for(int i = 0; i < kSpawnBatchSize && liveCount < kMaxLiveEnemies; ++i) {
            SpawnOne(registry, *ctx, playerPosition);
            ++liveCount;
        }
    }

    //-------------------------------------------------------------
    //! @brief 敵を1体、抽選テーブルに従って湧かせる
    //-------------------------------------------------------------
    void EnemySpawnDirectorSystem::SpawnOne(Tsukino::ECS::Registry& registry,
                                            Tsukino::EngineIntegration::EngineContext& context,
                                            const hlslpp::float3& playerPosition) {
        const EnemySpawnTableEntry* entry = PickEnemyType(m_rng, m_elapsedSeconds);
        if(!entry)
            return;

        EnemySpawnConfig config = entry->makeConfig(context, ResolveSpawnPosition(playerPosition));

        // 索敵距離の上書き。種類ごとの既定値（600）のままだとフォグの外から
        // 近づいてこないため、テーブルに何が増えてもここで一律に効かせる
        config.detectRange = kChaseDetectRange;

        // 全個体の再生位置をずらす。揃っていると群れの足の運びが完全に一致し、
        // AnimationSystemの分岐も毎フレーム同じになって不自然に見える
        std::uniform_real_distribution<float> phaseDist(0.0f, 3.0f);
        config.initialAnimationTime = phaseDist(m_rng);

        Tsukino::ECS::Entity enemyEntity = SpawnBehaviorEnemy(registry, context, config);
        registry.AddComponent<SpawnedEnemyComponent>(enemyEntity);
    }

    //-------------------------------------------------------------
    //! @brief プレイヤーを中心に、フォグの外側の湧き位置を1つ決める
    //-------------------------------------------------------------
    hlslpp::float3 EnemySpawnDirectorSystem::ResolveSpawnPosition(const hlslpp::float3& playerPosition) {
        std::uniform_real_distribution<float> angleDist(0.0f, 2.0f * kPi);
        std::uniform_real_distribution<float> radiusDist(kSpawnRadiusMin, kSpawnRadiusMax);

        //-----------------------------------------------------
        // 方角は一様。カメラ正面を避ける／狙う補正はあえて入れていない。
        // TPSカメラは自由に回るため「後ろから湧かせる」設計にしても、
        // プレイヤーが振り向いた瞬間に結局見えることになり意味が薄い。
        // 一様に囲ませるほうがサバイバーゲームの絵として素直
        //-----------------------------------------------------
        hlslpp::float3 candidate = playerPosition;

        for(int attempt = 0; attempt < kSpawnAttemptCount; ++attempt) {
            const float angle  = angleDist(m_rng);
            const float radius = radiusDist(m_rng);

            candidate = hlslpp::float3(playerPosition.x + std::cos(angle) * radius,
                                       kSpawnHeight,
                                       playerPosition.z + std::sin(angle) * radius);

            if(std::abs(candidate.x) <= kGroundLimit && std::abs(candidate.z) <= kGroundLimit)
                return candidate;
        }

        // 全て外れた（プレイヤーが地面の隅にいる）場合のみ、最後に引いた候補を
        // 地面の内側へ押し込んで諦める。稀なうえ、湧かないよりは近くに湧くほうがまだよい。
        // hlslppのfloat3::x/zはswizzleプロキシ型のため、std::clampへ渡す前に
        // 一度plainなfloatへ落としてから代入し直す
        const float clampedX = std::clamp(static_cast<float>(candidate.x), -kGroundLimit, kGroundLimit);
        const float clampedZ = std::clamp(static_cast<float>(candidate.z), -kGroundLimit, kGroundLimit);
        candidate.x = clampedX;
        candidate.z = clampedZ;
        return candidate;
    }

    //-------------------------------------------------------------
    //! @brief 遠くへ離れた、本Systemが湧かせた個体を間引く
    //-------------------------------------------------------------
    void EnemySpawnDirectorSystem::CullDistantEnemies(Tsukino::ECS::Registry& registry, const hlslpp::float3& playerPosition) {
        auto view = registry.View<SpawnedEnemyComponent, Tsukino::BuiltIn::ECS::TransformComponent>();
        view.each([&](entt::entity entity, SpawnedEnemyComponent&, Tsukino::BuiltIn::ECS::TransformComponent& transform) {
            hlslpp::float3 toPlayer = playerPosition - transform.position;
            toPlayer.y              = 0.0f;    // 高さは無視する（BTの追跡判定と揃える）

            // hlslpp::lengthの戻り値をplainなfloatへ受けてから比較する（ZombieBehavior.cppと同じ作法）。
            // float1のまま比較するとビルトイン演算子とのオーバーロード解決があいまいになる
            const float distance = hlslpp::length(toPlayer);
            if(distance <= kDespawnRadius)
                return;

            //-------------------------------------------------
            // レンダラに距離カリング・フラスタムカリングが無いため、置き去りにした敵は
            // 画面外・地面の端でも毎フレームぶんのドローとスキニングを払い続ける。
            // 「見えないものは消す」をここで肩代わりする。
            //
            // 敵1体につきHPバーが2エンティティぶら下がっている。本体だけ消すと
            // HPバーが宙に浮いたまま残る（EnemyStressTestSystem::DespawnAllと同じ作法）。
            // 死亡演出中の個体を二重にQueueDestroyする可能性があるが、
            // FlushDestroyQueueが重複を除去するため問題ない
            //-------------------------------------------------
            if(const HealthComponent* health = registry.try_get<HealthComponent>(entity)) {
                if(health->hpBarBackgroundEntity != entt::null)
                    registry.QueueDestroy(health->hpBarBackgroundEntity);
                if(health->hpBarFillEntity != entt::null)
                    registry.QueueDestroy(health->hpBarFillEntity);
            }
            registry.QueueDestroy(entity);
        });
    }

    //-------------------------------------------------------------
    //! @brief 生存中（死亡演出中を除く）の敵の総数を数える
    //-------------------------------------------------------------
    int EnemySpawnDirectorSystem::CountLiveEnemies(Tsukino::ECS::Registry& registry) {
        //-----------------------------------------------------
        // 死亡した敵はBTのPlayDeathがフェード終了時に自分でQueueDestroyするため、
        // 本Systemがカウンタを持って追いかけることはできない（必ず毎回数える）。
        // 60〜100体規模で毎フレーム1パス舐めるコストは無視できる
        // （負荷試験では2000体でも実用範囲だった）
        //-----------------------------------------------------
        int  count = 0;
        auto view  = registry.View<EnemyComponent, HealthComponent>();
        view.each([&](entt::entity, EnemyComponent&, HealthComponent& health) {
            if(!health.isDead)
                ++count;
        });
        return count;
    }
}    // namespace CombatAndroid::ECS
