//-------------------------------------------------------------
//! @file   EnemyAttackTelegraphSystem.cpp
//! @brief  EnemyAttackTelegraphSystemクラスの実装
//-------------------------------------------------------------
#include <CombatAndroid/ECS/System/Enemy/EnemyAttackTelegraphSystem.hpp>
#include <CombatAndroid/ECS/Serialization/Common/SerializationHelper.hpp>
#include <CombatAndroid/ECS/Utility/Table/TableJson.hpp>
#include <Tsukino/Core/Math/Serialization/HlslppSerialization.hpp>
#include <CombatAndroid/ECS/Component/Enemy/EliteEnemyComponent.hpp>
#include <CombatAndroid/ECS/Component/Enemy/EnemyAnimationSetComponent.hpp>
#include <CombatAndroid/ECS/Component/Enemy/EnemyAttackHitboxComponent.hpp>
#include <CombatAndroid/ECS/Component/Enemy/EnemyComponent.hpp>
#include <CombatAndroid/ECS/Component/Combat/HealthComponent.hpp>
#include <CombatAndroid/ECS/Utility/Spawn/EliteEnemy.hpp>

#include <Tsukino/BuiltIn/ECS/Component/RimGlowComponent.hpp>

#include <entt/entt.hpp>

#include <algorithm>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    namespace {
        //-------------------------------------------------------------
        //! @struct EnemyAttackTelegraphParams
        //! @brief  見た目と挙動のチューニング値（Assets/Tables/Systems/EnemyAttackTelegraph.json。ここの初期値はJSONにキーが無いときの既定値）
        //-------------------------------------------------------------
        struct EnemyAttackTelegraphParams {
            //-------------------------------------------------------------
            // 予兆の見た目のチューニング値。
            // 敵は数が多いので、常時光る演出ではなく「振りかぶりの間だけ赤くなる」ことを狙う
            //-------------------------------------------------------------
            hlslpp::float3 telegraphColor = hlslpp::float3(1.0f, 0.12f, 0.08f);    //!< 赤。スキル・拾得の金色と混ざらない色味にする

            float rimIntensityMax = 6.0f;     //!< 判定が出る直前のふちの強さ
            float glowMax = 0.30f;    //!< 同じく面全体の持ち上げ量（強くすると白飛びする）
            float rimPower = 2.2f;     //!< ふちの鋭さ。溜め攻撃（2.5）よりわずかに広く出す

            //! 振りかぶりの何割を過ぎてから光らせ始めるか。
            //! 0から光らせると「攻撃に入った瞬間」に全員が赤くなり、かえって読み取りにくい
            float startRatio = 0.25f;

            //-------------------------------------------------------------
            // エリートが常に纏う発光。予兆より弱くして、予兆（赤）へ向かって
            // 色と強さが連続的に移るようにする（紫のまま急に赤へ跳ばない）
            //-------------------------------------------------------------
            float eliteRimIntensity = 3.0f;
            float eliteGlow = 0.06f;
        };

        template <class Archive>
        void load(Archive& archive, EnemyAttackTelegraphParams& params) {
            LoadField(archive, "telegraphColor", params.telegraphColor);
            LoadField(archive, "rimIntensityMax", params.rimIntensityMax);
            LoadField(archive, "glowMax", params.glowMax);
            LoadField(archive, "rimPower", params.rimPower);
            LoadField(archive, "startRatio", params.startRatio);
            LoadField(archive, "eliteRimIntensity", params.eliteRimIntensity);
            LoadField(archive, "eliteGlow", params.eliteGlow);
        }

        //-------------------------------------------------------------
        //! @brief  チューニング値を得る関数（初回の呼び出しで1度だけ読む）
        //-------------------------------------------------------------
        const EnemyAttackTelegraphParams& GetParams() {
            static const EnemyAttackTelegraphParams s_params = LoadSystemParams<EnemyAttackTelegraphParams>("EnemyAttackTelegraph");
            return s_params;
        }

        //-------------------------------------------------------------
        //! @struct RimState
        //! @brief  リムライトに書く値一式（消灯・エリート常時・予兆の間で補間するため）
        //-------------------------------------------------------------
        struct RimState {
            hlslpp::float3 color;
            float          intensity;
            float          glow;
        };
    }    // namespace

    //-------------------------------------------------------------
    //! @brief システムの更新
    //-------------------------------------------------------------
    void EnemyAttackTelegraphSystem::Update(Tsukino::ECS::Registry& registry, float /*deltaTime*/) {
        const EnemyAttackTelegraphParams& params = GetParams();


        auto view = registry.View<EnemyComponent, EnemyAnimationSetComponent, EnemyAttackHitboxComponent, HealthComponent,
                                  Tsukino::BuiltIn::ECS::RimGlowComponent>();

        view.each([&registry, &params](Tsukino::ECS::Entity entity, EnemyComponent&, const EnemyAnimationSetComponent& animSet,
                              const EnemyAttackHitboxComponent& hitbox, const HealthComponent& health,
                              Tsukino::BuiltIn::ECS::RimGlowComponent& rimGlow) {
            //-------------------------------------------------------------
            // 平常時の発光。エリートは紫を纏い、それ以外は消灯。
            // 死亡中は死亡演出（フェード）の邪魔になるので、エリートでも消す
            //-------------------------------------------------------------
            const bool     isElite = !health.isDead && registry.HasComponent<EliteEnemyComponent>(entity);
            const RimState rest    = isElite ? RimState{GetEliteSettings().glowColor, params.eliteRimIntensity, params.eliteGlow}
                                             : RimState{params.telegraphColor, 0.0f, 0.0f};

            //-------------------------------------------------------------
            // 予兆を出す条件：攻撃モーション中で、まだ判定が出ていないこと。
            // 死亡中は出さない
            //-------------------------------------------------------------
            const bool isWindingUp = !health.isDead && animSet.currentState == EnemyAnimState::Attack
                                     && animSet.attackTimer < hitbox.hitStartTime && hitbox.hitStartTime > 0.0f;

            //-------------------------------------------------------------
            // 判定が出る瞬間へ向けて、平常時の発光から予兆の赤へ強めていく。
            // kStartRatioまでは平常時のまま溜める。
            // 他に敵のRimGlowを書くSystemは無いので、ここで毎フレーム書き切ってよい
            //-------------------------------------------------------------
            float strength = 0.0f;
            if(isWindingUp) {
                const float windupProgress = std::clamp(animSet.attackTimer / hitbox.hitStartTime, 0.0f, 1.0f);
                strength = std::clamp((windupProgress - params.startRatio) / std::max(1.0f - params.startRatio, 0.01f), 0.0f, 1.0f);
            }

            rimGlow.rimColor     = hlslpp::lerp(rest.color, params.telegraphColor, strength);
            rimGlow.rimIntensity = rest.intensity + (params.rimIntensityMax - rest.intensity) * strength;
            rimGlow.rimPower     = params.rimPower;
            rimGlow.glow         = rest.glow + (params.glowMax - rest.glow) * strength;
            rimGlow.active       = rimGlow.rimIntensity > 0.0f || rimGlow.glow > 0.0f;
        });
    }
}    // namespace CombatAndroid::ECS
