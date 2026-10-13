//-------------------------------------------------------------
//! @file   SkillAuraSystem.cpp
//! @brief  SkillAuraSystemクラスの実装
//-------------------------------------------------------------
#include <CombatAndroid/ECS/System/Effect/SkillAuraSystem.hpp>
#include <CombatAndroid/ECS/Component/Effect/SkillAuraComponent.hpp>
#include <CombatAndroid/ECS/Component/Player/PlayerComponent.hpp>
#include <CombatAndroid/ECS/Component/Progression/PlayerSkillComponent.hpp>
#include <CombatAndroid/ECS/Serialization/Common/SerializationHelper.hpp>
#include <CombatAndroid/ECS/Utility/Common/SkillAura.hpp>
#include <CombatAndroid/ECS/Utility/Table/TableJson.hpp>

#include <Tsukino/BuiltIn/ECS/Component/RimGlowComponent.hpp>

#include <hlsl++.h>
#include <entt/entt.hpp>

#include <cmath>
#include <optional>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    namespace {
        //-------------------------------------------------------------
        //! @struct SkillAuraParams
        //! @brief  体の発光のチューニング値（Assets/Tables/Systems/SkillAura.json。ここの初期値はJSONにキーが無いときの既定値）
        //! @note   色そのものはここに持たない。Skills.jsonのpanelColorを使う（スキルの色の実体は1か所）。
        //!         オーラの主役は立ちのぼる粒（SkillAuraParticleSystem）で、こちらは体にうっすら色を添える脇役。
        //!         強くしすぎると体の色が飛んで、キャラクターが読めなくなる
        //-------------------------------------------------------------
        struct SkillAuraParams {
            float colorBoost = 2.0f;    //!< 色に掛ける倍率（HDR。1.0超で輪郭が明るく光る）
            float saturation = 1.8f;    //!< panelColorの彩度を持ち上げる倍率（1=そのまま）

            // レベルによる強さ。Lv1とLv5の間は線形に補間する
            float rimIntensityLv1 = 0.5f;    //!< Lv1の輪郭の強さ（控えめ）
            float rimIntensityLv5 = 1.2f;    //!< Lv5の輪郭の強さ
            float rimPower        = 2.5f;    //!< 輪郭の鋭さ（大きいほど縁に集中する）

            // 脈動（ゆっくり呼吸するように明滅させる）
            float pulseSpeed = 2.2f;     //!< 脈動の速さ（ラジアン/秒）
            float pulseDepth = 0.25f;    //!< 脈動で強さが沈む割合（0=脈動なし、1=消えるまで沈む）

            //! 色・強さ・レベルを目標値へ寄せる速さ（1秒あたり、残りの差をどれだけ詰めるかの指数）。
            //! スキルを取った瞬間に色がパッと切り替わらず、数分の1秒で移り変わる
            float blendSpeed = 4.0f;
        };

        template <class Archive>
        void load(Archive& archive, SkillAuraParams& params) {
            LoadField(archive, "colorBoost", params.colorBoost);
            LoadField(archive, "saturation", params.saturation);
            LoadField(archive, "rimIntensityLv1", params.rimIntensityLv1);
            LoadField(archive, "rimIntensityLv5", params.rimIntensityLv5);
            LoadField(archive, "rimPower", params.rimPower);
            LoadField(archive, "pulseSpeed", params.pulseSpeed);
            LoadField(archive, "pulseDepth", params.pulseDepth);
            LoadField(archive, "blendSpeed", params.blendSpeed);
        }

        //-------------------------------------------------------------
        //! @brief  チューニング値を得る関数（初回の呼び出しで1度だけ読む）
        //-------------------------------------------------------------
        const SkillAuraParams& GetParams() {
            static const SkillAuraParams s_params = LoadSystemParams<SkillAuraParams>("SkillAura");
            return s_params;
        }

        //-------------------------------------------------------------
        //! @brief  フレームレートに依存しない指数的な寄せ（1フレームで詰める割合）
        //-------------------------------------------------------------
        [[nodiscard]]
        float BlendFactor(float speed, float deltaTime) {
            return 1.0f - std::exp(-speed * deltaTime);
        }

        [[nodiscard]]
        float Lerp(float a, float b, float t) {
            return a + (b - a) * t;
        }
    }    // namespace

    //-------------------------------------------------------------
    //! @brief システムの更新
    //-------------------------------------------------------------
    void SkillAuraSystem::Update(Tsukino::ECS::Registry& registry, float deltaTime) {
        const SkillAuraParams& params = GetParams();
        const float            blend  = BlendFactor(params.blendSpeed, deltaTime);

        auto view = registry.View<PlayerComponent, PlayerSkillComponent, SkillAuraComponent, Tsukino::BuiltIn::ECS::RimGlowComponent>();
        for(entt::entity entity : view) {
            const auto& player = view.get<PlayerComponent>(entity);
            const auto& skills = view.get<PlayerSkillComponent>(entity);
            auto&       aura   = view.get<SkillAuraComponent>(entity);
            auto&       rim    = view.get<Tsukino::BuiltIn::ECS::RimGlowComponent>(entity);

            //-------------------------------------------------------------
            // 目標値へ寄せる。スキル未取得なら何も書かない
            // （RimGlowは溜め・無敵の演出に任せ、消灯もあちらが行う）
            //-------------------------------------------------------------
            const std::optional<DominantSkill> dominant = ResolveDominantSkill(skills);
            if(!dominant) {
                aura.currentStrength = 0.0f;
                continue;
            }

            const hlslpp::float3 targetColor = ResolveSkillAuraColor(dominant->id, params.saturation) * params.colorBoost;

            // 初めて点くときは色だけ即座に合わせ、強さを0から上げてフェードインさせる
            //（黒から目標色へ寄せると、一瞬くすんだ色が出てしまうため）
            if(aura.currentStrength <= 0.0f) {
                aura.currentColor  = targetColor;
                aura.currentLevelT = dominant->levelT;
            }
            aura.currentColor    = aura.currentColor + (targetColor - aura.currentColor) * blend;
            aura.currentLevelT   = Lerp(aura.currentLevelT, dominant->levelT, blend);
            aura.currentStrength = Lerp(aura.currentStrength, 1.0f, blend);
            aura.pulseTime += deltaTime;

            //-------------------------------------------------------------
            // 溜め攻撃・レベルアップ後無敵の発光が優先。その間は書き込まない
            //-------------------------------------------------------------
            if(player.isCharging || player.levelUpInvincibleTimer > 0.0f)
                continue;

            //-------------------------------------------------------------
            // 脈動：sin²で0〜1を作り、pulseDepthの分だけ強さを沈める
            //-------------------------------------------------------------
            const float wave  = std::sin(aura.pulseTime * params.pulseSpeed);
            const float pulse = 1.0f - params.pulseDepth * (1.0f - wave * wave);
            const float scale = aura.currentStrength * pulse;

            //-------------------------------------------------------------
            // glow（面全体の持ち上げ）は使わない。RimGlowComponent::glowは色を持たず
            // 白で足されるので、オーラの色が白っぽく濁る
            //-------------------------------------------------------------
            rim.active       = true;
            rim.rimColor     = aura.currentColor;
            rim.rimIntensity = Lerp(params.rimIntensityLv1, params.rimIntensityLv5, aura.currentLevelT) * scale;
            rim.rimPower     = params.rimPower;
            rim.glow         = 0.0f;
        }
    }
}    // namespace CombatAndroid::ECS
