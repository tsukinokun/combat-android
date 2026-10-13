//-------------------------------------------------------------
//! @file   SkillAuraParticleSystem.cpp
//! @brief  SkillAuraParticleSystemクラスの実装
//-------------------------------------------------------------
#include <CombatAndroid/ECS/System/Effect/SkillAuraParticleSystem.hpp>
#include <CombatAndroid/ECS/Component/Effect/AuraParticleComponent.hpp>
#include <CombatAndroid/ECS/Component/Player/PlayerComponent.hpp>
#include <CombatAndroid/ECS/Component/Progression/PlayerSkillComponent.hpp>
#include <CombatAndroid/ECS/Serialization/Common/SerializationHelper.hpp>
#include <CombatAndroid/ECS/Utility/Common/SkillAura.hpp>
#include <CombatAndroid/ECS/Utility/Table/TableJson.hpp>

#include <Tsukino/BuiltIn/ECS/Component/SpriteComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/TransformComponent.hpp>

#include <hlsl++.h>
#include <entt/entt.hpp>

#include <algorithm>
#include <cmath>
#include <optional>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    namespace {
        //-------------------------------------------------------------
        //! @struct SkillAuraParticleParams
        //! @brief  立ちのぼる粒のチューニング値（Assets/Tables/Systems/SkillAuraParticle.json。ここの初期値はJSONにキーが無いときの既定値）
        //! @note   距離はcm（1ユニット=1cm）。粒の大きさはTransformComponent.scaleで、
        //!         テクスチャ（AuraWisp.png、64x128px）のピクセル数に掛かる（0.32で幅約20cm・高さ約41cm）
        //-------------------------------------------------------------
        struct SkillAuraParticleParams {
            float saturation = 1.8f;    //!< panelColorの彩度を持ち上げる倍率（1=そのまま）
            float colorBoost = 2.2f;    //!< 色に掛ける倍率（加算合成なので、上げるほど粒が明るく白に寄る）

            //-------------------------------------------------------------
            // レベルで変わるもの。Lv1とLv5の間は線形に補間する。
            // 量だけ増やしても「濃くなった」としか見えず育ったことが伝わりにくいので、
            // 背丈（どこまで昇るか）・広がり・濃さも一緒に変え、Lv1は腰までの細い炎、
            // Lv5は頭の上まで噴き上がる、と一目で段階が分かるようにする
            //-------------------------------------------------------------
            float spawnRateLv1   = 12.0f;    //!< Lv1の1秒あたりの湧き数（まばら）
            float spawnRateLv5   = 50.0f;    //!< Lv5の1秒あたりの湧き数
            float sizeLv1        = 0.3f;     //!< Lv1の粒の大きさ
            float sizeLv5        = 0.48f;    //!< Lv5の粒の大きさ
            float heightScaleLv1 = 0.65f;    //!< Lv1の上昇速度の倍率（＝昇る高さ。腰のあたりで消える）
            float heightScaleLv5 = 1.35f;    //!< Lv5の上昇速度の倍率（頭の上まで昇る）
            float spawnRadiusLv1 = 40.0f;    //!< Lv1の足元の輪の半径
            float spawnRadiusLv5 = 62.0f;    //!< Lv5の足元の輪の半径
            float alphaLv1       = 0.65f;    //!< Lv1の粒1個の濃さ
            float alphaLv5       = 0.9f;     //!< Lv5の粒1個の濃さ

            // 湧き方と昇り方（レベルによらない基準値）
            float spawnHeight    = 15.0f;     //!< 湧く高さ（足元から）
            float riseSpeedMin   = 140.0f;    //!< 上昇速度の下限（cm/秒。heightScaleを掛ける前）
            float riseSpeedMax   = 230.0f;    //!< 上昇速度の上限（cm/秒。heightScaleを掛ける前）
            float lifetimeMin    = 0.8f;      //!< 寿命の下限（秒）
            float lifetimeMax    = 1.4f;      //!< 寿命の上限（秒）
            float inwardSpeed    = 0.35f;     //!< 昇りながら体の中心へ寄る速さ（1秒あたりに半径が縮む割合）
            float swayAmplitude  = 7.0f;      //!< 横揺れの幅（cm）
            float swayFrequency  = 6.0f;      //!< 横揺れの速さ（ラジアン/秒）
            float stretch        = 1.5f;      //!< 縦の伸び（昇るほどさらに細長くなる）
        };

        template <class Archive>
        void load(Archive& archive, SkillAuraParticleParams& params) {
            LoadField(archive, "saturation", params.saturation);
            LoadField(archive, "colorBoost", params.colorBoost);
            LoadField(archive, "spawnRateLv1", params.spawnRateLv1);
            LoadField(archive, "spawnRateLv5", params.spawnRateLv5);
            LoadField(archive, "sizeLv1", params.sizeLv1);
            LoadField(archive, "sizeLv5", params.sizeLv5);
            LoadField(archive, "heightScaleLv1", params.heightScaleLv1);
            LoadField(archive, "heightScaleLv5", params.heightScaleLv5);
            LoadField(archive, "spawnRadiusLv1", params.spawnRadiusLv1);
            LoadField(archive, "spawnRadiusLv5", params.spawnRadiusLv5);
            LoadField(archive, "alphaLv1", params.alphaLv1);
            LoadField(archive, "alphaLv5", params.alphaLv5);
            LoadField(archive, "spawnHeight", params.spawnHeight);
            LoadField(archive, "riseSpeedMin", params.riseSpeedMin);
            LoadField(archive, "riseSpeedMax", params.riseSpeedMax);
            LoadField(archive, "lifetimeMin", params.lifetimeMin);
            LoadField(archive, "lifetimeMax", params.lifetimeMax);
            LoadField(archive, "inwardSpeed", params.inwardSpeed);
            LoadField(archive, "swayAmplitude", params.swayAmplitude);
            LoadField(archive, "swayFrequency", params.swayFrequency);
            LoadField(archive, "stretch", params.stretch);
        }

        //-------------------------------------------------------------
        //! @brief  チューニング値を得る関数（初回の呼び出しで1度だけ読む）
        //-------------------------------------------------------------
        const SkillAuraParticleParams& GetParams() {
            static const SkillAuraParticleParams s_params = LoadSystemParams<SkillAuraParticleParams>("SkillAuraParticle");
            return s_params;
        }

        //! @brief 1フレームに湧かせる数の上限。ヒットストップ明けなどで経過時間が
        //!        大きく跳んだとき、プールを一度に食い潰して粒が一斉に揃うのを防ぐ
        constexpr int kMaxSpawnPerFrame = 6;

        //! @brief 2π
        constexpr float kTwoPi = 6.2831853f;

        [[nodiscard]]
        float Lerp(float a, float b, float t) {
            return a + (b - a) * t;
        }

        [[nodiscard]]
        float SmoothStep(float edge0, float edge1, float x) {
            const float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
            return t * t * (3.0f - 2.0f * t);
        }

        //-------------------------------------------------------------
        //! @brief  粒を非表示にしてプールへ戻す関数
        //! @note   SpriteRenderSystemは極小スケールを描画対象から外す（EXP玉と同じ流儀）
        //-------------------------------------------------------------
        void Deactivate(AuraParticleComponent& particle, Tsukino::BuiltIn::ECS::TransformComponent& transform) {
            particle.active = false;
            transform.scale = hlslpp::float3(0.0f, 0.0f, 0.0f);
            transform.dirty = true;
        }
    }    // namespace

    //-------------------------------------------------------------
    //! @brief システムの更新
    //-------------------------------------------------------------
    void SkillAuraParticleSystem::Update(Tsukino::ECS::Registry& registry, float deltaTime) {
        const SkillAuraParticleParams& params = GetParams();

        //-------------------------------------------------------------
        // プレイヤー（足元の位置と、取得スキル）を探す。1人しか居ない前提
        //-------------------------------------------------------------
        std::optional<hlslpp::float3> playerFeet;
        std::optional<DominantSkill>  dominant;
        {
            auto playerView = registry.View<PlayerComponent, PlayerSkillComponent, Tsukino::BuiltIn::ECS::TransformComponent>();
            for(entt::entity entity : playerView) {
                playerFeet = playerView.get<Tsukino::BuiltIn::ECS::TransformComponent>(entity).position;
                dominant   = ResolveDominantSkill(playerView.get<PlayerSkillComponent>(entity));
                break;
            }
        }

        auto particleView =
            registry.View<AuraParticleComponent, Tsukino::BuiltIn::ECS::TransformComponent, Tsukino::BuiltIn::ECS::SpriteComponent>();

        //-------------------------------------------------------------
        // プレイヤーが居なければ全て隠す（シーンの終わり際など）
        //-------------------------------------------------------------
        if(!playerFeet) {
            for(entt::entity entity : particleView) {
                auto& particle = particleView.get<AuraParticleComponent>(entity);
                if(particle.active)
                    Deactivate(particle, particleView.get<Tsukino::BuiltIn::ECS::TransformComponent>(entity));
            }
            m_spawnAccumulator = 0.0f;
            return;
        }

        //-------------------------------------------------------------
        // 湧かせる数を決める。スキル未取得なら湧かせず、残っている粒は寿命まで流しきる
        //-------------------------------------------------------------
        int spawnCount = 0;
        if(dominant) {
            m_spawnAccumulator += Lerp(params.spawnRateLv1, params.spawnRateLv5, dominant->levelT) * deltaTime;
            spawnCount = std::min(static_cast<int>(m_spawnAccumulator), kMaxSpawnPerFrame);
            m_spawnAccumulator -= static_cast<float>(static_cast<int>(m_spawnAccumulator));
        } else {
            m_spawnAccumulator = 0.0f;
        }

        // レベルで変わる値。スキル未取得（流しきるだけ）のときはLv1の値で見せる
        const float levelT      = dominant ? dominant->levelT : 0.0f;
        const float size        = Lerp(params.sizeLv1, params.sizeLv5, levelT);
        const float heightScale = Lerp(params.heightScaleLv1, params.heightScaleLv5, levelT);
        const float spawnRadius = Lerp(params.spawnRadiusLv1, params.spawnRadiusLv5, levelT);
        const float alpha       = Lerp(params.alphaLv1, params.alphaLv5, levelT);

        std::uniform_real_distribution<float> angleDist(0.0f, kTwoPi);
        std::uniform_real_distribution<float> radiusDist(0.6f, 1.0f);
        std::uniform_real_distribution<float> riseDist(params.riseSpeedMin, params.riseSpeedMax);
        std::uniform_real_distribution<float> lifeDist(params.lifetimeMin, params.lifetimeMax);
        std::uniform_real_distribution<float> sizeJitterDist(0.75f, 1.15f);

        //-------------------------------------------------------------
        // 色は全ての粒で共通（今の主の罪の色）。取り直した瞬間に昇っている途中の粒も
        // 新しい色へ変わるが、寿命が1秒前後なので切り替わりは目立たない
        //-------------------------------------------------------------
        hlslpp::float3 color(1.0f, 1.0f, 1.0f);
        if(dominant)
            color = ResolveSkillAuraColor(dominant->id, params.saturation) * params.colorBoost;

        for(entt::entity entity : particleView) {
            auto& particle  = particleView.get<AuraParticleComponent>(entity);
            auto& transform = particleView.get<Tsukino::BuiltIn::ECS::TransformComponent>(entity);
            auto& sprite    = particleView.get<Tsukino::BuiltIn::ECS::SpriteComponent>(entity);

            //-------------------------------------------------------------
            // 空きスロットへ新しい粒を湧かせる。足元の輪の上のランダムな点から
            //-------------------------------------------------------------
            if(!particle.active) {
                if(spawnCount <= 0)
                    continue;
                --spawnCount;

                const float angle  = angleDist(m_rng);
                const float radius = spawnRadius * radiusDist(m_rng);

                particle.active      = true;
                particle.localOffset = hlslpp::float3(std::cos(angle) * radius, params.spawnHeight, std::sin(angle) * radius);
                particle.riseSpeed   = riseDist(m_rng) * heightScale;
                particle.age         = 0.0f;
                particle.lifetime    = lifeDist(m_rng);
                particle.swayPhase   = angleDist(m_rng);
                particle.size        = size * sizeJitterDist(m_rng);
            } else {
                particle.age += deltaTime;
                if(particle.age >= particle.lifetime) {
                    Deactivate(particle, transform);
                    continue;
                }

                //-------------------------------------------------------------
                // 昇らせ、少しずつ体の中心へ寄せる（炎がすぼまって上へ抜けていく形）
                //-------------------------------------------------------------
                const float shrink = std::max(1.0f - params.inwardSpeed * deltaTime, 0.0f);
                particle.localOffset.x *= shrink;
                particle.localOffset.z *= shrink;
                particle.localOffset.y += particle.riseSpeed * deltaTime;
            }

            //-------------------------------------------------------------
            // 見た目。t=0で湧いて t=1で消える
            //   濃さ：最初の2割でふわっと現れ、後半で消えていく
            //   形　：昇るほど細く、縦に伸びる（炎の舌の先）
            //-------------------------------------------------------------
            const float t    = particle.age / particle.lifetime;
            const float fade = SmoothStep(0.0f, 0.2f, t) * (1.0f - SmoothStep(0.45f, 1.0f, t));

            const float sway = std::sin(particle.swayPhase + particle.age * params.swayFrequency) * params.swayAmplitude;
            const float swayX = sway * std::cos(particle.swayPhase);
            const float swayZ = sway * std::sin(particle.swayPhase);

            transform.position = *playerFeet + particle.localOffset + hlslpp::float3(swayX, 0.0f, swayZ);

            const float width  = particle.size * (1.0f - 0.55f * t);
            const float height = particle.size * params.stretch * (1.0f + 0.4f * t);
            transform.scale    = hlslpp::float3(width, height, 1.0f);
            transform.dirty    = true;

            sprite.tintColor = hlslpp::float4(color, alpha * fade);
        }
    }
}    // namespace CombatAndroid::ECS
