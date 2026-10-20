//-------------------------------------------------------------
//! @file   EnemyTreeCollisionSystem.cpp
//! @brief  EnemyTreeCollisionSystemクラスの実装
//-------------------------------------------------------------
#include <CombatAndroid/ECS/System/Enemy/EnemyTreeCollisionSystem.hpp>
#include <CombatAndroid/ECS/Component/Enemy/EnemyComponent.hpp>
#include <CombatAndroid/ECS/Component/Player/PlayerComponent.hpp>
#include <CombatAndroid/ECS/Serialization/Common/SerializationHelper.hpp>
#include <CombatAndroid/ECS/Utility/Table/TableJson.hpp>
#include <CombatAndroid/ECS/Utility/World/TreeObstacle.hpp>

#include <Tsukino/BuiltIn/ECS/Component/CollisionComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/TransformComponent.hpp>

#include <entt/entt.hpp>

#include <algorithm>
#include <cmath>
#include <optional>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    namespace {
        //-------------------------------------------------------------
        //! @struct EnemyTreeCollisionParams
        //! @brief  調整値（Assets/Tables/Systems/EnemyTreeCollision.json。ここの初期値はJSONにキーが無いときの既定値）
        //-------------------------------------------------------------
        struct EnemyTreeCollisionParams {
            float activeRange  = 3000.0f;    //!< プレイヤーからこの距離以内の敵だけ押し出す（木を描いている範囲＝drawRadiusと同じ。遠くの見えない敵は省く）
            float slideBias    = 0.5f;       //!< 押し出し量のうち、幹の接線方向（回り込む向き）へも振り分ける割合
            float extraPadding = 0.0f;       //!< 幹と敵の間に空ける余白（cm）
        };

        template <class Archive>
        void load(Archive& archive, EnemyTreeCollisionParams& params) {
            LoadField(archive, "activeRange", params.activeRange);
            LoadField(archive, "slideBias", params.slideBias);
            LoadField(archive, "extraPadding", params.extraPadding);
        }

        //-------------------------------------------------------------
        //! @brief  チューニング値を得る関数（初回の呼び出しで読む。Debugでは保存し直すと読み直す）
        //-------------------------------------------------------------
        const EnemyTreeCollisionParams& GetParams() {
            return GetSystemParams<EnemyTreeCollisionParams>("EnemyTreeCollision");
        }
    }    // namespace

    //-------------------------------------------------------------
    //! @brief システムの更新
    //-------------------------------------------------------------
    void EnemyTreeCollisionSystem::Update(Tsukino::ECS::Registry& registry, float deltaTime) {
        (void)deltaTime;

        if(!registry.HasContext<TreeObstacleContext>())
            return;
        const TreeObstacleContext& obstacles = registry.GetContext<TreeObstacleContext>();
        if(obstacles.trunks.empty())
            return;

        //-------------------------------------------------------------
        // プレイヤーの位置（遠くの敵を省くのと、回り込む向きを決めるのに使う）
        //-------------------------------------------------------------
        std::optional<hlslpp::float3> playerPosition;
        {
            auto playerView = registry.View<PlayerComponent, Tsukino::BuiltIn::ECS::TransformComponent>();
            for(entt::entity entity : playerView) {
                playerPosition = playerView.get<Tsukino::BuiltIn::ECS::TransformComponent>(entity).position;
                break;
            }
        }
        if(!playerPosition)
            return;

        const EnemyTreeCollisionParams& params        = GetParams();
        const float                     activeRangeSq = params.activeRange * params.activeRange;

        auto view = registry.View<EnemyComponent, Tsukino::BuiltIn::ECS::TransformComponent>();
        for(entt::entity entity : view) {
            auto& transform = view.get<Tsukino::BuiltIn::ECS::TransformComponent>(entity);

            const float toPlayerX = static_cast<float>(playerPosition->x - transform.position.x);
            const float toPlayerZ = static_cast<float>(playerPosition->z - transform.position.z);
            if(toPlayerX * toPlayerX + toPlayerZ * toPlayerZ > activeRangeSq)
                continue;

            //-------------------------------------------------------------
            // 敵の体の半径。当たり判定のカプセルの半径を使い、無ければEnemyComponentの体の半径
            //-------------------------------------------------------------
            float bodyRadius = view.get<EnemyComponent>(entity).bodyRadius;
            if(const auto* collision = registry.try_get<Tsukino::BuiltIn::ECS::CollisionComponent>(entity);
               collision && collision->type == Tsukino::BuiltIn::ECS::ColliderType::Capsule)
                bodyRadius = collision->extent.x;

            float x     = transform.position.x;
            float z     = transform.position.z;
            bool  moved = false;

            ForEachTreeObstacleNear(obstacles, x, z, [&](const TreeObstacle& trunk) {
                const float minDist = bodyRadius + trunk.radius + params.extraPadding;
                float       dx      = x - trunk.x;
                float       dz      = z - trunk.z;
                float       dist    = std::sqrt(dx * dx + dz * dz);
                if(dist >= minDist)
                    return;

                //-------------------------------------------------------------
                // 幹の中心から外への向き。中心がぴったり重なったら、決まった向き（+X）へ逃がす
                //-------------------------------------------------------------
                const float penetration = minDist - dist;
                if(dist < 0.001f) {
                    dx   = 1.0f;
                    dz   = 0.0f;
                    dist = 1.0f;
                }
                const float nx = dx / dist;
                const float nz = dz / dist;

                //-------------------------------------------------------------
                // 回り込む向き（接線）。2つある接線のうちプレイヤーに近づく側を選ぶ。
                // ちょうど真正面で決められないときは、敵ごとに決まった側（番号の偶奇）にして、毎フレーム揺れないようにする
                //-------------------------------------------------------------
                float       tx    = -nz;
                float       tz    = nx;
                const float along = tx * toPlayerX + tz * toPlayerZ;
                const bool  flip  = (std::abs(along) < 1.0f) ? ((static_cast<std::uint32_t>(entt::to_integral(entity)) & 1u) != 0u) : (along < 0.0f);
                if(flip) {
                    tx = -tx;
                    tz = -tz;
                }

                x += nx * penetration + tx * penetration * params.slideBias;
                z += nz * penetration + tz * penetration * params.slideBias;

                // 接線へ寄せたぶんで幹へ食い込み直さないよう、最後に幹の表面へそろえる
                const float ex    = x - trunk.x;
                const float ez    = z - trunk.z;
                const float eDist = std::sqrt(ex * ex + ez * ez);
                if(eDist > 0.001f && eDist < minDist) {
                    x = trunk.x + ex / eDist * minDist;
                    z = trunk.z + ez / eDist * minDist;
                }
                moved = true;
            });

            if(moved) {
                transform.position.x = x;
                transform.position.z = z;
                transform.dirty      = true;
            }
        }
    }
}    // namespace CombatAndroid::ECS
