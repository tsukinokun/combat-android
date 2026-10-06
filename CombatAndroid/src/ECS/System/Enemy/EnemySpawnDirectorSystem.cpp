//-------------------------------------------------------------
//! @file    EnemySpawnDirectorSystem.cpp
//! @brief   サバイバー型の敵湧き潰しシステムの実装
//! @author  山﨑愛
//-------------------------------------------------------------
#include <CombatAndroid/ECS/System/Enemy/EnemySpawnDirectorSystem.hpp>

#include <CombatAndroid/ECS/Component/Enemy/EliteEnemyComponent.hpp>
#include <CombatAndroid/ECS/Component/Enemy/EnemyComponent.hpp>
#include <CombatAndroid/ECS/Component/Enemy/EnemyHeldWeaponComponent.hpp>
#include <CombatAndroid/ECS/Component/Combat/HealthComponent.hpp>
#include <CombatAndroid/ECS/Component/Enemy/PaladinArsenalComponent.hpp>
#include <CombatAndroid/ECS/Component/Player/PlayerComponent.hpp>
#include <CombatAndroid/ECS/Component/Progression/RunClockComponent.hpp>
#include <CombatAndroid/ECS/Component/Enemy/SpawnedEnemyComponent.hpp>
#include <CombatAndroid/ECS/Event/UI/GameLogEvent.hpp>
#include <CombatAndroid/ECS/Utility/Spawn/EliteEnemy.hpp>
#include <CombatAndroid/ECS/Utility/Table/EnemyDifficultyTable.hpp>
#include <CombatAndroid/ECS/Utility/Table/EnemySpawnTable.hpp>
#include <CombatAndroid/ECS/Utility/Spawn/EnemySpawner.hpp>

#include <Tsukino/EngineIntegration/EngineContext.hpp>

#include <Tsukino/Core/ECS/Event/EventBus.hpp>

#include <Tsukino/BuiltIn/ECS/Component/ModelComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/TransformComponent.hpp>

#include <entt/entt.hpp>

