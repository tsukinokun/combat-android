//-------------------------------------------------------------
//! @file   PickupSystem.cpp
//! @brief  PickupSystemクラスの実装
//! @author 山﨑愛
//-------------------------------------------------------------
#include <CombatAndroid/ECS/System/PickupSystem.hpp>
#include <CombatAndroid/ECS/System/SkillSelectSystem.hpp>
#include <CombatAndroid/ECS/Component/PickupComponent.hpp>
#include <CombatAndroid/ECS/Component/PickupPromptComponent.hpp>
#include <CombatAndroid/ECS/Component/PlayerComponent.hpp>
#include <CombatAndroid/ECS/Component/WeaponComponent.hpp>
#include <CombatAndroid/ECS/Component/WeaponAbsorbComponent.hpp>
#include <CombatAndroid/ECS/Utility/WeaponTable.hpp>
#include <CombatAndroid/ECS/Event/GameLogEvent.hpp>

#include <Tsukino/BuiltIn/ECS/Component/TransformComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/FontComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/HighlightComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/ModelComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/WorldAnchorComponent.hpp>

#include <Tsukino/EngineIntegration/EngineContext.hpp>

#include <Tsukino/Core/ECS/Event/EventBus.hpp>
#include <Tsukino/Core/Input/InputSystem.hpp>
#include <Tsukino/Core/Math/MathHelper.hpp>

