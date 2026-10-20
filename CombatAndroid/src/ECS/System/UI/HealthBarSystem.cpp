//-------------------------------------------------------------
//! @file   HealthBarSystem.cpp
//! @brief  HealthBarSystemクラスの実装
//-------------------------------------------------------------
#include <CombatAndroid/ECS/System/UI/HealthBarSystem.hpp>
#include <CombatAndroid/ECS/Serialization/Common/SerializationHelper.hpp>
#include <CombatAndroid/ECS/Utility/Table/TableJson.hpp>
#include <Tsukino/Core/Math/Serialization/HlslppSerialization.hpp>
#include <CombatAndroid/ECS/Component/Combat/HealthComponent.hpp>

#include <Tsukino/BuiltIn/ECS/Component/TransformComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/WorldAnchorComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/SpriteComponent.hpp>

#include <hlsl++.h>
#include <entt/entt.hpp>

#include <algorithm>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    namespace {
        // HPバー用テクスチャ（1辺kBarTexturePixelSizeの単色正方形）をこのピクセルサイズまで引き伸ばして表示する
        constexpr float kBarTexturePixelSize = 4.0f;

        //-------------------------------------------------------------
        //! @struct HealthBarParams
        //! @brief  見た目と挙動のチューニング値（Assets/Tables/Systems/HealthBar.json。ここの初期値はJSONにキーが無いときの既定値）
        //-------------------------------------------------------------
        struct HealthBarParams {
            float barWidthPixels  = 60.0f;    //!< バー全体（満タン時）の幅
            float barHeightPixels = 8.0f;     //!< バーの高さ

            // 削れの演出：減った分を白いバーで少し残し、遅れて縮める
            float          trailDelay       = 0.35f;                                   //!< 減ってから削れが縮み始めるまで（秒）
            float          trailShrinkSpeed = 1.5f;                                    //!< 削れが縮む速さ（満タン幅に対する割合/秒）
            float          hitFlashSeconds  = 0.08f;                                   //!< 減った瞬間に残量のバーを白く光らせる長さ（秒）
            hlslpp::float4 trailColor       = hlslpp::float4(1.0f, 1.0f, 1.0f, 0.85f);  //!< 削れのバーの色
        };

        template <class Archive>
        void load(Archive& archive, HealthBarParams& params) {
            LoadField(archive, "barWidthPixels", params.barWidthPixels);
            LoadField(archive, "barHeightPixels", params.barHeightPixels);
            LoadField(archive, "trailDelay", params.trailDelay);
            LoadField(archive, "trailShrinkSpeed", params.trailShrinkSpeed);
            LoadField(archive, "hitFlashSeconds", params.hitFlashSeconds);
            LoadField(archive, "trailColor", params.trailColor);
        }

        //-------------------------------------------------------------
        //! @brief  チューニング値を得る関数（初回の呼び出しで読む。Debugでは保存し直すと読み直す）
        //-------------------------------------------------------------
        const HealthBarParams& GetParams() {
            return GetSystemParams<HealthBarParams>("HealthBar");
        }

        //-------------------------------------------------------------
        //! @brief  左端を固定したまま、満タン幅に対するratioぶんの長さでバーを置く関数
        //! @param  registry [in,out] エンティティレジストリ
        //! @param  bar      [in]     バーのエンティティ（Transform＋WorldAnchor）
        //! @param  params   [in]     チューニング値（バーの幅・高さ）
        //! @param  ratio    [in]     長さの割合（0〜1）
        //! @param  show     [in]     表示するか（falseならscale0で隠す）
        //! @note   中央基準で縮むと右端しか動かないように見えるため、
        //!         WorldAnchorComponent.screenOffsetを左へずらして左端を固定し、右端だけが減っていくようにする
        //-------------------------------------------------------------
        void PlaceBar(Tsukino::ECS::Registry& registry, Tsukino::ECS::Entity bar, const HealthBarParams& params, float ratio, bool show) {
            Tsukino::BuiltIn::ECS::TransformComponent& transform = registry.GetComponent<Tsukino::BuiltIn::ECS::TransformComponent>(bar);
            transform.scale = show ? hlslpp::float3((params.barWidthPixels / kBarTexturePixelSize) * ratio, (params.barHeightPixels / kBarTexturePixelSize), 1.0f)
                                   : hlslpp::float3(0.0f, 0.0f, 0.0f);

            if(auto* anchor = registry.try_get<Tsukino::BuiltIn::ECS::WorldAnchorComponent>(bar))
                anchor->screenOffset.x = -(params.barWidthPixels * (1.0f - ratio) * 0.5f);
        }
    }    // namespace

    //-------------------------------------------------------------
    //! @brief システムの更新
    //-------------------------------------------------------------
    void HealthBarSystem::Update(Tsukino::ECS::Registry& registry, float deltaTime) {
        const HealthBarParams& params = GetParams();

        auto view = registry.View<HealthComponent>();
        view.each([&](entt::entity, HealthComponent& health) {
            if(health.hpBarBackgroundEntity == entt::null || health.hpBarFillEntity == entt::null)
                return;    // HPバーを持たないエンティティ（プレイヤー等）は対象外

            if(!registry.IsValid(health.hpBarBackgroundEntity) || !registry.IsValid(health.hpBarFillEntity))
                return;

            //-------------------------------------------------------------
            // 表示タイマーの更新（被弾時にCombatSystemがhpBarVisibleTimerをセットする）
            //-------------------------------------------------------------
            if(health.hpBarVisibleTimer > 0.0f) {
                health.hpBarVisibleTimer = std::max(0.0f, health.hpBarVisibleTimer - deltaTime);
            }

            bool show = health.hpBarVisibleTimer > 0.0f && !health.isDead;

            float ratio = health.maxHealth > 0.0f ? std::clamp(health.currentHealth / health.maxHealth, 0.0f, 1.0f) : 0.0f;

            //-------------------------------------------------------------
            // 背景バー：常に満タン幅のまま、表示/非表示だけscaleで切り替える
            //-------------------------------------------------------------
            Tsukino::BuiltIn::ECS::TransformComponent& backgroundTransform =
                registry.GetComponent<Tsukino::BuiltIn::ECS::TransformComponent>(health.hpBarBackgroundEntity);
            backgroundTransform.scale = show ? hlslpp::float3((params.barWidthPixels / kBarTexturePixelSize), (params.barHeightPixels / kBarTexturePixelSize), 1.0f) : hlslpp::float3(0.0f, 0.0f, 0.0f);

            //-------------------------------------------------------------
            // 削れ：残量が減ったら、白いバーを前の長さのまましばらく残し、遅れて残量まで縮める。
            // 減った瞬間には残量のバーも一瞬白く光らせる。回復で増えたときは削れをすぐ追いつかせる。
            // 連撃の間は削れを残し続け、手が止まってから縮める（追撃のたびに待ち直す）
            //-------------------------------------------------------------
            if(ratio < health.hpBarLastRatio - 1e-4f) {
                health.hpBarTrailDelay = params.trailDelay;
                health.hpBarFlashTimer = params.hitFlashSeconds;
            }
            health.hpBarLastRatio = ratio;

            if(health.hpBarTrailDelay > 0.0f) {
                health.hpBarTrailDelay = std::max(0.0f, health.hpBarTrailDelay - deltaTime);
            } else {
                health.hpBarTrailRatio = std::max(ratio, health.hpBarTrailRatio - params.trailShrinkSpeed * deltaTime);
            }
            if(ratio > health.hpBarTrailRatio)
                health.hpBarTrailRatio = ratio;

            if(health.hpBarFlashTimer > 0.0f)
                health.hpBarFlashTimer = std::max(0.0f, health.hpBarFlashTimer - deltaTime);

            if(health.hpBarTrailEntity != entt::null && registry.IsValid(health.hpBarTrailEntity)) {
                PlaceBar(registry, health.hpBarTrailEntity, params, health.hpBarTrailRatio, show);
                if(auto* trailSprite = registry.try_get<Tsukino::BuiltIn::ECS::SpriteComponent>(health.hpBarTrailEntity))
                    trailSprite->tintColor = params.trailColor;
            }

            //-------------------------------------------------------------
            // 残量バー：ratio分だけ幅を縮め、残量に応じてtintColorを緑→赤へ補間する（減った瞬間だけ白）
            //-------------------------------------------------------------
            PlaceBar(registry, health.hpBarFillEntity, params, ratio, show);

            if(auto* fillSprite = registry.try_get<Tsukino::BuiltIn::ECS::SpriteComponent>(health.hpBarFillEntity)) {
                fillSprite->tintColor = health.hpBarFlashTimer > 0.0f ? hlslpp::float4(1.0f, 1.0f, 1.0f, 1.0f)
                                                                      : hlslpp::float4(1.0f - ratio, ratio, 0.0f, 1.0f);
            }
        });
    }
}    // namespace CombatAndroid::ECS
