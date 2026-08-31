//-------------------------------------------------------------
//! @file   CombatSystem.cpp
//! @brief  CombatSystemクラスの実装
//! @author 山﨑愛
//-------------------------------------------------------------
#include <CombatAndroid/ECS/System/CombatSystem.hpp>
#include <CombatAndroid/ECS/Component/PlayerComponent.hpp>
#include <CombatAndroid/ECS/Component/WeaponComponent.hpp>
#include <CombatAndroid/ECS/Component/EnemyComponent.hpp>
#include <CombatAndroid/ECS/Component/EnemyAnimationSetComponent.hpp>
#include <CombatAndroid/ECS/Component/EnemyAttackHitboxComponent.hpp>
#include <CombatAndroid/ECS/Component/HealthComponent.hpp>
#include <CombatAndroid/ECS/Component/PlayerSkillComponent.hpp>
#include <CombatAndroid/ECS/Component/HitStopComponent.hpp>
#include <CombatAndroid/ECS/Component/ProjectileComponent.hpp>
#include <CombatAndroid/ECS/Event/WeaponHitEvent.hpp>
#include <CombatAndroid/ECS/Event/PlayerDamagedEvent.hpp>
#include <CombatAndroid/ECS/Utility/CombatHit.hpp>

#include <Tsukino/BuiltIn/ECS/Component/TransformComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/ModelComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/NodeWorldPoseComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/NodeWorldMatrixComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/CharacterControllerComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/AnimationPlayerComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/EffectComponent.hpp>

#include <Tsukino/EngineIntegration/EngineContext.hpp>
#include <Tsukino/EngineIntegration/ECS/System/PhysicsSystem.hpp>
#include <Tsukino/EngineIntegration/ECS/System/EffectSystem.hpp>
#include <Tsukino/Engine/Asset/AssetManager.hpp>
#include <Tsukino/Engine/Asset/Model/ModelAsset.hpp>
#include <Tsukino/GraphicsCommon/Model/ModelData.hpp>
#include <Tsukino/Core/ECS/Event/EventBus.hpp>
#include <Tsukino/Core/Math/MathHelper.hpp>
#include <Tsukino/Engine/Physics/SpringBone/SpringBoneMath.hpp>
#ifdef _DEBUG
#include <CombatAndroid/ECS/Utility/CombatDebugDraw.hpp>
#include <Tsukino/Renderer/Renderer.hpp>
#include <Tsukino/GraphicsCommon/Vertex/DebugVertex.hpp>
#include <fstream>
#endif

