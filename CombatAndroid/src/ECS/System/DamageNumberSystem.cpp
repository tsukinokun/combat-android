//-------------------------------------------------------------
//! @file   DamageNumberSystem.cpp
//! @brief  DamageNumberSystemクラスの実装
//! @note   時間はCombatAndroidScene::OnUpdateがヒットストップ用にスケールした
//!         deltaTimeをそのまま受け取る。そのため被弾直後の数フレームは数値が
//!         小さいままほぼ静止し、ヒットストップ明けに動き出す（狙いどおりの挙動）
//-------------------------------------------------------------
#include <CombatAndroid/ECS/System/DamageNumberSystem.hpp>
#include <CombatAndroid/ECS/Component/DamageNumberComponent.hpp>

#include <Tsukino/BuiltIn/ECS/Component/TransformComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/WorldAnchorComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/FontComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/CollisionComponent.hpp>

#include <hlsl++.h>
#include <entt/entt.hpp>

#include <algorithm>
#include <string>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    namespace {
        constexpr float kLifetime     = 0.75f;    //!< 表示開始から消えるまでの時間（秒）
        constexpr float kRiseDistance = 70.0f;    //!< 寿命いっぱいで上昇するピクセル数
        constexpr float kFadeStart    = 0.55f;    //!< 進行度がこの値を超えてからフェードを始める

        // ポップ演出。0.45倍から1.25倍へ跳ね上げ、そこから等倍へ落ち着かせる
        constexpr float kPopStartScale = 0.45f;
        constexpr float kPopOvershoot  = 1.25f;
        constexpr float kPopInEnd      = 0.14f;    //!< 進行度がここまでで最大まで拡大する
        constexpr float kSettleEnd     = 0.30f;    //!< 進行度がここまでで等倍へ収束する

        constexpr float kOutlineWidth = 2.0f;    //!< 縁取りの太さ（ピクセル）

        // ヒット位置（敵の足元原点）からどれだけ上に出すか。
        // 通常は敵のカプセル中心（CollisionComponent::offsetPosition.y）を使い、
        // 取得できなかったときだけこの値へフォールバックする
        constexpr float kDefaultSpawnHeight = 120.0f;

        //!< 数値の色（温かい黄）と縁取りの色
        const hlslpp::float4 kDamageColor  = hlslpp::float4(1.0f, 0.92f, 0.35f, 1.0f);
        const hlslpp::float4 kOutlineColor = hlslpp::float4(0.0f, 0.0f, 0.0f, 1.0f);

        //-------------------------------------------------------------
        //! @brief 0から1を滑らかに補間する関数（smoothstepの本体部分）
        //-------------------------------------------------------------
        [[nodiscard]]
        float SmoothStep01(float t) {
            return t * t * (3.0f - 2.0f * t);
        }

        //-------------------------------------------------------------
        //! @brief 進行度からポップの拡大率を求める関数
        //! @param t [in] 0から1の進行度（elapsed / lifetime）
        //-------------------------------------------------------------
        [[nodiscard]]
        float EvaluatePopScale(float t) {
            if(t < kPopInEnd) {
                float u = SmoothStep01(t / kPopInEnd);
                return kPopStartScale + (kPopOvershoot - kPopStartScale) * u;
            }

            if(t < kSettleEnd) {
                float u = SmoothStep01((t - kPopInEnd) / (kSettleEnd - kPopInEnd));
                return kPopOvershoot + (1.0f - kPopOvershoot) * u;
            }

            return 1.0f;
        }
    }    // namespace

    //-------------------------------------------------------------
    //! @brief WeaponHitEventの購読を開始する
    //-------------------------------------------------------------
    void DamageNumberSystem::Initialize(Tsukino::ECS::EventBus& eventBus) {
        // ハンドラ内でヒープを触らないよう、あらかじめ最大数分を確保しておく
        m_pending.reserve(kDamageNumberPoolSize);

        m_hitConnection = eventBus.Subscribe<WeaponHitEvent>([this](const WeaponHitEvent& event) { OnWeaponHit(event); });
    }

    //-------------------------------------------------------------
    //! @brief ヒット通知のハンドラ
    //-------------------------------------------------------------
    void DamageNumberSystem::OnWeaponHit(const WeaponHitEvent& event) {
        m_pending.push_back(PendingDamageNumber{event.target, event.hitPosition, event.damage});
    }

    //-------------------------------------------------------------
    //! @brief システムの更新
    //-------------------------------------------------------------
    void DamageNumberSystem::Update(Tsukino::ECS::Registry& registry, float deltaTime) {
        auto view = registry.View<DamageNumberComponent,
                                  Tsukino::BuiltIn::ECS::TransformComponent,
                                  Tsukino::BuiltIn::ECS::WorldAnchorComponent,
                                  Tsukino::BuiltIn::ECS::FontComponent>();

        //-------------------------------------------------------------
        // 保留中のヒットをスロットへ割り当てる
        //-------------------------------------------------------------
        for(const PendingDamageNumber& pending : m_pending) {
            //-------------------------------------------------------------
            // 空きスロットを探す。全て使用中なら最も古いもの（elapsedが最大）を再利用する
            //-------------------------------------------------------------
            entt::entity slotEntity = entt::null;
            float        oldest     = -1.0f;

            for(entt::entity entity : view) {
                const DamageNumberComponent& damageNumber = view.get<DamageNumberComponent>(entity);

                if(!damageNumber.active) {
                    slotEntity = entity;
                    break;
                }

                if(damageNumber.elapsed > oldest) {
                    oldest     = damageNumber.elapsed;
                    slotEntity = entity;
                }
            }

            if(slotEntity == entt::null)
                continue;    // プールが1つも用意されていない（シーン側の生成漏れ）

            //-------------------------------------------------------------
            // 表示する高さを決める。WeaponHitEvent::hitPositionは敵の足元原点なので、
            // そのままでは地面に数値が出てしまう。敵のカプセル中心まで持ち上げる
            //-------------------------------------------------------------
            float spawnHeight = kDefaultSpawnHeight;
            if(registry.IsValid(pending.target)) {
                if(const auto* collision = registry.try_get<Tsukino::BuiltIn::ECS::CollisionComponent>(pending.target))
                    spawnHeight = collision->offsetPosition.y;
            }

            DamageNumberComponent&                       damageNumber = view.get<DamageNumberComponent>(slotEntity);
            Tsukino::BuiltIn::ECS::TransformComponent&   transform    = view.get<Tsukino::BuiltIn::ECS::TransformComponent>(slotEntity);
            Tsukino::BuiltIn::ECS::WorldAnchorComponent& anchor       = view.get<Tsukino::BuiltIn::ECS::WorldAnchorComponent>(slotEntity);
            Tsukino::BuiltIn::ECS::FontComponent&        font         = view.get<Tsukino::BuiltIn::ECS::FontComponent>(slotEntity);

            damageNumber.active    = true;
            damageNumber.elapsed   = 0.0f;
            damageNumber.lifetime  = kLifetime;
            damageNumber.baseColor = kDamageColor;

            font.text            = std::to_wstring(static_cast<int>(pending.damage + 0.5f));
            font.color           = kDamageColor;
            font.outlineColor    = kOutlineColor;
            font.outlineWidth    = kOutlineWidth;
            font.horizontalAlign = Tsukino::BuiltIn::ECS::HorizontalAlign::Center;
            font.verticalAlign   = Tsukino::BuiltIn::ECS::VerticalAlign::Middle;

            //-------------------------------------------------------------
            // 敵を追従させず、斬った瞬間のワールド座標へ貼り付ける。
            // こうしておくと、とどめを刺して敵が破棄されても数値だけは正しく残る
            //-------------------------------------------------------------
            anchor.target                = entt::null;
            anchor.useFixedWorldPosition = true;
            anchor.fixedWorldPosition    = pending.hitPosition + hlslpp::float3(0.0f, spawnHeight, 0.0f);
            anchor.worldOffset           = hlslpp::float3(0.0f, 0.0f, 0.0f);
            anchor.screenOffset          = hlslpp::float2(0.0f, 0.0f);

            transform.scale = hlslpp::float3(kPopStartScale, kPopStartScale, 1.0f);
            transform.dirty = true;
        }

        m_pending.clear();

        //-------------------------------------------------------------
        // 表示中のスロットを進める
        //-------------------------------------------------------------
        view.each([&](entt::entity,
                      DamageNumberComponent&                       damageNumber,
                      Tsukino::BuiltIn::ECS::TransformComponent&   transform,
                      Tsukino::BuiltIn::ECS::WorldAnchorComponent& anchor,
                      Tsukino::BuiltIn::ECS::FontComponent&        font) {
            if(!damageNumber.active)
                return;

            damageNumber.elapsed += deltaTime;

            //-------------------------------------------------------------
            // 寿命切れ。空文字にしておけばFontRendererSystemが描画をスキップする
            //-------------------------------------------------------------
            if(damageNumber.elapsed >= damageNumber.lifetime) {
                damageNumber.active = false;
                font.text.clear();
                anchor.useFixedWorldPosition = false;
                transform.scale              = hlslpp::float3(0.0f, 0.0f, 0.0f);
                transform.dirty              = true;
                return;
            }

            float t = damageNumber.elapsed / damageNumber.lifetime;

            //-------------------------------------------------------------
            // 拡大率。FontRendererSystemはworldMatrixのX軸長（＝scale.x）を
            // フォントの拡大率として読むため、scaleへ書けばそのまま文字サイズになる
            //-------------------------------------------------------------
            float popScale  = EvaluatePopScale(t);
            transform.scale = hlslpp::float3(popScale, popScale, 1.0f);
            transform.dirty = true;

            //-------------------------------------------------------------
            // 上昇。スクリーン座標は下方向が正（WorldAnchorSystemの投影式）なので、
            // 上へ動かすにはscreenOffset.yを負にする。減速させるためease-outにする
            //-------------------------------------------------------------
            float riseCurve     = 1.0f - (1.0f - t) * (1.0f - t);
            anchor.screenOffset = hlslpp::float2(0.0f, -kRiseDistance * riseCurve);

            //-------------------------------------------------------------
            // フェード。縁取りだけ残らないよう本体と同じアルファを掛ける
            //-------------------------------------------------------------
            float alpha = (t < kFadeStart) ? 1.0f : (1.0f - t) / (1.0f - kFadeStart);
            alpha       = std::clamp(alpha, 0.0f, 1.0f);

            font.color = hlslpp::float4(
                damageNumber.baseColor.x, damageNumber.baseColor.y, damageNumber.baseColor.z, damageNumber.baseColor.w * alpha);
            font.outlineColor = hlslpp::float4(kOutlineColor.x, kOutlineColor.y, kOutlineColor.z, kOutlineColor.w * alpha);
        });
    }
}    // namespace CombatAndroid::ECS
