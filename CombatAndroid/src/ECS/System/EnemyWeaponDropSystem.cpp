//-------------------------------------------------------------
//! @file   EnemyWeaponDropSystem.cpp
//! @brief  EnemyWeaponDropSystemクラスの実装
//! @author 山﨑愛
//-------------------------------------------------------------
#include <CombatAndroid/ECS/System/EnemyWeaponDropSystem.hpp>

#include <CombatAndroid/ECS/Component/WeaponDropFallComponent.hpp>
#include <CombatAndroid/ECS/Utility/WeaponSpawner.hpp>

#include <Tsukino/BuiltIn/ECS/Component/TransformComponent.hpp>
#include <Tsukino/Core/Math/MathHelper.hpp>

#include <entt/entt.hpp>

#include <algorithm>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    namespace {
        //! 落とした武器を置く高さ。シーンへ手置きしている武器と同じ値にして、
        //! 拾えるアイテムが地面から浮く高さを揃える
        constexpr float kDropGroundHeight = 10.0f;

        //! 手を離れてから地面に横たわるまでの時間（ワールド時間。大技のスロー中は伸びる）
        constexpr float kDropFallDuration = 0.4f;

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
        m_diedConnection = eventBus.Subscribe<EnemyDiedEvent>([this](const EnemyDiedEvent& event) { OnEnemyDied(event); });
    }

    //-------------------------------------------------------------
    //! @brief 死亡通知のハンドラ
    //-------------------------------------------------------------
    void EnemyWeaponDropSystem::OnEnemyDied(const EnemyDiedEvent& event) {
        // 武器を持っていない敵（ゾンビ系）の通知は積まない。
        // 撃破の大半はこちらなので、Update側の空回りを避ける
        if(event.heldWeaponEntity == entt::null)
            return;

        m_pending.push_back(event);
    }

    //-------------------------------------------------------------
    //! @brief システムの更新
    //-------------------------------------------------------------
    void EnemyWeaponDropSystem::Update(Tsukino::ECS::Registry& registry, float deltaTime) {
        //-------------------------------------------------------------
        // 死亡通知を受けた武器を手から外し、落下を始めさせる。
        // 位置はまだ動かさず、このフレームの手の姿勢を落下の始点にする
        //-------------------------------------------------------------
        for(const EnemyDiedEvent& pending : m_pending) {
            // 死亡位置は敵の足元。武器は接地高さへ置き直して横たわらせる
            hlslpp::float3 dropPosition = pending.position;
            dropPosition.y              = kDropGroundHeight;

            BeginWeaponDrop(registry, pending.heldWeaponEntity, dropPosition);
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
            float t = std::clamp(fall.timer / kDropFallDuration, 0.0f, 1.0f);

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

            DropWeaponToWorld(registry, entity, groundPosition);
        }
    }
}    // namespace CombatAndroid::ECS