#include <hlsl++.h>
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <string>
#include <vector>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    namespace {
        constexpr float kHighlightBlendSpeed = 10.0f;    //!< highlightBlendが0↔1へ遷移する速さ（大きいほど素早く切り替わる）
        constexpr float kPulseSpeed          = 3.0f;     //!< 白発光の脈動速度（rad/sec相当）
        constexpr float kRimColorR           = 0.3f;     //!< ネオン風リムカラー（シアン系）
        constexpr float kRimColorG           = 0.9f;
        constexpr float kRimColorB           = 1.0f;
        constexpr float kRimIntensityMax     = 4.0f;     //!< 完全点灯時のリム強度
        constexpr float kRimPower            = 2.5f;     //!< リムの鋭さ
        constexpr float kGlowMin             = 0.05f;    //!< 白発光の脈動の下限
        constexpr float kGlowMax             = 0.35f;    //!< 白発光の脈動の上限

        constexpr float kFloatSpacing = 70.0f;     //!< 浮遊武器を横に並べる間隔（隣同士のx距離）
        constexpr float kFloatHeight  = 170.0f;    //!< 浮遊武器の高さ（既存の初期配置に合わせる）
        constexpr float kFloatDepth   = -20.0f;    //!< 浮遊武器の前後オフセット（既存の初期配置に合わせる）

        // レベルアップの糧になった武器が装備中の同種武器へ吸い寄せられる演出のチューニング値
        // （ExpOrbSystemのホーミング演出と同じ考え方：開始はゆっくり、時間経過で加速する。
        // 「磁石にゆっくり吸い込まれる」感を出すため、開始速度・終端速度とも控えめにしてある）
        constexpr float kAbsorbSpeedStart      = 50.0f;      //!< 吸い寄せ開始時の速度
        constexpr float kAbsorbSpeedEnd        = 400.0f;     //!< 吸い寄せが十分進んだ時点の速度
        constexpr float kAbsorbAccelDuration   = 0.6f;       //!< 開始速度→終端速度まで加速しきるまでの時間
        constexpr float kAbsorbReachDistance   = 25.0f;      //!< 装備武器とこの距離未満まで近づいたら「重なった」と見なす
        constexpr float kAbsorbMaxDuration     = 2.0f;       //!< 万一追いつけない場合の保険（この秒数で強制的に到達扱いにする）
        constexpr float kAbsorbRotationLerpSpeed = 8.0f;     //!< 装備武器の姿勢へ回転補間で近づく速さ（WeaponComponent::attachRotationLerpSpeedと同じ指数減衰の考え方）

        // レベルアップ完了の瞬間に装備武器へ焼くリムライト発光のチューニング値。
        // 拾える武器のシアン系ハイライトと見分けられるよう暖色系（ゴールド）にしている
        constexpr float kLevelUpFlashDuration   = 0.45f;    //!< 発光が続く時間（秒）。この時間でrimIntensity/glowが0まで減衰する
        constexpr float kLevelUpRimColorR       = 1.0f;
        constexpr float kLevelUpRimColorG       = 0.85f;
        constexpr float kLevelUpRimColorB       = 0.35f;
        constexpr float kLevelUpRimIntensityMax = 6.0f;    //!< 発光開始直後のリム強度
        constexpr float kLevelUpGlowMax         = 0.6f;    //!< 発光開始直後の白発光量

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
    //! @brief システムの更新
    //-------------------------------------------------------------
    void PickupSystem::Update(Tsukino::ECS::Registry& registry, float deltaTime) {
        //-------------------------------------------------------------
        // コンテキストの取得
        //-------------------------------------------------------------
        Tsukino::EngineIntegration::EngineContext* ctx = registry.GetContext<Tsukino::EngineIntegration::EngineContext*>();
        if(!ctx || !ctx->inputSystem)
            return;

        //-------------------------------------------------------------
        // スキル選択メニュー中（決定直後の1フレームも含む）はFキーがメニューの
        // 決定入力と衝突するため、拾得を一切処理しない
        // （PlayerSystem等、他の入力Systemと同じ流儀）
        //-------------------------------------------------------------
        if(IsSkillSelectActive(registry))
            return;

        Tsukino::Input::InputSystem* inputSystem = ctx->inputSystem;

        //-------------------------------------------------------------
        // プレイヤーを取得（単一プレイヤー前提）
        //-------------------------------------------------------------
        entt::entity     playerEntity   = entt::null;
        PlayerComponent* player         = nullptr;
        hlslpp::float3   playerPosition = hlslpp::float3(0.0f, 0.0f, 0.0f);

        auto playerView = registry.View<PlayerComponent, Tsukino::BuiltIn::ECS::TransformComponent>();
        for(auto entity : playerView) {
            playerEntity   = entity;
            player         = &playerView.get<PlayerComponent>(entity);
            playerPosition = playerView.get<Tsukino::BuiltIn::ECS::TransformComponent>(entity).position;
            break;
        }

        if(playerEntity == entt::null || !player)
            return;

        //-------------------------------------------------------------
        // レベルアップの糧として吸い寄せられている武器を進める。
        // 装備中の同種武器（target）へ加速しながら直線移動し、重なったら消えて
        // targetのレベルを上げ、リムライト発光を焼く（ExpOrbSystemのHoming演出と同じ考え方）
        //-------------------------------------------------------------
        {
            std::vector<entt::entity> finishedAbsorptions;

            auto absorbView = registry.View<WeaponAbsorbComponent, Tsukino::BuiltIn::ECS::TransformComponent>();
            absorbView.each([&](entt::entity entity, WeaponAbsorbComponent& absorb,
                                Tsukino::BuiltIn::ECS::TransformComponent& transform) {
                absorb.stateTimer += deltaTime;

                if(!registry.IsValid(absorb.target) || !registry.HasComponent<Tsukino::BuiltIn::ECS::TransformComponent>(absorb.target)) {
                    finishedAbsorptions.push_back(entity);    // 吸い寄せ先が消えている等の想定外。安全側でその場で終える
                    return;
                }

                const auto&        targetTransform = registry.GetComponent<Tsukino::BuiltIn::ECS::TransformComponent>(absorb.target);
                hlslpp::float3     targetPosition  = targetTransform.position;
                hlslpp::quaternion targetRotation  = targetTransform.rotation;

                hlslpp::float3 toTarget = targetPosition - transform.position;
                float          distance = hlslpp::length(toTarget);

                // 開始はゆっくり、時間が経つほど吸い込まれる速度が増していく（加速イージング）
                float speedT = SmoothStep01(absorb.stateTimer / kAbsorbAccelDuration);
                float speed  = kAbsorbSpeedStart + (kAbsorbSpeedEnd - kAbsorbSpeedStart) * speedT;

                if(distance > 0.001f) {
                    hlslpp::float3 direction = toTarget / distance;
                    float          moveDist  = std::min(distance, speed * deltaTime);
                    transform.position += direction * moveDist;
                }

                // 姿勢も装備武器の向きへ指数減衰で滑らかに近づける（CombatSystemの武器アタッチと同じ考え方）
                float rotationLerpT = 1.0f - std::exp(-kAbsorbRotationLerpSpeed * deltaTime);
                transform.rotation  = Tsukino::Core::Math::SlerpShortestPath(transform.rotation, targetRotation, rotationLerpT);
                transform.dirty     = true;

                bool reached  = distance <= kAbsorbReachDistance;
                bool timedOut = absorb.stateTimer >= kAbsorbMaxDuration;
                if(reached || timedOut)
                    finishedAbsorptions.push_back(entity);
            });

            for(entt::entity entity : finishedAbsorptions) {
                if(auto* model = registry.try_get<Tsukino::BuiltIn::ECS::ModelComponent>(entity)) {
                    model->visible = false;
                }

                entt::entity target = registry.GetComponent<WeaponAbsorbComponent>(entity).target;
                if(registry.IsValid(target) && registry.HasComponent<WeaponComponent>(target)) {
                    WeaponComponent& targetWeapon = registry.GetComponent<WeaponComponent>(target);
                    if(targetWeapon.level < kMaxWeaponLevel) {
                        ++targetWeapon.level;
                        RecalculateWeaponStats(targetWeapon);

                        // 画面右の取得ログへ流す。カンストしている場合は何も起きていないので出さない
                        if(auto* eventBus = registry.GetContext<Tsukino::ECS::EventBus*>()) {
                            eventBus->Publish(GameLogEvent{GameLogCategory::WeaponLevelUp,
                                                          std::wstring(GetWeaponEntry(targetWeapon.weaponId).displayName) + L" Lv."
                                                              + std::to_wstring(targetWeapon.level)});
                        }
                    }
                    targetWeapon.levelUpFlashTimer = kLevelUpFlashDuration;
                }

                registry.RemoveComponent<WeaponAbsorbComponent>(entity);
            }
        }

        //-------------------------------------------------------------
        // レベルアップ発光の減衰。levelUpFlashTimerが立っている武器だけを対象に、
        // イーズアウトさせながらHighlightComponentへ発光値を書き込む
        //-------------------------------------------------------------
        {
            auto flashView = registry.View<WeaponComponent, Tsukino::BuiltIn::ECS::HighlightComponent>();
            flashView.each([&](entt::entity, WeaponComponent& weapon, Tsukino::BuiltIn::ECS::HighlightComponent& highlight) {
                if(weapon.levelUpFlashTimer <= 0.0f)
                    return;

                weapon.levelUpFlashTimer -= deltaTime;
                if(weapon.levelUpFlashTimer <= 0.0f) {
                    weapon.levelUpFlashTimer = 0.0f;
                    highlight.active           = false;
                    return;
                }

                float ease = SmoothStep01(weapon.levelUpFlashTimer / kLevelUpFlashDuration);
                highlight.active       = true;
                highlight.rimColor     = hlslpp::float3(kLevelUpRimColorR, kLevelUpRimColorG, kLevelUpRimColorB);
                highlight.rimIntensity = kLevelUpRimIntensityMax * ease;
                highlight.rimPower     = kRimPower;
                highlight.glow         = kLevelUpGlowMax * ease;
            });
        }

        //-------------------------------------------------------------
        // 浮遊武器同士が同じ位置に重ならないよう、インベントリ内の並び順(index)と
        // 総数(count)から横方向の位置を割り出して毎フレーム配置し直す。
        // 個数が変わった瞬間もCombatSystem側の指数追従（attachPositionLerpSpeed）で
        // 新しい位置へ滑らかに移動するため、ここで値を書き換えるだけでよい
        //-------------------------------------------------------------
        {
            int weaponCount = static_cast<int>(player->weaponInventory.size());
            for(int i = 0; i < weaponCount; ++i) {
                entt::entity weaponEntity = player->weaponInventory[i];
                if(!registry.HasComponent<WeaponComponent>(weaponEntity))
                    continue;

                // count等分した位置に中央揃えで並べる（例: 2本なら-35, +35）
                float             offsetX = (static_cast<float>(i) - (weaponCount - 1) * 0.5f) * kFloatSpacing;
                WeaponComponent& weapon   = registry.GetComponent<WeaponComponent>(weaponEntity);
                weapon.localOffset        = hlslpp::float3(offsetX, kFloatHeight, kFloatDepth);
            }
        }

        //-------------------------------------------------------------
        // 拾える対象のうち、プレイヤーに最も近い1つだけを選ぶ
        // （範囲内に複数あっても常に1つだけを拾える対象にするための絞り込み）
        //-------------------------------------------------------------
        entt::entity nearest         = entt::null;
        float        nearestDistance = FLT_MAX;

        auto pickupView = registry.View<PickupComponent, Tsukino::BuiltIn::ECS::TransformComponent>();
        pickupView.each([&](entt::entity entity, PickupComponent& pickup, Tsukino::BuiltIn::ECS::TransformComponent& transform) {
            hlslpp::float3 toItem = transform.position - playerPosition;
            toItem.y              = 0.0f;    // 高さのずれで拾えなくならないよう水平距離のみで判定する

            float distance = hlslpp::length(toItem);
            if(distance <= pickup.radius && distance < nearestDistance) {
                nearestDistance = distance;
                nearest         = entity;
            }
        });
        player->pickupTarget = nearest;

        //-------------------------------------------------------------
        // 全ての拾えるアイテムのハイライト演出を更新する。
        // 対象になっているものだけ0→1へ、それ以外は1→0へ滑らかに戻す
        //-------------------------------------------------------------
        pickupView.each([&](entt::entity entity, PickupComponent& pickup, Tsukino::BuiltIn::ECS::TransformComponent& transform) {
            float target = (entity == nearest) ? 1.0f : 0.0f;
            float t      = 1.0f - std::exp(-kHighlightBlendSpeed * deltaTime);
            pickup.highlightBlend += (target - pickup.highlightBlend) * t;
            pickup.pulseTime += deltaTime;

            // 0→1→0を往復する脈動。sinを2乗して滑らかな山にする
            float wave  = std::sin(pickup.pulseTime * kPulseSpeed);
            float pulse = wave * wave;

            if(auto* highlight = registry.try_get<Tsukino::BuiltIn::ECS::HighlightComponent>(entity)) {
                highlight->active       = pickup.highlightBlend > 0.001f;
                highlight->rimColor     = hlslpp::float3(kRimColorR, kRimColorG, kRimColorB);
                highlight->rimIntensity = kRimIntensityMax * pickup.highlightBlend;
                highlight->rimPower     = kRimPower;
                highlight->glow         = (kGlowMin + (kGlowMax - kGlowMin) * pulse) * pickup.highlightBlend;
            }
        });

        //-------------------------------------------------------------
        // 「Fキーで拾う」UIラベルの更新（対象がいるときだけ表示する）。
        // 座標計算そのものはWorldAnchorSystemに任せ、ここではtarget/worldOffsetの
        // 設定とテキストの更新だけを行う
        //-------------------------------------------------------------
        auto promptView =
            registry.View<PickupPromptComponent, Tsukino::BuiltIn::ECS::FontComponent, Tsukino::BuiltIn::ECS::WorldAnchorComponent>();
        promptView.each([&](entt::entity, PickupPromptComponent& prompt, Tsukino::BuiltIn::ECS::FontComponent& promptFont,
                            Tsukino::BuiltIn::ECS::WorldAnchorComponent& promptAnchor) {
            if(nearest == entt::null || !registry.HasComponent<PickupComponent>(nearest)) {
                promptAnchor.target = entt::null;
                promptFont.text.clear();    // 空文字ならFontRendererSystemが描画をスキップする
                return;
            }

            const auto& pickup = registry.GetComponent<PickupComponent>(nearest);

            promptAnchor.target       = nearest;
            promptAnchor.worldOffset  = hlslpp::float3(0.0f, pickup.labelHeight, 0.0f);
            promptAnchor.screenOffset = hlslpp::float2(0.0f, prompt.screenOffsetY);
            promptFont.text           = L"F : " + pickup.displayName + L" を拾う";
        });

        //-------------------------------------------------------------
        // Fキーで取得する。反復中にコンポーネント構成を変えるとViewが壊れるため、
        // 上の絞り込み・演出更新が終わった後にここでまとめて行う
        //-------------------------------------------------------------
        if(nearest != entt::null && inputSystem->IsKeyPressed(Tsukino::Input::KeyCode::F)) {
            if(registry.HasComponent<WeaponComponent>(nearest)) {
                WeaponComponent& pickedWeapon = registry.GetComponent<WeaponComponent>(nearest);

                // 既に同じ種類の武器を持っていないか、インベントリ内を探す
                entt::entity existingWeaponEntity = entt::null;
                for(entt::entity ownedEntity : player->weaponInventory) {
                    if(!registry.HasComponent<WeaponComponent>(ownedEntity))
                        continue;
                    if(registry.GetComponent<WeaponComponent>(ownedEntity).weaponId == pickedWeapon.weaponId) {
                        existingWeaponEntity = ownedEntity;
                        break;
                    }
                }

                if(existingWeaponEntity != entt::null) {
                    // 2本目以降は新規枠を増やさず、装備中の個体へ吸い寄せてレベルアップさせる。
                    // レベル加算・リムライト発光は吸い寄せが完了した瞬間（Update冒頭の吸収処理）で行う。
                    // Scene::DestroyEntity()を経由しないSystemからの直接破棄は前例が無く、
                    // EffectSystem/PhysicsSystem側にScene経由の破棄を前提にした注意書きがあるため、
                    // 吸収完了時も非表示化のみ行い、以後owner=entt::nullのまま放置する
                    // （weaponInventoryに入らないためCombatSystem/PlayerSystemからは触られない）
                    registry.AddComponent<WeaponAbsorbComponent>(nearest).target = existingWeaponEntity;
                } else {
                    // 拾った武器は「所有者つきの浮遊武器」へ昇格させる
                    pickedWeapon.owner        = playerEntity;
                    pickedWeapon.floatEnabled = true;

                    player->weaponInventory.push_back(nearest);

                    // 画面右の取得ログへ流す（初取得のときだけ。2本目以降は上の吸収側が出す）
                    if(auto* eventBus = registry.GetContext<Tsukino::ECS::EventBus*>()) {
                        eventBus->Publish(
                            GameLogEvent{GameLogCategory::WeaponAcquired, GetWeaponEntry(pickedWeapon.weaponId).displayName});
                    }
                }
            }

            // 演出とワールド判定を止める（PickupComponentを外すのでこれ以降候補に上がらない）
            if(auto* highlight = registry.try_get<Tsukino::BuiltIn::ECS::HighlightComponent>(nearest)) {
                highlight->active = false;
            }
            registry.RemoveComponent<PickupComponent>(nearest);
            player->pickupTarget = entt::null;
        }
    }
}    // namespace CombatAndroid::ECS
