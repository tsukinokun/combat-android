//-------------------------------------------------------------
//! @file   EnemyWeaponDropSystem.cpp
//! @brief  EnemyWeaponDropSystemクラスの実装
//! @author 山﨑愛
//-------------------------------------------------------------
#include <CombatAndroid/ECS/System/Weapon/EnemyWeaponDropSystem.hpp>

#include <CombatAndroid/ECS/Component/Weapon/PickupComponent.hpp>
#include <CombatAndroid/ECS/Component/Weapon/WeaponDropFallComponent.hpp>
#include <CombatAndroid/ECS/Serialization/Common/SerializationHelper.hpp>
#include <CombatAndroid/ECS/Utility/Table/TableJson.hpp>
#include <CombatAndroid/ECS/Utility/Spawn/WeaponSpawner.hpp>

#include <Tsukino/BuiltIn/ECS/Component/TransformComponent.hpp>
#include <Tsukino/EngineIntegration/EngineContext.hpp>
#include <Tsukino/Core/Math/MathHelper.hpp>

#include <entt/entt.hpp>

#include <algorithm>
#include <cmath>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    namespace {
        //! 落とした武器を置く高さ。シーンへ手置きしている武器と同じ値にして、
        //! 拾えるアイテムが地面から浮く高さを揃える
        constexpr float kDropGroundHeight = 10.0f;

        constexpr float kPi = 3.14159265f;

        //-------------------------------------------------------------
        //! @struct EnemyWeaponDropParams
        //! @brief  落下演出の調整値（Assets/Tables/Systems/EnemyWeaponDrop.json。ここの初期値はJSONにキーが無いときの既定値）
        //-------------------------------------------------------------
        struct EnemyWeaponDropParams {
            //! 手を離れてから地面に横たわるまでの時間（ワールド時間。大技のスロー中は伸びる）
            float dropFallDuration = 0.4f;

            //! エリートが落とす武器を出現させる高さ（接地高さからの差）。
            //! 死亡演出で体は消えているので、宙から降ってくるように見せる
            float eliteDropStartHeight = 140.0f;

            //! エリートのPaladinが使っていなかった武器を、死亡位置からどれだけ離して落とすか。
            //! 重なっていると見分けられないので散らす。拾う対象はPickupSystemが最寄りの1本だけに
            //! 絞るので、拾える距離（150）の内側に入っていても、近づいた方から1本ずつ拾える
            float extraDropSpread      = 120.0f;
            float extraDropAngleOffset = 0.6f;    //!< 散らす向きの起点（真横に並ぶと画面奥の1本が隠れやすいので少し回す）
        };

        template <class Archive>
        void load(Archive& archive, EnemyWeaponDropParams& params) {
            LoadField(archive, "dropFallDuration", params.dropFallDuration);
            LoadField(archive, "eliteDropStartHeight", params.eliteDropStartHeight);
            LoadField(archive, "extraDropSpread", params.extraDropSpread);
            LoadField(archive, "extraDropAngleOffset", params.extraDropAngleOffset);
        }

        //-------------------------------------------------------------
        //! @brief  調整値を得る関数（初回の呼び出しで1度だけ読む）
        //-------------------------------------------------------------
        const EnemyWeaponDropParams& GetParams() {
            static const EnemyWeaponDropParams s_params = LoadSystemParams<EnemyWeaponDropParams>("EnemyWeaponDrop");
            return s_params;
        }

        //-------------------------------------------------------------
        //! @brief 0から1を滑らかに補間する関数（smoothstepの本体部分）
        //-------------------------------------------------------------
        [[nodiscard]]
        float SmoothStep01(float t) {
            t = std::clamp(t, 0.0f, 1.0f);
            return t * t * (3.0f - 2.0f * t);
        }
    }    // namespace

    //-------------------------------------------------------------
    //! @brief EnemyDiedEventの購読を開始する
    //-------------------------------------------------------------
    void EnemyWeaponDropSystem::Initialize(Tsukino::ECS::EventBus& eventBus) {
        (void)GetParams();    // 初回の撃破でファイルを読まないよう先に読んでおく
        m_diedConnection = eventBus.Subscribe<EnemyDiedEvent>([this](const EnemyDiedEvent& event) { OnEnemyDied(event); });
    }

    //-------------------------------------------------------------
    //! @brief 死亡通知のハンドラ
    //-------------------------------------------------------------
    void EnemyWeaponDropSystem::OnEnemyDied(const EnemyDiedEvent& event) {
        // 武器を持っていない普通の敵（ゾンビ系）の通知は積まない。
        // 撃破の大半はこちらなので、Update側の空回りを避ける
        if(event.heldWeaponEntity == entt::null && event.extraWeaponEntities.empty() && !event.isElite)
            return;

        m_pending.push_back(event);
    }

    //-------------------------------------------------------------
    //! @brief システムの更新
    //-------------------------------------------------------------
    void EnemyWeaponDropSystem::Update(Tsukino::ECS::Registry& registry, float deltaTime) {
        const EnemyWeaponDropParams& params = GetParams();

        //-------------------------------------------------------------
        // 死亡通知を受けた武器を手から外し、落下を始めさせる。
        // 位置はまだ動かさず、このフレームの手の姿勢を落下の始点にする
        //-------------------------------------------------------------
        auto* context = registry.GetContext<Tsukino::EngineIntegration::EngineContext*>();
        if(!context)
            return;    // 武器の落ちている姿勢などをPrefabから引くのに要る

        for(const EnemyDiedEvent& pending : m_pending) {
            // 死亡位置は敵の足元。武器は接地高さへ置き直して横たわらせる
            hlslpp::float3 dropPosition = pending.position;
            dropPosition.y              = kDropGroundHeight;

            //-------------------------------------------------------------
            // 使っていなかった武器（エリートのPaladin）。同じ場所に重なると
            // どれを拾うか選べないので、死亡位置のまわりへ円状に散らして落とす
            //-------------------------------------------------------------
            const int extraCount = static_cast<int>(pending.extraWeaponEntities.size());
            for(int i = 0; i < extraCount; ++i) {
                const float    angle = params.extraDropAngleOffset + 2.0f * kPi * static_cast<float>(i) / static_cast<float>(extraCount);
                hlslpp::float3 extraPosition = dropPosition;
                extraPosition.x += std::cos(angle) * params.extraDropSpread;
                extraPosition.z += std::sin(angle) * params.extraDropSpread;

                BeginWeaponDrop(registry, *context, pending.extraWeaponEntities[static_cast<size_t>(i)], extraPosition);
            }

            if(pending.heldWeaponEntity != entt::null) {
                BeginWeaponDrop(registry, *context, pending.heldWeaponEntity, dropPosition);
                continue;
            }

            //-------------------------------------------------------------
            // 武器を持っていないエリート。ランダムな武器を宙に作って落とす。
            // SpawnWeaponは持ち主なしで作ると最初から拾える状態にするが、
            // 落ちている途中で拾えると着地の演出が飛ぶので、着地まで外しておく
            // （着地時のDropWeaponToWorldが付け直す）
            //-------------------------------------------------------------
            if(!pending.isElite || !context->assetManager)
                continue;

            std::uniform_int_distribution<int> weaponDist(0, static_cast<int>(WeaponId::Count) - 1);
            const WeaponId                     weaponId = static_cast<WeaponId>(weaponDist(m_rng));

            hlslpp::float3 spawnPosition = dropPosition;
            spawnPosition.y += params.eliteDropStartHeight;

            Tsukino::ECS::Entity weaponEntity = SpawnWeapon(registry, *context, weaponId, spawnPosition);
            if(registry.HasComponent<PickupComponent>(weaponEntity))
                registry.RemoveComponent<PickupComponent>(weaponEntity);

            BeginWeaponDrop(registry, *context, weaponEntity, dropPosition);
        }
        m_pending.clear();

        //-------------------------------------------------------------
        // 落下中の武器を進める。水平方向は等速、高さは加速しながら落とし（重力っぽさ）、
        // 姿勢は手の向きから横たわりへ滑らかに倒す
        //-------------------------------------------------------------
        std::vector<Tsukino::ECS::Entity> landedWeapons;

        auto fallView = registry.View<WeaponDropFallComponent, Tsukino::BuiltIn::ECS::TransformComponent>();
        fallView.each([&](Tsukino::ECS::Entity entity, WeaponDropFallComponent& fall,
                          Tsukino::BuiltIn::ECS::TransformComponent& transform) {
            fall.timer += deltaTime;
            float t = std::clamp(fall.timer / params.dropFallDuration, 0.0f, 1.0f);

            hlslpp::float3 position = hlslpp::lerp(fall.startPosition, fall.groundPosition, t);
            position.y              = fall.startPosition.y + (fall.groundPosition.y - fall.startPosition.y) * (t * t);

            transform.position = position;
            transform.rotation = Tsukino::Core::Math::SlerpShortestPath(fall.startRotation, fall.groundRotation, SmoothStep01(t));
            transform.dirty    = true;

            if(t >= 1.0f)
                landedWeapons.push_back(entity);
        });

        //-------------------------------------------------------------
        // 着地した武器を拾える状態にする。コンポーネント構成が変わるのでViewの外で行う
        //-------------------------------------------------------------
        for(Tsukino::ECS::Entity entity : landedWeapons) {
            hlslpp::float3 groundPosition = registry.GetComponent<WeaponDropFallComponent>(entity).groundPosition;
            registry.RemoveComponent<WeaponDropFallComponent>(entity);

            DropWeaponToWorld(registry, *context, entity, groundPosition);
        }
    }
}    // namespace CombatAndroid::ECS