#include <algorithm>
#include <cmath>
#include <utility>

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

        //---------------------------------------------------------
        // 経過秒数と危険度はRunClockSystem（SystemPriority::RunClock＝本Systemより手前）が
        // 進めたものをそのまま読む。本Systemが自前で数えていた頃は、HUDの生存時間と
        // 別々に加算されていて死亡後にズレていた
        //---------------------------------------------------------
        const RunClockComponent* runClock = registry.try_get<RunClockComponent>(playerEntity);
        if(!runClock)
            return;

        const float elapsedSeconds = runClock->elapsedSeconds;
        const int   dangerRank     = runClock->dangerRank;

        //---------------------------------------------------------
        // (1) 間引き → (2) 数える → (3) 湧かせる の順で行う。
        // 生成・破棄は必ずViewの反復の外側（ここ）でのみ行う。
        // 反復中にエンティティを増減させるとEnTTのイテレータが壊れる
        //---------------------------------------------------------
        CullDistantEnemies(registry, playerPosition);
        UpdateSpawnFadeIn(registry, playerPosition, deltaTime);

        if(playerDead || elapsedSeconds < kWarmupSeconds)
            return;

        m_spawnTimer -= deltaTime;
        if(m_spawnTimer > 0.0f)
            return;

        // 経過時間で湧き間隔を詰める（線形）
        const float rampT = std::clamp(elapsedSeconds / kIntervalRampSeconds, 0.0f, 1.0f);
        m_spawnTimer       = kIntervalStart + (kIntervalEnd - kIntervalStart) * rampT;

        // クリア前のラスト1分は、詰めきった間隔をさらに縮めて山場にする
        if(elapsedSeconds >= kRunClearSeconds - kRunFinalStretchSeconds)
            m_spawnTimer *= kFinalStretchIntervalScale;

        //---------------------------------------------------------
        // 上限判定はシーン手置き・負荷試験が湧かせた分も含めた総数で行う。
        // こうしておくと、負荷試験（F1）で数百体出ている間は本Systemが
        // 自動的に湧きを止め、計測の邪魔をしない
        //---------------------------------------------------------
        int          liveCount    = CountLiveEnemies(registry);
        SectorCounts sectorCounts = CountEnemiesPerSector(registry, playerPosition);

        //---------------------------------------------------------
        // 上限に達していたら、霧の奥に取り残された雑魚を遠い順に消して枠を空ける。
        // プレイヤーは敵より3倍ほど速いので、一方向へ逃げると湧いた敵が全員背後に
        // 置き去りになり、それが上限を埋めて行く手に何も湧かなくなるため。
        // 取り残しは先にvectorへ集めておき、破棄はViewの反復の外で行う
        //---------------------------------------------------------
        std::vector<Tsukino::ECS::Entity> stragglers;
        if(liveCount >= kMaxLiveEnemies)
            stragglers = CollectStragglers(registry, playerPosition);

        size_t nextStraggler = 0;
        for(int i = 0; i < kSpawnBatchSize; ++i) {
            if(liveCount >= kMaxLiveEnemies) {
                if(nextStraggler >= stragglers.size())
                    break;

                // QueueDestroyした個体はフレーム末まで残り、CountLiveEnemiesにも数えられるので
                // 生存数はここで手元の値から引く
                DespawnSpawnedEnemy(registry, stragglers[nextStraggler++]);
                --liveCount;
            }

            SpawnOne(registry, *ctx, playerPosition, elapsedSeconds, dangerRank, sectorCounts);
            ++liveCount;
        }
    }

    //-------------------------------------------------------------
    //! @brief 敵を1体、抽選テーブルに従って湧かせる
    //-------------------------------------------------------------
    void EnemySpawnDirectorSystem::SpawnOne(Tsukino::ECS::Registry& registry,
                                            Tsukino::EngineIntegration::EngineContext& context,
                                            const hlslpp::float3& playerPosition,
                                            float                 elapsedSeconds,
                                            int                   dangerRank,
                                            SectorCounts&         sectorCounts) {
        const EnemySpawnTableEntry* entry = PickEnemyType(m_rng, elapsedSeconds);
        if(!entry)
            return;

        EnemySpawnConfig config = entry->makeConfig(context, ResolveSpawnPosition(playerPosition, sectorCounts));

        // 索敵距離の上書き。種類ごとの既定値（600）のままだとフォグの外から
        // 近づいてこないため、テーブルに何が増えてもここで一律に効かせる
        config.detectRange = kChaseDetectRange;

        //-----------------------------------------------------
        // ★ 危険度ランクに応じてHP・EXP・攻撃力・ひるみ閾値を底上げする ★
        //   ここを通らない生成経路（シーン手置き・F1の負荷試験）は素の値のままになる。
        //   負荷試験は1体あたりのコストを一定に保つ必要があり、手置きは見た目確認用なので、
        //   スケーリングを本Systemだけの責務に閉じておくのが正しい
        //-----------------------------------------------------
        ApplyEnemyDifficulty(config, dangerRank);

        //-----------------------------------------------------
        // エリート（強化個体）の抽選。危険度の補正の上から掛けるので、
        // 後半のエリートほど硬く強くなる
        //-----------------------------------------------------
        const bool isFinalStretch = elapsedSeconds >= kRunClearSeconds - kRunFinalStretchSeconds;
        const bool isElite        = RollElite(m_rng, dangerRank, isFinalStretch, CountLiveElites(registry));
        if(isElite)
            ApplyEliteModifiers(config);

        // 全個体の再生位置をずらす。揃っていると群れの足の運びが完全に一致し、
        // AnimationSystemの分岐も毎フレーム同じになって不自然に見える
        std::uniform_real_distribution<float> phaseDist(0.0f, 3.0f);
        config.initialAnimationTime = phaseDist(m_rng);

        Tsukino::ECS::Entity enemyEntity = SpawnBehaviorEnemy(registry, context, config);

        {
            const float dx = static_cast<float>(config.spawnPosition.x) - static_cast<float>(playerPosition.x);
            const float dz = static_cast<float>(config.spawnPosition.z) - static_cast<float>(playerPosition.z);
            registry.AddComponent<SpawnedEnemyComponent>(enemyEntity).spawnDistance = std::sqrt(dx * dx + dz * dz);
        }

        if(isElite) {
            registry.AddComponent<EliteEnemyComponent>(enemyEntity).baseType = entry->id;

            // エリートのPaladinは2本以上の武器を持ち、攻撃ごとに使い分ける（PaladinWeaponSwitchSystem）
            if(entry->id == EnemyTypeId::Paladin)
                EquipPaladinArsenal(registry, context, m_rng, enemyEntity);

            // 重いので押されにくくする（EnemySpawnConfigを経由しない値なので生成後に書く）
            if(auto* enemy = registry.try_get<EnemyComponent>(enemyEntity))
                enemy->knockbackDecayRate *= GetEliteSettings().knockbackDecayScale;

            // フォグの外から来るので、先に知らせて身構えさせる
            if(auto* eventBus = registry.GetContext<Tsukino::ECS::EventBus*>())
                eventBus->Publish(GameLogEvent{GameLogCategory::EliteAppeared, GetEliteDisplayName(entry->id)});
        }

        // 最初のフレームから見えないようにしておく（エリートPaladinの浮遊武器を持たせた後で書く）。
        // 以後はUpdateSpawnFadeInが霧の中から浮かび上がらせる
        ApplyEnemyOpacity(registry, enemyEntity, 0.0f);
    }

    //-------------------------------------------------------------
    //! @brief 湧いたばかりの敵を、霧の中から浮かび上がるようにフェードインさせる
    //-------------------------------------------------------------
    void EnemySpawnDirectorSystem::UpdateSpawnFadeIn(Tsukino::ECS::Registry& registry, const hlslpp::float3& playerPosition, float deltaTime) {
        auto view = registry.View<SpawnedEnemyComponent, HealthComponent, Tsukino::BuiltIn::ECS::TransformComponent>();
        view.each([&](entt::entity entity, SpawnedEnemyComponent& spawned, const HealthComponent& health,
                      const Tsukino::BuiltIn::ECS::TransformComponent& transform) {
            // 死亡演出中はZombieBehaviorのフェードアウトに任せる（取り合わない）
            if(spawned.fadeInDone || health.isDead)
                return;

            spawned.aliveTime += deltaTime;

            //-------------------------------------------------
            // 時間と距離のうち進んでいるほうを採る。
            // 時間だけだと、湧いた敵へ向かって走ったときに目の前で半透明のままになる。
            // 距離だけだと、プレイヤーが逃げている間はいつまでも見えない
            //-------------------------------------------------
            const float timeT = std::clamp(spawned.aliveTime / kFadeInSeconds, 0.0f, 1.0f);

            const float dx       = static_cast<float>(transform.position.x) - static_cast<float>(playerPosition.x);
            const float dz       = static_cast<float>(transform.position.z) - static_cast<float>(playerPosition.z);
            const float distance = std::sqrt(dx * dx + dz * dz);
            const float span     = std::max(spawned.spawnDistance - kFadeInOpaqueDistance, 1.0f);
            const float distT    = std::clamp((spawned.spawnDistance - distance) / span, 0.0f, 1.0f);

            const float t       = std::max(timeT, distT);
            const float opacity = t * t * (3.0f - 2.0f * t);    // スムーズステップ

            ApplyEnemyOpacity(registry, entity, opacity);

            if(t >= 1.0f)
                spawned.fadeInDone = true;
        });
    }

    //-------------------------------------------------------------
    //! @brief 敵本体と持っている武器の不透明度をまとめて書く
    //-------------------------------------------------------------
    void EnemySpawnDirectorSystem::ApplyEnemyOpacity(Tsukino::ECS::Registry& registry, Tsukino::ECS::Entity entity, float opacity) {
        auto apply = [&](Tsukino::ECS::Entity target) {
            if(target == entt::null)
                return;
            if(auto* model = registry.try_get<Tsukino::BuiltIn::ECS::ModelComponent>(target)) {
                model->opacity = opacity;
                model->visible = opacity > 0.0f;
            }
        };

        apply(entity);

        if(const EnemyHeldWeaponComponent* heldWeapon = registry.try_get<EnemyHeldWeaponComponent>(entity))
            apply(heldWeapon->weaponEntity);

        // エリートPaladinの浮遊武器（使用中の1本は上と重複するが、同じ値を書くだけ）
        if(const PaladinArsenalComponent* arsenal = registry.try_get<PaladinArsenalComponent>(entity)) {
            for(Tsukino::ECS::Entity weaponEntity : arsenal->weaponEntities)
                apply(weaponEntity);
        }
    }

    //-------------------------------------------------------------
    //! @brief 生きている（死亡演出に入っていない）エリートを数える
    //-------------------------------------------------------------
    int EnemySpawnDirectorSystem::CountLiveElites(Tsukino::ECS::Registry& registry) const {
        int  count = 0;
        auto view  = registry.View<EliteEnemyComponent, HealthComponent>();
        view.each([&](const EliteEnemyComponent&, const HealthComponent& health) {
            if(!health.isDead)
                ++count;
        });
        return count;
    }

    //-------------------------------------------------------------
    //! @brief プレイヤーを中心に、フォグの外側の湧き位置を1つ決める
    //-------------------------------------------------------------
    hlslpp::float3 EnemySpawnDirectorSystem::ResolveSpawnPosition(const hlslpp::float3& playerPosition, SectorCounts& sectorCounts) {
        std::uniform_real_distribution<float> unitDist(0.0f, 1.0f);
        std::uniform_real_distribution<float> radiusDist(kSpawnRadiusMin, kSpawnRadiusMax);

        //-----------------------------------------------------
        // 方角は「いま敵の少ない扇形」から選ぶ。一様に引くだけだと、
        // 一方向へ逃げたときに半分が背後へ湧いて即座に置き去りになり、行く手が空になる。
        // 敵の少ない方角＝逃げている先なので、自然に行く手の霧の奥から湧く。
        // 同数の扇形の並びは先にシャッフルして偏りを出さない（stable_sortで順序を保つ）。
        //
        // カメラ正面を避ける／狙う補正はあえて入れていない。
        // TPSカメラは自由に回るため「後ろから湧かせる」設計にしても、
        // プレイヤーが振り向いた瞬間に結局見えることになり意味が薄い
        //-----------------------------------------------------
        std::array<int, kSectorCount> order{};
        for(int s = 0; s < kSectorCount; ++s)
            order[static_cast<size_t>(s)] = s;
        std::shuffle(order.begin(), order.end(), m_rng);
        std::stable_sort(order.begin(), order.end(), [&](int a, int b) {
            return sectorCounts[static_cast<size_t>(a)] < sectorCounts[static_cast<size_t>(b)];
        });

        //-----------------------------------------------------
        // 地面の範囲による判定はしない。地面はGroundFollowSystemがプレイヤーへ追従させており、
        // プレイヤーの周囲は常に地面の上にある。以前はワールド原点中心の±4500を地面とみなして
        // いたため、原点から遠くまで走ると候補が全て外れ、その四角の端（プレイヤーのはるか後ろ）へ
        // 押し戻されて湧いた直後に間引かれ、敵が一切湧かなくなっていた
        //-----------------------------------------------------
        const int   sector      = order[0];
        const float sectorWidth = 2.0f * kPi / static_cast<float>(kSectorCount);
        const float angle       = -kPi + (static_cast<float>(sector) + unitDist(m_rng)) * sectorWidth;
        const float radius      = radiusDist(m_rng);

        // 同じバッチの次の1体が同じ扇形へ偏らないよう、湧かせた分をすぐ数えに入れる
        ++sectorCounts[static_cast<size_t>(sector)];

        return hlslpp::float3(playerPosition.x + std::cos(angle) * radius,
                              kSpawnHeight,
                              playerPosition.z + std::sin(angle) * radius);
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
            // 死亡演出中の個体を二重にQueueDestroyする可能性があるが、
            // FlushDestroyQueueが重複を除去するため問題ない
            //-------------------------------------------------
            DespawnSpawnedEnemy(registry, entity);
        });
    }

    //-------------------------------------------------------------
    //! @brief 本Systemが湧かせた敵1体を、HPバー・持ち武器ごと破棄予約する
    //-------------------------------------------------------------
    void EnemySpawnDirectorSystem::DespawnSpawnedEnemy(Tsukino::ECS::Registry& registry, Tsukino::ECS::Entity entity) {
        // 敵1体につきHPバーが2エンティティぶら下がっている。本体だけ消すと
        // HPバーが宙に浮いたまま残る（EnemyStressTestSystem::DespawnAllと同じ作法）
        if(const HealthComponent* health = registry.try_get<HealthComponent>(entity)) {
            if(health->hpBarBackgroundEntity != entt::null)
                registry.QueueDestroy(health->hpBarBackgroundEntity);
            if(health->hpBarFillEntity != entt::null)
                registry.QueueDestroy(health->hpBarFillEntity);
        }

        // 武器を持つ敵（Paladin等）は、その武器も一緒に消す。
        // 武器は敵の子エンティティではなく独立したエンティティなので、
        // ここで消し忘れると所有者を失った武器が地面に置き去りのまま永久に残り続ける
        // （撃破された場合はEnemyWeaponDropSystemが拾える武器として引き取るが、
        // 　間引きはその経路を通らない）
        if(const EnemyHeldWeaponComponent* heldWeapon = registry.try_get<EnemyHeldWeaponComponent>(entity)) {
            if(heldWeapon->weaponEntity != entt::null)
                registry.QueueDestroy(heldWeapon->weaponEntity);
        }

        // エリートのPaladinが浮かせている残りの武器も一緒に消す
        // （使用中の1本は上と重複するが、FlushDestroyQueueが重複を除去する）
        if(const PaladinArsenalComponent* arsenal = registry.try_get<PaladinArsenalComponent>(entity)) {
            for(Tsukino::ECS::Entity weaponEntity : arsenal->weaponEntities)
                registry.QueueDestroy(weaponEntity);
        }

        registry.QueueDestroy(entity);
    }

    //-------------------------------------------------------------
    //! @brief 囲み半径内の生存敵を、プレイヤーから見た方角の扇形ごとに数える
    //-------------------------------------------------------------
    EnemySpawnDirectorSystem::SectorCounts EnemySpawnDirectorSystem::CountEnemiesPerSector(Tsukino::ECS::Registry& registry,
                                                                                           const hlslpp::float3& playerPosition) const {
        // シーン手置き・負荷試験の敵も「囲んでいる敵」には違いないので、
        // SpawnedEnemyComponentの有無は問わずに数える
        SectorCounts counts{};
        const float  sectorWidth = 2.0f * kPi / static_cast<float>(kSectorCount);

        auto view = registry.View<EnemyComponent, HealthComponent, Tsukino::BuiltIn::ECS::TransformComponent>();
        view.each([&](const EnemyComponent&, const HealthComponent& health, const Tsukino::BuiltIn::ECS::TransformComponent& transform) {
            if(health.isDead)
                return;

            const float dx = static_cast<float>(transform.position.x) - static_cast<float>(playerPosition.x);
            const float dz = static_cast<float>(transform.position.z) - static_cast<float>(playerPosition.z);
            if(dx * dx + dz * dz > kSurroundRadius * kSurroundRadius)
                return;

            // ResolveSpawnPositionと同じく -π を扇形0の始まりとする
            const int sector = std::clamp(static_cast<int>((std::atan2(dz, dx) + kPi) / sectorWidth), 0, kSectorCount - 1);
            ++counts[static_cast<size_t>(sector)];
        });
        return counts;
    }

    //-------------------------------------------------------------
    //! @brief 囲み半径の外に取り残された、本Systemが湧かせた雑魚を遠い順に集める
    //-------------------------------------------------------------
    std::vector<Tsukino::ECS::Entity> EnemySpawnDirectorSystem::CollectStragglers(Tsukino::ECS::Registry& registry,
                                                                                  const hlslpp::float3& playerPosition) const {
        std::vector<std::pair<float, Tsukino::ECS::Entity>> candidates;

        auto view = registry.View<SpawnedEnemyComponent, HealthComponent, Tsukino::BuiltIn::ECS::TransformComponent>();
        view.each([&](entt::entity entity, const SpawnedEnemyComponent&, const HealthComponent& health,
                      const Tsukino::BuiltIn::ECS::TransformComponent& transform) {
            // 死亡演出中は自分で消えるので触らない。エリートは告知済みなので黙って消さない
            if(health.isDead || registry.HasComponent<EliteEnemyComponent>(entity))
                return;

            const float dx         = static_cast<float>(transform.position.x) - static_cast<float>(playerPosition.x);
            const float dz         = static_cast<float>(transform.position.z) - static_cast<float>(playerPosition.z);
            const float distanceSq = dx * dx + dz * dz;
            if(distanceSq > kSurroundRadius * kSurroundRadius)
                candidates.emplace_back(distanceSq, entity);
        });

        std::sort(candidates.begin(), candidates.end(), [](const auto& a, const auto& b) { return a.first > b.first; });

        std::vector<Tsukino::ECS::Entity> stragglers;
        stragglers.reserve(candidates.size());
        for(const auto& candidate : candidates)
            stragglers.push_back(candidate.second);
        return stragglers;
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
