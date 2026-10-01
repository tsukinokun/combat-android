//-------------------------------------------------------------
//! @file   PickupSystem.cpp
//! @brief  PickupSystemクラスの実装
//! @author 山﨑愛
//-------------------------------------------------------------
#include <CombatAndroid/ECS/System/PickupSystem.hpp>
#include <CombatAndroid/ECS/Utility/GameplayFreeze.hpp>
#include <CombatAndroid/ECS/Component/PickupComponent.hpp>
#include <CombatAndroid/ECS/Component/PlayerComponent.hpp>
#include <CombatAndroid/ECS/Component/WeaponComponent.hpp>
#include <CombatAndroid/ECS/Component/WeaponAbsorbComponent.hpp>
#include <CombatAndroid/ECS/Component/WeaponDropFallComponent.hpp>
#include <CombatAndroid/ECS/Utility/WeaponTable.hpp>
#include <CombatAndroid/ECS/Utility/WeaponEvolutionTable.hpp>
#include <CombatAndroid/ECS/Event/GameLogEvent.hpp>
#include <CombatAndroid/ECS/Serialization/SerializationHelper.hpp>
#include <CombatAndroid/ECS/Utility/TableJson.hpp>

#include <Tsukino/BuiltIn/ECS/Component/TransformComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/FontComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/RimGlowComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/ModelComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/WorldAnchorComponent.hpp>

#include <Tsukino/EngineIntegration/EngineContext.hpp>

#include <Tsukino/Core/ECS/Event/EventBus.hpp>
#include <Tsukino/Core/Input/InputSystem.hpp>
#include <Tsukino/Core/Math/MathHelper.hpp>
#include <Tsukino/Core/Math/Serialization/HlslppSerialization.hpp>