#include <hlsl++.h>
#include <algorithm>
#include <cmath>
#include <vector>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    namespace {
        //-------------------------------------------------------------
        // プレイヤーが被弾したときのヒットストップは、敵を殴ったとき
        // （CombatHit.hppのkHitStopDuration/kHitStopScale）より弱めにする。
        // プレイヤー操作が止まる時間を短くし、被弾直後にすぐ回避・反撃できるようにするため
        //-------------------------------------------------------------
        constexpr float kPlayerHitStopDuration = 0.2f;
        constexpr float kPlayerHitStopScale    = 0.15f;

        //-------------------------------------------------------------
        //! @struct PendingProjectileSpawn
        //! @brief  「この武器が斬撃弾を撃つ」と決まった内容を、実際に生成するまで保留しておく箱
        //! @note   武器を回すループの最中にエンティティを作ると、コンポーネントのプールが
        //!         再確保されてループが参照中のWeaponComponent/TransformComponentが宙に浮く。
        //!         ExpOrbSystemがEnemyDiedEventをキューへ積んでからECSを触るのと同じ理由
        //-------------------------------------------------------------
        struct PendingProjectileSpawn {
            entt::entity       weaponEntity;      //!< 発射元の武器（生成時に残りの設定値を引き直す）
            hlslpp::float3     position;          //!< 発射位置
            hlslpp::quaternion rotation;          //!< 発射時の姿勢（エフェクトの向き）
            hlslpp::float3     direction;         //!< 進行方向（正規化済み・水平）
            float              damage;            //!< 命中時に与える実ダメージ（倍率適用後の確定値）
            float              lifeStealRatio;    //!< スキル「嫉妬」の吸収割合
            bool               piercing;          //!< 貫通するか（falseなら1体で消滅する）
        };

        //-------------------------------------------------------------
        //! @brief  エンティティのModelComponentが指すモデルに対し、名前でボーンのnodeIndexを解決する。
        //!         WeaponComponentの手ボーン追従・EnemyAttackHitboxComponentの両方が使う共通ロジック。
        //!         resolvedAgainstModelが対象モデルと一致していれば何もしない（毎フレームの名前検索を避ける）
        //! @param  ctx                  [in]     アセットマネージャを引くためのエンジンコンテキスト
        //! @param  registry             [in]     ECSレジストリ
        //! @param  modelEntity          [in]     ModelComponentを持つエンティティ（ボーンの持ち主）
        //! @param  boneName             [in]     解決したいボーン名
        //! @param  resolvedAgainstModel [in,out] 最後に解決した時点のモデルハンドル（キャッシュキー）
        //! @param  outNodeIndex         [in,out] 解決結果のnodeIndex（見つからなければUINT32_MAXのまま）
        //-------------------------------------------------------------
        void ResolveBoneNodeIndex(Tsukino::EngineIntegration::EngineContext* ctx, Tsukino::ECS::Registry& registry, Tsukino::ECS::Entity modelEntity,
                                  const std::string& boneName, Tsukino::Asset::AssetHandle& resolvedAgainstModel, u32& outNodeIndex) {
            if(!ctx || !ctx->assetManager || !registry.HasComponent<Tsukino::BuiltIn::ECS::ModelComponent>(modelEntity))
                return;

            auto& model = registry.GetComponent<Tsukino::BuiltIn::ECS::ModelComponent>(modelEntity);
            if(resolvedAgainstModel == model.modelHandle)
                return;    // 既に同じモデルに対して解決済み

            auto asset = ctx->assetManager->Get(model.modelHandle);
            if(!asset || asset->GetType() != Tsukino::Asset::AssetType::Model)
                return;

            auto modelAsset = std::static_pointer_cast<Tsukino::Asset::ModelAsset>(asset);
            outNodeIndex     = UINT32_MAX;
            for(u32 i = 0; i < modelAsset->modelData.nodes.size(); ++i) {
                if(modelAsset->modelData.nodes[i].name == boneName) {
                    outNodeIndex = i;
                    break;
                }
            }
            resolvedAgainstModel = model.modelHandle;    // アセットが読めた時点で確定（見つからなければUINT32_MAXのまま）
        }

        //-------------------------------------------------------------
        //! @brief  点と線分の間の最短距離の2乗を求める
        //! @param  point [in] 対象の点
        //! @param  segA  [in] 線分の始点
        //! @param  segB  [in] 線分の終点
        //-------------------------------------------------------------
        [[nodiscard]]
        float DistanceSqPointToSegment(const hlslpp::float3& point, const hlslpp::float3& segA, const hlslpp::float3& segB) {
            hlslpp::float3 ab      = segB - segA;
            float          abLenSq = hlslpp::dot(ab, ab);
            float          abDot   = hlslpp::dot(point - segA, ab);

            float t = (abLenSq > 1e-8f) ? std::clamp(abDot / abLenSq, 0.0f, 1.0f) : 0.0f;

            hlslpp::float3 closest = segA + ab * t;
            hlslpp::float3 diff     = point - closest;
            return hlslpp::dot(diff, diff);
        }

        //-------------------------------------------------------------
        //! @brief  2つの線分の間の最短距離の2乗を求める
        //!         （Ericson "Real-Time Collision Detection" のClosestPtSegmentSegmentを移植したもの）
        //! @param  p1 [in] 線分1の始点
        //! @param  q1 [in] 線分1の終点
        //! @param  p2 [in] 線分2の始点
        //! @param  q2 [in] 線分2の終点
        //! @note   敵の手ボーンの移動軌跡（前フレーム位置→今フレーム位置）とプレイヤーのカプセル芯線との
        //!         判定に使う。速い振りやフレーム落ちで点判定をすり抜けるのを防ぐためのスイープ判定
        //-------------------------------------------------------------
        [[nodiscard]]
        float DistanceSqBetweenSegments(const hlslpp::float3& p1, const hlslpp::float3& q1, const hlslpp::float3& p2, const hlslpp::float3& q2) {
            constexpr float kEpsilon = 1e-8f;

            hlslpp::float3 d1 = q1 - p1;    // 線分1の方向ベクトル
            hlslpp::float3 d2 = q2 - p2;    // 線分2の方向ベクトル
            hlslpp::float3 r  = p1 - p2;

            float a = hlslpp::dot(d1, d1);    // 線分1の長さの2乗
            float e = hlslpp::dot(d2, d2);    // 線分2の長さの2乗
            float f = hlslpp::dot(d2, r);

            float s = 0.0f;
            float t = 0.0f;

            if(a <= kEpsilon && e <= kEpsilon) {
                // 両方とも点に退化している
                s = 0.0f;
                t = 0.0f;
            } else if(a <= kEpsilon) {
                // 線分1が点に退化している
                s = 0.0f;
                t = std::clamp(f / e, 0.0f, 1.0f);
            } else {
                float c = hlslpp::dot(d1, r);
                if(e <= kEpsilon) {
                    // 線分2が点に退化している
                    t = 0.0f;
                    s = std::clamp(-c / a, 0.0f, 1.0f);
                } else {
                    float b     = hlslpp::dot(d1, d2);
                    float denom = a * e - b * b;

                    if(denom > kEpsilon)
                        s = std::clamp((b * f - c * e) / denom, 0.0f, 1.0f);
                    else
                        s = 0.0f;    // 2直線がほぼ平行

                    t = (b * s + f) / e;

                    if(t < 0.0f) {
                        t = 0.0f;
                        s = std::clamp(-c / a, 0.0f, 1.0f);
                    } else if(t > 1.0f) {
                        t = 1.0f;
                        s = std::clamp((b - c) / a, 0.0f, 1.0f);
                    }
                }
            }

            hlslpp::float3 c1   = p1 + d1 * s;
            hlslpp::float3 c2   = p2 + d2 * t;
            hlslpp::float3 diff = c1 - c2;
            return hlslpp::dot(diff, diff);
        }

        //-------------------------------------------------------------
        //! @brief  1体の敵への「武器ヒット」を確定させる。
        //!         直線カプセル判定・AoE(範囲攻撃)判定の両方から呼ばれる。
        //!         実ダメージの組み立て（武器の基礎ダメージ×連撃段の倍率×スキル「憤怒」の倍率）だけを
        //!         ここで行い、ヒットの確定そのものは斬撃弾と共有するApplyCombatHit（CombatHit.hpp）へ委ねる
        //! @param  hitPositionFallback [in] 対象にTransformComponentが無い場合に使う位置
        //-------------------------------------------------------------
        void ApplyWeaponHitToEntity(Tsukino::ECS::Registry& registry, Tsukino::ECS::EventBus* eventBus, entt::entity weaponEntity,
                                    WeaponComponent& weapon, entt::entity hitEntity, const hlslpp::float3& hitPositionFallback,
                                    float skillAttackMultiplier, float skillLifeStealRatio) {
            // 実ダメージ＝武器の基礎ダメージ×連撃段の倍率（PlayerAnimationSystemが段ごとに書く）
            //             ×スキル「憤怒」の攻撃力倍率
            float dealtDamage = weapon.damage * weapon.damageMultiplier * skillAttackMultiplier;

            ApplyCombatHit(registry, eventBus, weapon.owner, weaponEntity, hitEntity, dealtDamage, hitPositionFallback,
                           weapon.hitEnemiesThisAttack, skillLifeStealRatio, weapon.lifeStealHealedThisAttack);
        }
    }    // namespace

    //-------------------------------------------------------------
    //! @brief システムの更新
    //-------------------------------------------------------------
    void CombatSystem::Update(Tsukino::ECS::Registry& registry, float deltaTime) {
        auto* ctx      = registry.GetContext<Tsukino::EngineIntegration::EngineContext*>();
        auto* eventBus = registry.GetContext<Tsukino::ECS::EventBus*>();

        // このフレームに発射が確定した斬撃弾。生成は武器のループを抜けてから行う
        std::vector<PendingProjectileSpawn> pendingProjectiles;

        //-------------------------------------------------------------
        // 武器：所有者への追従、タイマー更新、攻撃発生時のカプセルオーバーラップ判定によるダメージ
        //-------------------------------------------------------------
        auto weaponView = registry.View<WeaponComponent, Tsukino::BuiltIn::ECS::TransformComponent>();
        weaponView.each([&](entt::entity entity, WeaponComponent& weapon, Tsukino::BuiltIn::ECS::TransformComponent& transform) {
            // 追従・浮遊計算でtransformが書き換わる前の姿勢（＝前フレーム終了時点の姿勢）を保存しておく。
            // 新しいアタックがこのフレームで開始した場合、サブステップ補間のprevとして使う
            hlslpp::float3     preFollowPosition = transform.position;
            hlslpp::quaternion preFollowRotation = transform.rotation;

            if(weapon.owner != entt::null && registry.HasComponent<Tsukino::BuiltIn::ECS::TransformComponent>(weapon.owner)) {
                Tsukino::BuiltIn::ECS::TransformComponent& ownerTransform =
                    registry.GetComponent<Tsukino::BuiltIn::ECS::TransformComponent>(weapon.owner);

                // 所有者のアタッチ対象ボーン名を解決してキャッシュする。
                // NodeWorldPoseComponent/NodeWorldMatrixComponentは、所有者のModelComponentが指す
                // キャラクターモデル自身のmodelData.nodesを基準にAnimationSystemが書き出す
                // （再生中のクリップのnode配列ではない。クリップ側は名前でチャンネルを引くためだけに
                // 使われ、ノードindex空間はモデル側で固定される）。そのため解決もModelComponent側の
                // ノード一覧に対して行い、再生中のクリップが切り替わっても再解決は不要になる
                ResolveBoneNodeIndex(ctx, registry, weapon.owner, weapon.handBoneName, weapon.resolvedAgainstModel, weapon.handBoneNodeIndex);

                // isAttackingがfalse/trueへ瞬時に切り替わっても追従先の「目標」自体が不連続にジャンプ
                // しないよう、0(非攻撃)↔1(攻撃)の連続値attackBlendを指数減衰で追従させ、
                // 追従パラメータはこのattackBlendで連続的に補間する（末尾のexp減衰補間は目標への
                // 追従を滑らかにするだけで、目標自体の飛びは吸収しきれず「カクッ」とスナップして
                // 見える不具合の原因だったため、目標を作る側で先に連続化する）
                float attackBlendTarget = weapon.isAttacking ? 1.0f : 0.0f;
                float attackBlendLerpT  = 1.0f - std::exp(-weapon.attackBlendSpeed * deltaTime);
                weapon.attackBlend += (attackBlendTarget - weapon.attackBlend) * attackBlendLerpT;

                // attackBlendが1に近いほどattackHandTrackingWeight/attackLocalOffset/attackGripRotationOffsetへ、
                // 0に近いほど通常値へ連続的に補間する。
                // localOffsetは「ほぼ静止した基準点からの浮遊位置」として調整された大きい値（170ユニット近く）
                // なので、実際に振られる手ボーンにそのまま適用するとテコの原理で武器が大きく・速く振り回されてしまう
                float              trackingWeight = weapon.handTrackingWeight + (weapon.attackHandTrackingWeight - weapon.handTrackingWeight) * weapon.attackBlend;
                hlslpp::float3     gripOffset     = hlslpp::lerp(weapon.localOffset, weapon.attackLocalOffset, weapon.attackBlend);
                hlslpp::quaternion gripRotOffset = Tsukino::Core::Math::SlerpShortestPath(weapon.gripRotationOffset, weapon.attackGripRotationOffset, weapon.attackBlend);

                // ボーンが解決できていれば手のボーンへアタッチする。できなければ従来通り
                // ルートTransformへ固定オフセットで追従させる（フォールバック）。
                // ここではtransformへ直接書き込まず、まず目標位置・姿勢を求める
                // （攻撃モーションへの出入りでgripOffset/trackingWeightが瞬時に切り替わっても
                //   目標そのものが動くだけで、実際の反映は末尾のexp減衰補間に任せるため）
                hlslpp::float3     targetPosition = transform.position;
                hlslpp::quaternion targetRotation = transform.rotation;

                bool attachedToBone = false;
                if(weapon.handBoneNodeIndex != UINT32_MAX
                   && registry.HasComponent<Tsukino::BuiltIn::ECS::NodeWorldMatrixComponent>(weapon.owner)) {
                    auto& ownerMatrices = registry.GetComponent<Tsukino::BuiltIn::ECS::NodeWorldMatrixComponent>(weapon.owner);
                    if(weapon.handBoneNodeIndex < ownerMatrices.matrices.size()) {
                        // Unityのボーンソケットと同じ考え方：実際にスキンメッシュを描画するのに使う
                        // スケール込みのボーン行列（globalNodeMatrices由来）から位置・回転を取り出す。
                        // NodeWorldPoseComponent（スケール1近似の軽量版。揺れ物物理専用）は使わない
                        // ——これが攻撃中に武器が暴れる不具合の原因だった。
                        hlslpp::float3     handBonePos;
                        hlslpp::quaternion handBoneRot;
                        Tsukino::Core::Math::matrix::decomposePositionRotation(
                            ownerMatrices.matrices[weapon.handBoneNodeIndex], handBonePos, handBoneRot);

                        // モデルローカルのボーン姿勢 → ワールド空間（所有者のTransformを反映）
                        //
                        // 【重要】hlsl++のクォータニオン積は行列積と合成順が逆。
                        //   「Aを適用してからBを適用する」合成は、
                        //     行列          : hlslpp::mul(A, B)   （行ベクトル規約。TransformSystem等と同じ）
                        //     クォータニオン: hlslpp::mul(B, A)   （Hamilton積。親を左に置く）
                        //   （hlslpp本体のunit_tests_quaternion.cpp「M_AB = M_A * M_B / Q_AB = Q_B * Q_A」参照）
                        //   ここを行列と同じ順で書くと、実効的にボーンの回転軸が所有者の向きの逆回転で
                        //   回ってしまい、「攻撃の振りの軌道がキャラクターの向きによって変わる」不具合になる。
                        //   なおベクトルの回転は mul(v, q) が前方回転で、mul(q, v) は逆回転（別関数）なので混同しない
                        hlslpp::float3 handWorldPos =
                            ownerTransform.position + hlslpp::mul(handBonePos * ownerTransform.scale, ownerTransform.rotation);
                        hlslpp::quaternion handWorldRot = hlslpp::mul(ownerTransform.rotation, handBoneRot);

                        // trackingWeightで手ボーン姿勢への追従度を位置・回転の両方に一貫して適用する
                        // （アニメーションクリップのボーン姿勢が信頼できない/振り幅が大きい場合に下げて使う。
                        //   0にすると所有者のルートTransformにのみ追従する）
                        hlslpp::float3     worldPos = hlslpp::lerp(ownerTransform.position, handWorldPos, trackingWeight);
                        hlslpp::quaternion worldRot = Tsukino::Core::Math::SlerpShortestPath(ownerTransform.rotation, handWorldRot, trackingWeight);

                        // 握り位置・向きの微調整（WeaponComponentのオフセットをボーンローカル空間で適用）。
                        // gripRotOffsetは「手のローカル軸まわりの補正」なので、上と同じ理由で
                        // 親（worldRot）を左に置く（逆にするとワールド軸まわりの補正になってしまう）
                        targetPosition = worldPos + hlslpp::mul(gripOffset, worldRot);
                        targetRotation = hlslpp::mul(worldRot, gripRotOffset);
                        attachedToBone  = true;
                    }
                }

                if(!attachedToBone) {
                    // ボーン追従側（上）と同じ規約に揃える。
                    // mul(q, v)は逆回転になる別関数なので、位置オフセットの回転はmul(v, q)を使う
                    hlslpp::float3 rotatedOffset = hlslpp::mul(gripOffset, ownerTransform.rotation);
                    targetPosition                = ownerTransform.position + rotatedOffset;
                    targetRotation                = hlslpp::mul(ownerTransform.rotation, gripRotOffset);
                }

                // 手に持つのではなく所有者の周りをふわふわ浮遊させる演出
                // （localOffsetの位置を基準に上下・左右前後へゆったり漂わせ、
                //   姿勢もわずかに前後へ傾けるだけに留めてほぼ縦向きを保つ）。
                // isAttackingでの完全な有効/無効切り替えはやめ、常に浮遊姿勢・漂いを計算した上で
                // attackBlendによりボーン追従の姿勢と連続的にブレンドする（攻撃開始/終了の瞬間に
                // 回転が浮遊姿勢↔ボーン追従で不連続に入れ替わってスナップして見える不具合の原因だったため）
                if(weapon.floatEnabled) {
                    weapon.floatTime += deltaTime;

                    // 所有者に対してほぼ縦向きを保ったまま、わずかに前後・左右へ揺れるだけ。
                    // gripRotationOffsetは「手に持つ」ときの握り角度調整用のオフセットで、
                    // モデル自体がエクスポート時点で既に縦向き（Y-up）のため、浮遊時には適用しない。
                    //
                    // 揺れは所有者のローカル空間で作ってから所有者の向きで回す（漂いの位置と同じ考え方）。
                    // ワールド絶対姿勢のままにすると、下のattackBlendによるブレンドが
                    // 「浮遊姿勢→手ボーン姿勢」へ辿る経路がキャラクターの向きごとに変わってしまい、
                    // 攻撃の入り／抜けの動きが向きによって違って見える原因になる
                    float               swayX = std::sin(weapon.floatTime * weapon.floatSwaySpeed) * weapon.floatSwayAngle;
                    float               swayZ = std::cos(weapon.floatTime * weapon.floatSwaySpeed * 0.8f) * weapon.floatSwayAngle;
                    hlslpp::quaternion localSway    = hlslpp::mul(hlslpp::quaternion::rotation_x(swayX), hlslpp::quaternion::rotation_z(swayZ));
                    hlslpp::quaternion floatRotation = hlslpp::mul(ownerTransform.rotation, localSway);

                    // attackBlendが1に近いほどボーン追従の姿勢（targetRotation）、0に近いほど浮遊姿勢へ
                    targetRotation = Tsukino::Core::Math::SlerpShortestPath(floatRotation, targetRotation, weapon.attackBlend);

                    // 上下方向の漂い（ワールドYはどの向きでも共通なのでそのまま加算）。
                    // attackBlendが1に近づくほど自然にゼロへ収束させる
                    float bobOffset = std::sin(weapon.floatTime * weapon.floatBobSpeed) * weapon.floatBobAmplitude * (1.0f - weapon.attackBlend);
                    targetPosition.y += bobOffset;

                    // 左右・前後方向の漂い。所有者のローカル空間で計算してからownerの向きで回転することで、
                    // 所有者に対する相対的な漂い方がどの向きでも同じになるようにする（こちらもattackBlendでゼロへ収束）
                    hlslpp::float3 localDrift(std::sin(weapon.floatTime * weapon.floatDriftSpeed) * weapon.floatDriftAmplitude,
                                              0.0f,
                                              std::cos(weapon.floatTime * weapon.floatDriftSpeed * 0.7f) * weapon.floatDriftAmplitude);
                    targetPosition += hlslpp::mul(localDrift, ownerTransform.rotation) * (1.0f - weapon.attackBlend);

                    // 選択中の武器だけ高く浮かせて、どれが使用中かを視覚的に示す
                    // （attachPositionLerpSpeedによる指数減衰で自然に上下するので、切り替え時のジャンプ処理は不要）
                    if(weapon.floatSelected) {
                        targetPosition.y += weapon.floatSelectedHeightBoost * (1.0f - weapon.attackBlend);
                    }
                }

                // gripPointLocalが手のひら位置（targetPosition）に来るよう、姿勢確定後に
                // 位置側だけを引く。姿勢（targetRotation）で回してから引くことで、角度調整と
                // 位置調整を独立に行える（角度を振るたびに位置オフセットを取り直さずに済む）。
                // 攻撃中のみ効かせたいのでattackBlendで重み付けする（浮遊演出時は影響しない）
                targetPosition -= hlslpp::mul(weapon.gripPointLocal * transform.scale, targetRotation) * weapon.attackBlend;

                // 攻撃中、目標へ十分近づいたら指数減衰をやめて目標へ直接スナップする（ビタ置き）。
                // 指数減衰は原理上どれだけ速くしても定常的な遅れが残り（速度/lerpSpeed相当）、
                // 速い振りでは「手から遅れて武器がついてくる」ように見えてしまうため、
                // もう遅れが気にならない距離・角度まで来た時点で追従方式を切り替える。
                // ボーンが解決できていない（フォールバック）間はスナップしない
                bool canSnap = weapon.isAttacking && attachedToBone;
                if(!canSnap) {
                    weapon.isSnapped = false;
                } else if(!weapon.isSnapped) {
                    float distance = hlslpp::length(targetPosition - transform.position);

                    // クォータニオンの内積 = cos(角度差/2)。二重被覆（qと-qが同じ回転）を
                    // 考慮して絶対値を取ってから比較する
                    float rotDot = transform.rotation.x * targetRotation.x + transform.rotation.y * targetRotation.y
                                   + transform.rotation.z * targetRotation.z + transform.rotation.w * targetRotation.w;
                    float angleDot = std::cos(weapon.attackSnapAngleDeg * (3.14159265f / 180.0f) * 0.5f);

                    if(distance <= weapon.attackSnapDistance && std::abs(rotDot) >= angleDot) {
                        weapon.isSnapped = true;
                    }
                }

                if(weapon.isSnapped) {
                    // ビタ置き：手のひらの動きにそのまま一致させる（遅れなし）
                    transform.position = targetPosition;
                    transform.rotation = targetRotation;
                } else {
                    // スナップに至るまでは指数減衰で目標へ追従させる。攻撃モーションへの出入りや
                    // フォールバック切り替えで目標位置・姿勢が瞬時に変わっても瞬間移動しないための
                    // ものだが、攻撃中はattackBlendによりattackApproachLerpSpeedへ連続的に速度を
                    // 上げ、スナップ圏内へ素早く入れるようにする
                    // （PlayerSystem.cppの向き直し補間と同じ手法。フレームレート非依存）
                    float positionSpeed = weapon.attachPositionLerpSpeed
                                           + (weapon.attackApproachLerpSpeed - weapon.attachPositionLerpSpeed) * weapon.attackBlend;
                    float rotationSpeed = weapon.attachRotationLerpSpeed
                                           + (weapon.attackApproachLerpSpeed - weapon.attachRotationLerpSpeed) * weapon.attackBlend;
                    float positionLerpT = 1.0f - std::exp(-positionSpeed * deltaTime);
                    float rotationLerpT = 1.0f - std::exp(-rotationSpeed * deltaTime);
                    transform.position   = hlslpp::lerp(transform.position, targetPosition, positionLerpT);
                    transform.rotation   = Tsukino::Core::Math::SlerpShortestPath(transform.rotation, targetRotation, rotationLerpT);
                }
                transform.dirty = true;

#ifdef _DEBUG
                // 当たり判定カプセルを可視化する（OverlapCapsuleへ実際に渡す値と同じcenter/rotation/radius/halfHeight）。
                // 当たり判定有効中は赤、それ以外はシアンにする
                if(ctx && ctx->renderer) {
                    hlslpp::float3 bladeDir      = hlslpp::mul(hlslpp::float3(0.0f, 1.0f, 0.0f), transform.rotation);
                    float          debugHalfLen  = weapon.range * 0.5f;
                    hlslpp::float3 debugCapsuleCenter = transform.position + bladeDir * debugHalfLen;
                    hlslpp::float4 color =
                        weapon.isActive ? hlslpp::float4(1.0f, 0.2f, 0.0f, 1.0f) : hlslpp::float4(0.0f, 1.0f, 1.0f, 1.0f);
                    DrawWireCapsule(ctx->renderer, debugCapsuleCenter, transform.rotation, weapon.hitCapsuleRadius, debugHalfLen, color);

                    // AoE(範囲攻撃)半径の可視化。発動待ちカウントダウン中はマゼンタ、それ以外は薄紫で常時表示する
                    if(weapon.areaAttackRadius > 0.0f) {
                        hlslpp::float4 aoeColor = weapon.areaAttackArmed
                            ? hlslpp::float4(1.0f, 0.0f, 1.0f, 1.0f)
                            : hlslpp::float4(0.5f, 0.0f, 0.5f, 0.5f);
                        DrawWireCircleXZ(ctx->renderer, transform.position, weapon.areaAttackRadius, aoeColor);
                    }
                }
#endif
            }

            if(weapon.cooldownTimer > 0.0f) {
                weapon.cooldownTimer -= deltaTime;
                if(weapon.cooldownTimer < 0.0f)
                    weapon.cooldownTimer = 0.0f;
            } else if(weapon.attackRequested) {
                weapon.isActive        = true;
                weapon.activeTimer     = (weapon.nextActiveDurationOverride >= 0.0f) ? weapon.nextActiveDurationOverride : weapon.activeDuration;
                weapon.nextActiveDurationOverride = -1.0f;
                weapon.cooldownTimer   = weapon.cooldown;
                weapon.attackRequested = false;

                // 新しいアタックの開始。ヒット済み記録はここでのみクリアする
                // （毎フレームの判定側でクリアすると同じ敵に何度もヒットしてしまう）
                weapon.hitEnemiesThisAttack.clear();
                weapon.lifeStealHealedThisAttack = 0.0f;    // 嫉妬の吸収上限もアタック単位なので同じタイミングで戻す

                // 新しいアタック開始フレームも前フレーム姿勢(浮遊/追従姿勢)からサブステップ補間できるよう、
                // 追従計算前に保存しておいた姿勢をprevとして与える（falseにリセットして初回フレームだけ
                // 判定漏れさせていたのをやめる）
                weapon.prevAttackPosition = preFollowPosition;
                weapon.prevAttackRotation = preFollowRotation;
                weapon.hasPrevAttackPose  = true;

                // AoE(範囲攻撃)の武装。この段がAoEを要求していて、かつ装備武器がAoE対応
                // （areaAttackRadius>0、warhammer等のみ）の場合だけタイマーを仕込む
                if(weapon.pendingAreaAttack && weapon.areaAttackRadius > 0.0f) {
                    weapon.areaAttackArmed = true;
                    weapon.areaAttackTimer = std::max(weapon.pendingAreaAttackDelay, 0.0f);
                } else {
                    weapon.areaAttackArmed = false;
                }
                weapon.pendingAreaAttack = false;    // 一度きりの要求として消費する（nextActiveDurationOverrideと同じ作法）

                // 斬撃弾の武装。溜め攻撃の解放（PlayerAnimationSystem）が要求していて、かつ
                // 装備武器が斬撃弾に対応（エフェクトが設定済み。battleaxeのみ）の場合だけタイマーを仕込む
                if(weapon.pendingProjectile && weapon.projectileEffectAsset.IsValid()) {
                    weapon.projectileArmed = true;
                    weapon.projectileTimer = std::max(weapon.projectileSpawnDelay, 0.0f);
                } else {
                    weapon.projectileArmed = false;
                }
                weapon.pendingProjectile = false;    // AoEと同じく一度きりの要求として消費する
            }

            //-------------------------------------------------------------
            // 憤怒・怠惰（攻撃力倍率）と嫉妬（吸収割合）のスキル値。持ち主から引く。
            // 直線カプセル判定・AoE判定の両方で使うため、どちらのブロックに入る前に1回だけ求めておく
            //-------------------------------------------------------------
            float skillAttackMultiplier = 1.0f;
            float skillLifeStealRatio   = 0.0f;
            if(weapon.owner != entt::null) {
                if(auto* ownerSkills = registry.try_get<PlayerSkillComponent>(weapon.owner)) {
                    skillAttackMultiplier = ownerSkills->attackMultiplier;
                    skillLifeStealRatio   = ownerSkills->lifeStealRatio;
                }
            }

            if(weapon.isActive) {
                // 当たり判定が有効な間、毎フレーム武器の「今の」姿勢（スイング追従済みのtransform）を基準に
                // グリップ(transform.position)から刃の向き（ローカルY軸）へrangeだけ伸びるカプセルを構築し、
                // Jolt物理へオーバーラップ問い合わせする（PhysicsSystem::OverlapCapsule）。
                //
                // 剣の振りはグリップの移動量よりも「回転」が支配的（柄はほぼ同じ位置に留まり、刃先が
                // 弧を描いて大きく動く）。現フレームの姿勢1回だけの判定だと、速い振りやフレーム落ちで
                // 刃先が敵を「跨いで」通過し、すり抜けることがある。Joltのカプセル同士のスイープ判定
                // （CastShape）は平行移動のみしか表現できず回転を考慮できないため、この用途には使えない。
                // そこで前フレーム→今フレームの姿勢を位置(lerp)・回転(slerp)で補間したサブステップに
                // 分割し、各サブステップでOverlapCapsuleを呼んで弧全体をカバーする。
                // 1回のアタックで同じ敵に何度も当たらないよう、既にヒットした敵はhitEnemiesThisAttackに記録してスキップする
                if(ctx && ctx->physicsSystem) {
                    hlslpp::float3 bladeDir      = hlslpp::mul(hlslpp::float3(0.0f, 1.0f, 0.0f), transform.rotation);
                    float          halfLen       = weapon.range * 0.5f;
                    hlslpp::float3 capsuleCenter = transform.position + bladeDir * halfLen;

                    std::vector<entt::entity> overlapping;

                    if(weapon.hasPrevAttackPose) {
                        // 刃先（グリップからrange分先）の前フレーム→今フレームの移動距離から、
                        // 判定漏れが出ないサブステップ数を決める（半径分進むごとに1ステップ）
                        hlslpp::float3 prevBladeDir = hlslpp::mul(hlslpp::float3(0.0f, 1.0f, 0.0f), weapon.prevAttackRotation);
                        hlslpp::float3 prevTipPos   = weapon.prevAttackPosition + prevBladeDir * weapon.range;
                        hlslpp::float3 currTipPos   = transform.position + bladeDir * weapon.range;
                        float          tipMoveDist  = hlslpp::length(currTipPos - prevTipPos);

                        constexpr int kMaxSubsteps = 8;
                        int substeps = static_cast<int>(std::ceil(tipMoveDist / std::max(weapon.hitCapsuleRadius, 1.0f)));
                        substeps     = std::clamp(substeps, 1, kMaxSubsteps);

                        for(int step = 1; step <= substeps; ++step) {
                            float              t          = static_cast<float>(step) / static_cast<float>(substeps);
                            hlslpp::float3     stepPos     = hlslpp::lerp(weapon.prevAttackPosition, transform.position, t);
                            hlslpp::quaternion stepRot     = hlslpp::normalize(Tsukino::Core::Math::SlerpShortestPath(weapon.prevAttackRotation, transform.rotation, t));
                            hlslpp::float3     stepBladeDir = hlslpp::mul(hlslpp::float3(0.0f, 1.0f, 0.0f), stepRot);
                            hlslpp::float3     stepCenter   = stepPos + stepBladeDir * halfLen;

                            std::vector<entt::entity> stepHits =
                                ctx->physicsSystem->OverlapCapsule(stepCenter, stepRot, weapon.hitCapsuleRadius, halfLen);
                            overlapping.insert(overlapping.end(), stepHits.begin(), stepHits.end());
                        }
                    } else {
                        // アタック開始直後の初回フレームは前フレーム姿勢が無いため、現フレームのみ判定する
                        overlapping =
                            ctx->physicsSystem->OverlapCapsule(capsuleCenter, transform.rotation, weapon.hitCapsuleRadius, halfLen);
                    }

                    for(entt::entity hitEntity : overlapping) {
                        ApplyWeaponHitToEntity(registry, eventBus, entity, weapon, hitEntity, capsuleCenter, skillAttackMultiplier,
                                               skillLifeStealRatio);
                    }

                    weapon.prevAttackPosition = transform.position;
                    weapon.prevAttackRotation = transform.rotation;
                    weapon.hasPrevAttackPose  = true;
                }

                weapon.activeTimer -= deltaTime;
                if(weapon.activeTimer <= 0.0f) {
                    weapon.activeTimer = 0.0f;
                    weapon.isActive    = false;
                }
            }

            //-------------------------------------------------------------
            // AoE（範囲攻撃）。振り下ろし開始（attackRequestedの消費）から一定時間後に1回だけ発動する。
            // isActive/hitWindowDurationとは独立したタイマーで管理し、直線カプセル判定の
            // ヒット窓が先に閉じても予定通り発動できるようにする
            //-------------------------------------------------------------
            if(weapon.areaAttackArmed) {
                weapon.areaAttackTimer -= deltaTime;
                if(weapon.areaAttackTimer <= 0.0f) {
                    weapon.areaAttackArmed = false;

                    if(ctx && ctx->physicsSystem) {
                        // JPH::CapsuleShapeはhalfHeight>0を要求する（0は不可、JPH_ASSERT落ちする）ため、
                        // 半径に対して無視できるほど薄いカプセルにして疑似球判定として使う
                        constexpr float kAreaAttackCapsuleHalfHeight = 2.0f;
                        hlslpp::quaternion sphereRotation(0.0f, 0.0f, 0.0f, 1.0f);    // 疑似球なので向きは意味を持たない。恒等回転にしておく

                        std::vector<entt::entity> areaHits = ctx->physicsSystem->OverlapCapsule(
                            transform.position, sphereRotation, weapon.areaAttackRadius, kAreaAttackCapsuleHalfHeight);

                        for(entt::entity hitEntity : areaHits) {
                            ApplyWeaponHitToEntity(registry, eventBus, entity, weapon, hitEntity, transform.position, skillAttackMultiplier,
                                                   skillLifeStealRatio);
                        }

                        if(ctx->effectSystem && weapon.areaAttackEffectAsset.IsValid()) {
                            float pos[3] = {transform.position.x, transform.position.y, transform.position.z};
                            ctx->effectSystem->PlayEffect(registry, weapon.areaAttackEffectAsset, weapon.areaAttackEffectPath, pos, false,
                                                          weapon.areaAttackEffectScale);
                        }
                    }
                }
            }

            //-------------------------------------------------------------
            // 斬撃弾（溜め攻撃の解放）。AoEと同じく、振り下ろし開始から一定時間後に1回だけ発射する。
            // ここでは発射内容を確定させてキューへ積むだけにし、エンティティの生成は
            // このループを抜けてから行う：ループの最中に新しいエンティティへコンポーネントを
            // 足すとプールが再確保され、いま参照しているweapon/transformが宙に浮きかねない
            //-------------------------------------------------------------
            if(weapon.projectileArmed) {
                weapon.projectileTimer -= deltaTime;
                if(weapon.projectileTimer <= 0.0f) {
                    weapon.projectileArmed = false;

                    if(weapon.owner != entt::null && registry.HasComponent<Tsukino::BuiltIn::ECS::TransformComponent>(weapon.owner)) {
                        const Tsukino::BuiltIn::ECS::TransformComponent& ownerTransform =
                            registry.GetComponent<Tsukino::BuiltIn::ECS::TransformComponent>(weapon.owner);

                        // 所有者の正面へ水平に飛ばす。プレイヤーの向きはrotation_y（yawのみ）で
                        // 作られており、攻撃中は向き直りが止まるため、振り始めた方向へまっすぐ飛ぶ
                        hlslpp::float3 forward     = hlslpp::mul(hlslpp::float3(0.0f, 0.0f, 1.0f), ownerTransform.rotation);
                        hlslpp::float3 flatForward = hlslpp::float3(forward.x, 0.0f, forward.z);
                        float          flatLength  = hlslpp::length(flatForward);

                        if(flatLength > 1e-4f) {
                            flatForward = flatForward / flatLength;

                            PendingProjectileSpawn spawn{};
                            spawn.weaponEntity   = entity;
                            spawn.position       = ownerTransform.position + flatForward * weapon.projectileSpawnForward
                                                 + hlslpp::float3(0.0f, weapon.projectileSpawnHeight, 0.0f);
                            spawn.rotation       = ownerTransform.rotation;    // エフェクトを進行方向へ向ける
                            spawn.direction      = flatForward;
                            // 溜め段階の倍率（damageMultiplier）とスキル「憤怒」を乗せた実値を焼き込む。
                            // 飛翔中に武器を持ち替えても弾の威力が変わらないようにするため
                            spawn.damage         = weapon.damage * weapon.damageMultiplier * skillAttackMultiplier
                                                 * weapon.projectileDamageMultiplier;
                            spawn.lifeStealRatio = skillLifeStealRatio;
                            // 溜めの浅い一撃で群れを薙ぎ払えないよう、深く溜めたときだけ貫通させる。
                            // pendingProjectileChargeStageは要求を立てたとき（解放の瞬間）の値のままで、
                            // 次の解放まで書き換わらないため、遅延して発射するここから読んでよい
                            spawn.piercing       = weapon.pendingProjectileChargeStage >= weapon.projectilePierceMinChargeStage;

                            pendingProjectiles.push_back(spawn);
                        }
                    }
                }
            }
        });

        //-------------------------------------------------------------
        // 積んでおいた斬撃弾を実際に生成する（上のループを抜けた後に行う理由は積む側のコメント参照）。
        // 見た目はEffectComponentに任せ、EffectSystemがこのエンティティのTransformへ
        // 位置と姿勢を毎フレーム追従させる。移動と当たり判定はProjectileSystemが担当する
        //-------------------------------------------------------------
        for(const PendingProjectileSpawn& spawn : pendingProjectiles) {
            if(!registry.IsValid(spawn.weaponEntity) || !registry.HasComponent<WeaponComponent>(spawn.weaponEntity))
                continue;    // 発射を予約した武器がこのフレームで消えていた場合

            const WeaponComponent& sourceWeapon = registry.GetComponent<WeaponComponent>(spawn.weaponEntity);

            entt::entity projectileEntity = registry.CreateEntity();

            Tsukino::BuiltIn::ECS::TransformComponent& projectileTransform =
                registry.AddComponent<Tsukino::BuiltIn::ECS::TransformComponent>(projectileEntity);
            projectileTransform.position = spawn.position;
            projectileTransform.rotation = spawn.rotation;
            projectileTransform.scale    = hlslpp::float3(1.0f, 1.0f, 1.0f);
            projectileTransform.dirty    = true;
            projectileTransform.parent   = entt::null;

            ProjectileComponent& projectile = registry.AddComponent<ProjectileComponent>(projectileEntity);
            projectile.owner              = sourceWeapon.owner;
            projectile.direction          = spawn.direction;
            projectile.speed              = sourceWeapon.projectileSpeed;
            projectile.damage             = spawn.damage;
            projectile.radius             = sourceWeapon.projectileRadius;
            projectile.remainingLifetime  = sourceWeapon.projectileLifetime;
            projectile.maxDistance        = sourceWeapon.projectileMaxDistance;
            projectile.lifeStealRatio     = spawn.lifeStealRatio;
            projectile.piercing           = spawn.piercing;

            Tsukino::BuiltIn::ECS::EffectComponent& projectileEffect =
                registry.AddComponent<Tsukino::BuiltIn::ECS::EffectComponent>(projectileEntity);
            projectileEffect.effectAsset    = sourceWeapon.projectileEffectAsset;
            projectileEffect.effectPath     = sourceWeapon.projectileEffectPath;
            projectileEffect.scale          = sourceWeapon.projectileEffectScale;
            projectileEffect.playSpeed      = sourceWeapon.projectileEffectPlaySpeed;
            projectileEffect.followRotation = true;    // 斬撃波を進行方向へ向ける
            projectileEffect.active         = true;    // 次のEffectSystem::Updateが再生ハンドルを作る
        }

        //-------------------------------------------------------------
        // プレイヤーを特定する（単一プレイヤー前提）
        //-------------------------------------------------------------
        entt::entity playerEntity = entt::null;
        auto         playerView =
            registry.View<PlayerComponent, Tsukino::BuiltIn::ECS::TransformComponent, HealthComponent,
                          Tsukino::BuiltIn::ECS::CharacterControllerComponent>();
        for(auto entity : playerView) {
            playerEntity = entity;
            break;
        }

        HealthComponent*                                        playerHealth     = nullptr;
        Tsukino::BuiltIn::ECS::TransformComponent*             playerTransform  = nullptr;
        Tsukino::BuiltIn::ECS::CharacterControllerComponent* playerController = nullptr;
        // 回避の無敵時間中か。本SystemはPlayerAnimationSystem（Gameplay）より後に走るため、
        // ここで読めるのは今フレームの確定値になる
        bool             playerInvincible = false;
        // 傲慢（スキル）の被ダメージ倍率。スキルを持たない場合に備えて既定は等倍
        float            playerDamageTakenMultiplier = 1.0f;
        if(playerEntity != entt::null) {
            playerHealth     = &registry.GetComponent<HealthComponent>(playerEntity);
            playerTransform  = &registry.GetComponent<Tsukino::BuiltIn::ECS::TransformComponent>(playerEntity);
            playerController = &registry.GetComponent<Tsukino::BuiltIn::ECS::CharacterControllerComponent>(playerEntity);
            playerInvincible = registry.GetComponent<PlayerComponent>(playerEntity).isInvincible;

            if(auto* playerSkills = registry.try_get<PlayerSkillComponent>(playerEntity))
                playerDamageTakenMultiplier = playerSkills->damageTakenMultiplier;
        }

        //-------------------------------------------------------------
        // 敵の攻撃当たり判定：攻撃モーション（Attackステート）のhitStartTime〜hitStartTime+hitDurationの
        // 間だけ、手ボーンに判定球を出してプレイヤーへダメージを与える。振りかぶっただけでは当たらない。
        //
        // プレイヤーはJoltのBodyを持たないCharacterVirtualで動いているため、以前はプレイヤー専用の
        // Kinematic+isSensorセンサーカプセル（CombatAndroidScene::OnInitialize）を別途持たせ、
        // PhysicsSystem::OverlapCapsuleで検出していた。しかしこの経路は
        //   ・PhysicsSystemがシステム優先度の最後（Render/Audioの後）にあり、CombatSystemが読む
        //     センサーのボディ位置は常に1フレーム古い
        //   ・ボディの生成→毎フレーム同期→クエリという3段構成のどこか1つが欠けても例外を出さず
        //     判定ゼロになる
        // という理由で壊れやすく、実際にダメージが入らない不具合の原因になっていた。
        // そのためプレイヤーとの判定はJolt物理を経由せず、CharacterControllerComponent
        // （radius/halfHeight/centerOffset。実際に動いている当たり判定そのもの）から
        // 直接カプセルの芯線を組み立てて幾何判定する。併せて、前フレームの判定点→今フレームの
        // 判定点の線分でスイープ判定し、速い振りやフレーム落ちで判定をすり抜けるのも防ぐ
        // （EnemyAttackHitboxComponent::prevSweepPoint）。
        // 判定形状はendBoneNameの有無で2通り。空なら「boneName位置を中心とした球」
        // （頭部などの部位向け）、設定されていれば「boneName→endBoneNameを芯線とするカプセル」
        // （腕の振り抜きのように1点の球では部位を表現しきれない場合向け）
        //-------------------------------------------------------------
        auto enemyAttackView = registry.View<EnemyComponent, EnemyAnimationSetComponent, EnemyAttackHitboxComponent,
                                             Tsukino::BuiltIn::ECS::TransformComponent>();
        enemyAttackView.each([&](entt::entity                                  enemyEntity,
                                 EnemyComponent& /*enemy*/,
                                 EnemyAnimationSetComponent&                  animSet,
                                 EnemyAttackHitboxComponent&                  hitbox,
                                 Tsukino::BuiltIn::ECS::TransformComponent& enemyTransform) {
            if(animSet.currentState != EnemyAnimState::Attack)
                return;

            if(hitbox.hasLandedThisAttack)
                return;

            if(animSet.attackTimer < hitbox.hitStartTime || animSet.attackTimer > hitbox.hitStartTime + hitbox.hitDuration)
                return;    // まだ判定区間に入っていない、または過ぎた

            if(playerEntity == entt::null || !playerHealth || !playerTransform || !playerController || playerHealth->isDead)
                return;    // 当てる相手がいない

            if(!ctx)
                return;

            ResolveBoneNodeIndex(ctx, registry, enemyEntity, hitbox.boneName, hitbox.resolvedAgainstModel, hitbox.boneNodeIndex);
            if(hitbox.boneNodeIndex == UINT32_MAX
               || !registry.HasComponent<Tsukino::BuiltIn::ECS::NodeWorldMatrixComponent>(enemyEntity))
                return;

            auto& enemyMatrices = registry.GetComponent<Tsukino::BuiltIn::ECS::NodeWorldMatrixComponent>(enemyEntity);
            if(hitbox.boneNodeIndex >= enemyMatrices.matrices.size())
                return;

            // モデルローカルのボーン姿勢 → ワールド空間。WeaponComponentの手ボーン追従と同じ規約
            // （hlsl++のクォータニオン積は行列積と合成順が逆なので、親を左に置くmul(q_parent, q_local)を使う。
            //   CombatSystem冒頭の武器アタッチ処理のコメント参照）
            hlslpp::float3     boneLocalPos;
            hlslpp::quaternion boneLocalRot;
            Tsukino::Core::Math::matrix::decomposePositionRotation(enemyMatrices.matrices[hitbox.boneNodeIndex], boneLocalPos, boneLocalRot);

            hlslpp::float3     boneWorldPos =
                enemyTransform.position + hlslpp::mul(boneLocalPos * enemyTransform.scale, enemyTransform.rotation);
            hlslpp::quaternion boneWorldRot = hlslpp::mul(enemyTransform.rotation, boneLocalRot);

            hlslpp::float3 hitStart = boneWorldPos + hlslpp::mul(hitbox.boneLocalOffset, boneWorldRot);

            // カプセルモード（endBoneNameが設定されている＝腕の振り抜きなど）なら終点ボーンも
            // 同じ手順で解決する。終点が解決できない場合は設定ミスとして今フレームは判定を出さない
            bool           isCapsuleMode = !hitbox.endBoneName.empty();
            hlslpp::float3 hitEnd        = hitStart;
            if(isCapsuleMode) {
                ResolveBoneNodeIndex(ctx, registry, enemyEntity, hitbox.endBoneName, hitbox.resolvedAgainstModelEnd, hitbox.endBoneNodeIndex);
                if(hitbox.endBoneNodeIndex == UINT32_MAX || hitbox.endBoneNodeIndex >= enemyMatrices.matrices.size())
                    return;

                hlslpp::float3     endBoneLocalPos;
                hlslpp::quaternion endBoneLocalRot;
                Tsukino::Core::Math::matrix::decomposePositionRotation(enemyMatrices.matrices[hitbox.endBoneNodeIndex], endBoneLocalPos, endBoneLocalRot);

                hlslpp::float3     endBoneWorldPos =
                    enemyTransform.position + hlslpp::mul(endBoneLocalPos * enemyTransform.scale, enemyTransform.rotation);
                hlslpp::quaternion endBoneWorldRot = hlslpp::mul(enemyTransform.rotation, endBoneLocalRot);

                hitEnd = endBoneWorldPos + hlslpp::mul(hitbox.endBoneLocalOffset, endBoneWorldRot);
            }

            // スイープ用に前フレームと比較する判定点。球モードは中心（hitStart）、
            // カプセルモードは芯線の遠位端（hitEnd）を使う
            hlslpp::float3 sweepPoint = isCapsuleMode ? hitEnd : hitStart;

            //-------------------------------------------------------------
            // プレイヤーのカプセル芯線。centerOffsetはPhysicsSystemが他のKinematicボディへ適用するのと
            // 同じ規約（transform.rotationで回してtransform.positionへ足す）で組み立てる。
            // プレイヤーはyawのみ回転するため、芯線は常に鉛直のまま
            //-------------------------------------------------------------
            hlslpp::float3 playerCapsuleCenter =
                playerTransform->position + hlslpp::mul(playerController->centerOffset, playerTransform->rotation);
            hlslpp::float3 playerCapsuleTop    = playerCapsuleCenter + hlslpp::float3(0.0f, playerController->halfHeight, 0.0f);
            hlslpp::float3 playerCapsuleBottom = playerCapsuleCenter - hlslpp::float3(0.0f, playerController->halfHeight, 0.0f);

            float combinedRadius = hitbox.radius + playerController->radius;
            float distanceSq;
            if(isCapsuleMode) {
                // 芯線（hitStart→hitEnd）とプレイヤーカプセルの最短距離
                distanceSq = DistanceSqBetweenSegments(hitStart, hitEnd, playerCapsuleBottom, playerCapsuleTop);
                if(hitbox.hasPrevSweepPoint) {
                    // 遠位端の前フレーム位置→今フレーム位置もスイープし、速い振り抜きのすり抜けを防ぐ
                    float sweepDistanceSq = DistanceSqBetweenSegments(hitbox.prevSweepPoint, hitEnd, playerCapsuleBottom, playerCapsuleTop);
                    distanceSq             = std::min(distanceSq, sweepDistanceSq);
                }
            } else if(hitbox.hasPrevSweepPoint) {
                // 前フレーム位置→今フレーム位置の線分でスイープ判定する
                distanceSq = DistanceSqBetweenSegments(hitbox.prevSweepPoint, hitStart, playerCapsuleBottom, playerCapsuleTop);
            } else {
                // 判定窓に入った最初のフレームは前フレーム位置が無いため、点判定にフォールバックする
                distanceSq = DistanceSqPointToSegment(hitStart, playerCapsuleBottom, playerCapsuleTop);
            }

            bool isHit = distanceSq <= combinedRadius * combinedRadius;

#ifdef _DEBUG
            // 判定が当たらない不具合の切り分け用ログ。原因が特定でき次第このブロックごと削除する
            {
                std::ofstream diagFile("diag_enemyhit.txt", std::ios::app);
                if(diagFile) {
                    diagFile << "enemy=" << static_cast<u32>(enemyEntity) << " attackTimer=" << animSet.attackTimer
                             << " distance=" << std::sqrt(distanceSq) << " threshold=" << combinedRadius << " hit=" << (isHit ? 1 : 0)
                             << " invincible=" << (playerInvincible ? 1 : 0) << "\n";
                }
            }
#endif

            hitbox.prevSweepPoint    = sweepPoint;
            hitbox.hasPrevSweepPoint = true;

            // 無敵時間中はすり抜ける。hasLandedThisAttackは立てない＝無敵が明けた後、
            // 同じ判定区間内であれば改めて当たり得る
            if(isHit && !playerInvincible) {
                // 傲慢（スキル）で軽減した後の値を実ダメージとして扱う。
                // 以降のダメージ表示（PlayerDamagedEvent経由）もこの値を使い、
                // 画面に出る数字と実際のHPの減りを一致させる
                const float takenDamage = hitbox.damage * playerDamageTakenMultiplier;

                playerHealth->currentHealth -= takenDamage;
                if(playerHealth->currentHealth <= 0.0f) {
                    playerHealth->currentHealth = 0.0f;
                    playerHealth->isDead         = true;
                }
                hitbox.hasLandedThisAttack = true;

                // 被弾演出（点滅・画面フラッシュ。PlayerDamageEffectSystem）用の通知。
                // ヒットストップは敵を殴ったとき（kHitStopDuration/kHitStopScale）より弱めにする。
                // 画面全体ではなく、プレイヤーと攻撃した敵だけを止める
                if(eventBus) {
                    eventBus->Publish(PlayerDamagedEvent{enemyEntity, playerEntity, takenDamage, sweepPoint});
                }
                ApplyHitStop(registry, playerEntity, kPlayerHitStopDuration, kPlayerHitStopScale);
                ApplyHitStop(registry, enemyEntity, kPlayerHitStopDuration, kPlayerHitStopScale);
            }

#ifdef _DEBUG
            if(ctx->renderer) {
                hlslpp::float4 color = hitbox.hasLandedThisAttack ? hlslpp::float4(1.0f, 0.2f, 0.8f, 1.0f) : hlslpp::float4(0.3f, 0.6f, 1.0f, 1.0f);

                // hitStart→hitEndの芯線からカプセル（球モードはhitStart==hitEndでhalfHeight=0となり
                // DrawWireCapsule側が自動的に球として描画する）を組み立てて可視化する
                hlslpp::float3     debugSegment    = hitEnd - hitStart;
                float              debugSegLen     = hlslpp::length(debugSegment);
                hlslpp::float3     debugCapsuleCenter = (hitStart + hitEnd) * 0.5f;
                hlslpp::quaternion debugCapsuleRot = (debugSegLen > 1e-4f)
                    ? Tsukino::Physics::QuatFromToRotation(hlslpp::float3(0.0f, 1.0f, 0.0f), debugSegment / debugSegLen)
                    : hlslpp::quaternion(0.0f, 0.0f, 0.0f, 1.0f);
                DrawWireCapsule(ctx->renderer, debugCapsuleCenter, debugCapsuleRot, hitbox.radius, debugSegLen * 0.5f, color);
            }
#endif
        });

#ifdef _DEBUG
        // 敵の当たり判定範囲（EnemyComponent::bodyRadius。武器ヒット判定用カプセルの寸法決めに使う参考値）を
        // ワイヤーフレームで可視化する
        if(ctx && ctx->renderer) {
            auto enemyView = registry.View<EnemyComponent, Tsukino::BuiltIn::ECS::TransformComponent, HealthComponent>();
            enemyView.each([&](entt::entity, EnemyComponent& enemy, Tsukino::BuiltIn::ECS::TransformComponent& enemyTransform, HealthComponent& enemyHealth) {
                if(enemyHealth.isDead)
                    return;
                DrawWireCircleXZ(ctx->renderer, enemyTransform.position, enemy.bodyRadius, hlslpp::float4(1.0f, 1.0f, 0.0f, 1.0f));
            });

            Tsukino::Renderer::DrawCommand cmd{};
            cmd.customDraw = [renderer = ctx->renderer](ID3D11DeviceContext*) { renderer->FlushDebugDraw(); };
            ctx->renderer->PushDrawCommand(cmd);
        }
#endif
    }
}    // namespace CombatAndroid::ECS