#include <hlsl++.h>
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <string>
#include <vector>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    namespace {
        //-------------------------------------------------------------
        //! @struct PickupParams
        //! @brief  演出のチューニング値（Assets/Tables/Systems/Pickup.json。ここの初期値はJSONにキーが無いときの既定値）
        //-------------------------------------------------------------
        struct PickupParams {
            // 拾える武器のリムグロー。遠くでも弱く光らせ、近づくほど強くする
            float          rimGlowBlendSpeed = 10.0f;                                //!< rimGlowBlendが近さの値へ追従する速さ（大きいほど素早く切り替わる）
            float          pulseSpeed        = 3.0f;                                 //!< 白発光の脈動速度（rad/sec相当）
            hlslpp::float3 rimColor          = hlslpp::float3(0.3f, 0.9f, 1.0f);    //!< ネオン風リムカラー（シアン系）
            float          rimIntensityMax   = 4.0f;                                 //!< 近くにいるときのリム強度
            float          rimPower          = 2.5f;                                 //!< リムの鋭さ
            float          glowMin           = 0.05f;                                //!< 近くにいるときの白発光の脈動の下限
            float          glowMax           = 0.35f;                                //!< 近くにいるときの白発光の脈動の上限
            float          idleRimIntensity  = 1.5f;                                 //!< 遠くにあるときのリム強度（草むらの中でも目に留まる程度）
            float          idleGlow          = 0.08f;                                //!< 遠くにあるときの白発光の脈動の上限
            float          glowNearDistance  = 400.0f;                               //!< この距離から近づくほど、遠くの発光→近くの発光へ強まる

            // 落ちている武器を草の上に浮かせる演出（草の丈は22〜46あり、横倒しのままだと埋もれる）
            float groundFloatHeight  = 50.0f;    //!< 地面から浮かせる高さ（武器の原点＝握りの位置）
            float groundBobAmplitude = 8.0f;     //!< 上下に漂う振れ幅
            float groundBobSpeed     = 2.0f;     //!< 上下に漂う角速度（rad/sec）
            float groundSpinSpeed    = 1.5f;     //!< 縦軸で回る速さ（rad/sec）
            float groundRiseDuration = 0.5f;     //!< 着地した横倒しの姿勢から、浮かぶ姿勢へ起き上がるまでの秒数

            float floatSpacing = 70.0f;     //!< 浮遊武器を横に並べる間隔（隣同士のx距離）
            float floatHeight  = 170.0f;    //!< 浮遊武器の高さ（既存の初期配置に合わせる）
            float floatDepth   = -20.0f;    //!< 浮遊武器の前後オフセット（既存の初期配置に合わせる）

            // レベルアップの糧になった武器が装備中の同種武器へ吸い寄せられる演出
            // （ExpOrbSystemのホーミング演出と同じ考え方：開始はゆっくり、時間経過で加速する。
            // 「磁石にゆっくり吸い込まれる」感を出すため、開始速度・終端速度とも控えめにしてある）
            float absorbSpeedStart         = 50.0f;     //!< 吸い寄せ開始時の速度
            float absorbSpeedEnd           = 400.0f;    //!< 吸い寄せが十分進んだ時点の速度
            float absorbAccelDuration      = 0.6f;      //!< 開始速度→終端速度まで加速しきるまでの時間
            float absorbReachDistance      = 25.0f;     //!< 装備武器とこの距離未満まで近づいたら「重なった」と見なす
            float absorbMaxDuration        = 2.0f;      //!< 万一追いつけない場合の保険（この秒数で強制的に到達扱いにする）
            float absorbRotationLerpSpeed  = 8.0f;      //!< 装備武器の姿勢へ回転補間で近づく速さ（WeaponComponent::attachRotationLerpSpeedと同じ指数減衰の考え方）

            // レベルアップ完了の瞬間に装備武器へ焼くリムグロー。
            // 拾える武器のシアン系リムグローと見分けられるよう暖色系（ゴールド）にしている
            float          levelUpFlashDuration   = 0.45f;                                //!< 発光が続く時間（秒）。この時間でrimIntensity/glowが0まで減衰する
            hlslpp::float3 levelUpRimColor        = hlslpp::float3(1.0f, 0.85f, 0.35f);
            float          levelUpRimIntensityMax = 6.0f;                                 //!< 発光開始直後のリム強度
            float          levelUpGlowMax         = 0.6f;                                 //!< 発光開始直後の白発光量

            // 進化済みの武器に常に残す弱い発光。レベルアップ発光と同じ金色で、
            // 戦闘中に目障りにならないよう面全体（glow）はほとんど持ち上げない
            float evolvedRimIntensity = 1.8f;
            float evolvedGlow         = 0.04f;
        };

        template <class Archive>
        void load(Archive& archive, PickupParams& params) {
            LoadField(archive, "rimGlowBlendSpeed", params.rimGlowBlendSpeed);
            LoadField(archive, "pulseSpeed", params.pulseSpeed);
            LoadField(archive, "rimColor", params.rimColor);
            LoadField(archive, "rimIntensityMax", params.rimIntensityMax);
            LoadField(archive, "rimPower", params.rimPower);
            LoadField(archive, "glowMin", params.glowMin);
            LoadField(archive, "glowMax", params.glowMax);
            LoadField(archive, "idleRimIntensity", params.idleRimIntensity);
            LoadField(archive, "idleGlow", params.idleGlow);
            LoadField(archive, "glowNearDistance", params.glowNearDistance);
            LoadField(archive, "groundFloatHeight", params.groundFloatHeight);
            LoadField(archive, "groundBobAmplitude", params.groundBobAmplitude);
            LoadField(archive, "groundBobSpeed", params.groundBobSpeed);
            LoadField(archive, "groundSpinSpeed", params.groundSpinSpeed);
            LoadField(archive, "groundRiseDuration", params.groundRiseDuration);
            LoadField(archive, "floatSpacing", params.floatSpacing);
            LoadField(archive, "floatHeight", params.floatHeight);
            LoadField(archive, "floatDepth", params.floatDepth);
            LoadField(archive, "absorbSpeedStart", params.absorbSpeedStart);
            LoadField(archive, "absorbSpeedEnd", params.absorbSpeedEnd);
            LoadField(archive, "absorbAccelDuration", params.absorbAccelDuration);
            LoadField(archive, "absorbReachDistance", params.absorbReachDistance);
            LoadField(archive, "absorbMaxDuration", params.absorbMaxDuration);
            LoadField(archive, "absorbRotationLerpSpeed", params.absorbRotationLerpSpeed);
            LoadField(archive, "levelUpFlashDuration", params.levelUpFlashDuration);
            LoadField(archive, "levelUpRimColor", params.levelUpRimColor);
            LoadField(archive, "levelUpRimIntensityMax", params.levelUpRimIntensityMax);
            LoadField(archive, "levelUpGlowMax", params.levelUpGlowMax);
            LoadField(archive, "evolvedRimIntensity", params.evolvedRimIntensity);
            LoadField(archive, "evolvedGlow", params.evolvedGlow);
        }

        //-------------------------------------------------------------
        //! @brief  チューニング値を得る関数（初回の呼び出しで1度だけ読む）
        //-------------------------------------------------------------
        const PickupParams& GetParams() {
            static const PickupParams s_params = LoadSystemParams<PickupParams>("Pickup");
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
    //! @brief システムの更新
    //-------------------------------------------------------------
    void PickupSystem::Update(Tsukino::ECS::Registry& registry, float deltaTime) {
        const PickupParams& params = GetParams();

        //-------------------------------------------------------------
        // コンテキストの取得
        //-------------------------------------------------------------
        Tsukino::EngineIntegration::EngineContext* ctx = registry.GetContext<Tsukino::EngineIntegration::EngineContext*>();
        if(!ctx)
            return;

        //-------------------------------------------------------------
        // メニュー（スキル選択・ポーズ・リザルト）中（決定直後の1フレームも含む）は世界が止まっているので、
        // 拾得も演出も進めない（PlayerSystem等、他のゲームプレイSystemと同じ流儀）
        //-------------------------------------------------------------
        if(IsGameplayFrozen(registry))
            return;

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
        // targetのレベルを上げ、リムグローを焼く（ExpOrbSystemのHoming演出と同じ考え方）
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
                float speedT = SmoothStep01(absorb.stateTimer / params.absorbAccelDuration);
                float speed  = params.absorbSpeedStart + (params.absorbSpeedEnd - params.absorbSpeedStart) * speedT;

                if(distance > 0.001f) {
                    hlslpp::float3 direction = toTarget / distance;
                    float          moveDist  = std::min(distance, speed * deltaTime);
                    transform.position += direction * moveDist;
                }

                // 姿勢も装備武器の向きへ指数減衰で滑らかに近づける（CombatSystemの武器アタッチと同じ考え方）
                float rotationLerpT = 1.0f - std::exp(-params.absorbRotationLerpSpeed * deltaTime);
                transform.rotation  = Tsukino::Core::Math::SlerpShortestPath(transform.rotation, targetRotation, rotationLerpT);
                transform.dirty     = true;

                bool reached  = distance <= params.absorbReachDistance;
                bool timedOut = absorb.stateTimer >= params.absorbMaxDuration;
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
                    targetWeapon.levelUpFlashTimer = params.levelUpFlashDuration;

                    // 最大レベルに届いた武器が進化条件を満たしていれば進化させる
                    // （進化した武器はここで発光を長いものに焼き直す）
                    TryEvolvePlayerWeapons(registry, playerEntity);
                }

                registry.RemoveComponent<WeaponAbsorbComponent>(entity);
            }
        }

        //-------------------------------------------------------------
        // レベルアップ発光の減衰。levelUpFlashTimerが立っている武器だけを対象に、
        // イーズアウトさせながらRimGlowComponentへ発光値を書き込む
        //-------------------------------------------------------------
        {
            auto flashView = registry.View<WeaponComponent, Tsukino::BuiltIn::ECS::RimGlowComponent>();
            flashView.each([&](entt::entity, WeaponComponent& weapon, Tsukino::BuiltIn::ECS::RimGlowComponent& rimGlow) {
                // 手持ちの進化済み武器は、発光が減衰し切った後もこの弱さで光らせ続ける
                const bool  keepsEvolvedGlow = weapon.evolved && weapon.owner != entt::null;
                const float restRim          = keepsEvolvedGlow ? params.evolvedRimIntensity : 0.0f;
                const float restGlow         = keepsEvolvedGlow ? params.evolvedGlow : 0.0f;

                if(weapon.levelUpFlashTimer <= 0.0f) {
                    if(keepsEvolvedGlow) {
                        rimGlow.active       = true;
                        rimGlow.rimColor     = params.levelUpRimColor;
                        rimGlow.rimIntensity = restRim;
                        rimGlow.rimPower     = params.rimPower;
                        rimGlow.glow         = restGlow;
                    }
                    return;
                }

                weapon.levelUpFlashTimer -= deltaTime;
                if(weapon.levelUpFlashTimer <= 0.0f) {
                    weapon.levelUpFlashTimer = 0.0f;
                    rimGlow.active       = keepsEvolvedGlow;
                    rimGlow.rimIntensity = restRim;
                    rimGlow.glow         = restGlow;
                    return;
                }

                // 減衰の行き先は消灯ではなく常時発光の強さ（進化済みでなければ0）
                float ease = SmoothStep01(weapon.levelUpFlashTimer / params.levelUpFlashDuration);
                rimGlow.active       = true;
                rimGlow.rimColor     = params.levelUpRimColor;
                rimGlow.rimIntensity = restRim + (params.levelUpRimIntensityMax - restRim) * ease;
                rimGlow.rimPower     = params.rimPower;
                rimGlow.glow         = restGlow + (params.levelUpGlowMax - restGlow) * ease;
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
                float             offsetX = (static_cast<float>(i) - (weaponCount - 1) * 0.5f) * params.floatSpacing;
                WeaponComponent& weapon   = registry.GetComponent<WeaponComponent>(weaponEntity);
                weapon.localOffset        = hlslpp::float3(offsetX, params.floatHeight, params.floatDepth);
            }
        }

        //-------------------------------------------------------------
        // 落ちている武器の演出と、拾う対象の絞り込みを1回の反復で行う。
        // 落下中（WeaponDropFallComponent）はEnemyWeaponDropSystemが動かしているので触らない
        //-------------------------------------------------------------
        entt::entity nearest         = entt::null;
        float        nearestDistance = FLT_MAX;

        auto pickupView = registry.View<PickupComponent, Tsukino::BuiltIn::ECS::TransformComponent>();
        pickupView.each([&](entt::entity entity, PickupComponent& pickup, Tsukino::BuiltIn::ECS::TransformComponent& transform) {
            if(registry.HasComponent<WeaponDropFallComponent>(entity))
                return;

            pickup.pulseTime += deltaTime;

            //-------------------------------------------------
            // 草の上に浮かせる。草の丈（22〜46）より高く、縦向きにして回しながら上下させる。
            // 初めて扱うフレームの位置・姿勢を「地面に横たわった状態」として覚え、そこから
            // groundRiseDurationかけて起き上がらせる（落ちて→横たわって→ふわっと浮く）
            //-------------------------------------------------
            if(!pickup.hasRest) {
                pickup.restPosition = transform.position;
                pickup.restRotation = transform.rotation;
                pickup.hasRest      = true;
            }

            pickup.riseTimer += deltaTime;
            pickup.spinAngle = std::fmod(pickup.spinAngle + params.groundSpinSpeed * deltaTime, 6.28318531f);

            hlslpp::float3 floatPosition = pickup.restPosition;
            floatPosition.y += params.groundFloatHeight + std::sin(pickup.pulseTime * params.groundBobSpeed) * params.groundBobAmplitude;

            // 武器のモデルは刃が+Y（手持ちの浮遊武器と同じ前提）なので、縦軸で回すだけで縦向きになる
            const hlslpp::quaternion floatRotation = hlslpp::quaternion::rotation_y(pickup.spinAngle);

            const float rise   = params.groundRiseDuration > 0.0f ? SmoothStep01(pickup.riseTimer / params.groundRiseDuration) : 1.0f;
            transform.position = hlslpp::lerp(pickup.restPosition, floatPosition, rise);
            transform.rotation = Tsukino::Core::Math::SlerpShortestPath(pickup.restRotation, floatRotation, rise);
            transform.dirty    = true;

            //-------------------------------------------------
            // 拾う対象の絞り込み。高さのずれで拾えなくならないよう水平距離で判定し、
            // 範囲内に複数あっても1フレームに拾うのは一番近い1本だけにする
            //-------------------------------------------------
            hlslpp::float3 toItem = pickup.restPosition - playerPosition;
            toItem.y              = 0.0f;

            const float distance = hlslpp::length(toItem);
            if(distance <= pickup.radius && distance < nearestDistance) {
                nearestDistance = distance;
                nearest         = entity;
            }

            //-------------------------------------------------
            // リムグロー。遠くにあっても弱く光らせて草むらの中で目に留まるようにし、
            // glowNearDistanceより近づくほど強める（拾う直前が一番明るい）
            //-------------------------------------------------
            const float nearness = 1.0f - SmoothStep01((distance - pickup.radius) / std::max(params.glowNearDistance - pickup.radius, 1.0f));
            const float blendT   = 1.0f - std::exp(-params.rimGlowBlendSpeed * deltaTime);
            pickup.rimGlowBlend += (nearness - pickup.rimGlowBlend) * blendT;

            // 0→1→0を往復する脈動。sinを2乗して滑らかな山にする
            const float wave  = std::sin(pickup.pulseTime * params.pulseSpeed);
            const float pulse = wave * wave;

            if(auto* rimGlow = registry.try_get<Tsukino::BuiltIn::ECS::RimGlowComponent>(entity)) {
                const float nearGlow = params.glowMin + (params.glowMax - params.glowMin) * pulse;
                const float idleGlow = params.idleGlow * pulse;

                rimGlow->active       = true;
                rimGlow->rimColor     = params.rimColor;
                rimGlow->rimIntensity = params.idleRimIntensity + (params.rimIntensityMax - params.idleRimIntensity) * pickup.rimGlowBlend;
                rimGlow->rimPower     = params.rimPower;
                rimGlow->glow         = idleGlow + (nearGlow - idleGlow) * pickup.rimGlowBlend;
            }
        });

        //-------------------------------------------------------------
        // 触れたら自動で取得する。持てる数に上限が無く、同じ種類はレベルアップになるので
        // 拾って損する場面が無く、キーで選ばせる意味が無いため。
        // 反復中にコンポーネント構成を変えるとViewが壊れるため、反復の後でまとめて行う
        //-------------------------------------------------------------
        if(nearest != entt::null) {
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
                    // レベル加算・リムグローは吸い寄せが完了した瞬間（Update冒頭の吸収処理）で行う。
                    // Scene::DestroyEntity()を経由しないSystemからの直接破棄は前例が無く、
                    // EffectSystem/PhysicsSystem側にScene経由の破棄を前提にした注意書きがあるため、
                    // 吸収完了時も非表示化のみ行い、以後owner=entt::nullのまま放置する
                    // （weaponInventoryに入らないためCombatSystem/PlayerSystemからは触られない）
                    registry.AddComponent<WeaponAbsorbComponent>(nearest).target = existingWeaponEntity;
                } else {
                    // 拾った武器は「所有者つきの浮遊武器」へ昇格させる
                    pickedWeapon.owner        = playerEntity;
                    pickedWeapon.floatEnabled = true;
                    // 拾った瞬間は足元にあるので、ばね追従は浮遊の定位置から始めさせる
                    // （地面から引っ張り上げられる動きが毎回入ると拾得感がぼやける）
                    pickedWeapon.hasFollowSpringState = false;

                    player->weaponInventory.push_back(nearest);

                    // 今は拾った直後の武器がLv1なので進化しないが、将来レベル付きで落ちる武器を
                    // 足しても取りこぼさないよう、手持ちが変わる箇所では必ず判定しておく
                    TryEvolvePlayerWeapons(registry, playerEntity);

                    // 画面右の取得ログへ流す（初取得のときだけ。2本目以降は上の吸収側が出す）
                    if(auto* eventBus = registry.GetContext<Tsukino::ECS::EventBus*>()) {
                        eventBus->Publish(
                            GameLogEvent{GameLogCategory::WeaponAcquired, GetWeaponEntry(pickedWeapon.weaponId).displayName});
                    }
                }
            }

            // 演出とワールド判定を止める（PickupComponentを外すのでこれ以降候補に上がらない）
            if(auto* rimGlow = registry.try_get<Tsukino::BuiltIn::ECS::RimGlowComponent>(nearest)) {
                rimGlow->active = false;
            }
            registry.RemoveComponent<PickupComponent>(nearest);
        }
    }
}    // namespace CombatAndroid::ECS
