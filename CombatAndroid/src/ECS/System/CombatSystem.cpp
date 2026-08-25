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
#include <CombatAndroid/ECS/Event/WeaponHitEvent.hpp>
#include <CombatAndroid/ECS/Event/PlayerDamagedEvent.hpp>

#include <Tsukino/BuiltIn/ECS/Component/TransformComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/ModelComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/NodeWorldPoseComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/NodeWorldMatrixComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/CharacterControllerComponent.hpp>

#include <Tsukino/EngineIntegration/EngineContext.hpp>
#include <Tsukino/EngineIntegration/ECS/System/PhysicsSystem.hpp>
#include <Tsukino/Engine/Asset/AssetManager.hpp>
#include <Tsukino/Engine/Asset/Model/ModelAsset.hpp>
#include <Tsukino/GraphicsCommon/Model/ModelData.hpp>
#include <Tsukino/Core/ECS/Event/EventBus.hpp>
#ifdef _DEBUG
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
        // ヒットストップの調整用定数（実機で見た目を確認しながら調整する）
        //-------------------------------------------------------------
        constexpr float kHitStopDuration = 0.12f;    //!< ヒット時にかかる減速の持続時間（実時間・秒）
        constexpr float kHitStopScale    = 0.02f;    //!< 持続時間中のdeltaTimeへのスケール値（小さいほど強い停止）

        // プレイヤーが被弾したときのヒットストップは、敵を殴ったとき（上の2定数）より弱めにする。
        // プレイヤー操作が止まる時間を短くし、被弾直後にすぐ回避・反撃できるようにするため
        constexpr float kPlayerHitStopDuration = 0.08f;
        constexpr float kPlayerHitStopScale    = 0.15f;

        constexpr float kHpBarVisibleDuration = 3.0f;    //!< 被弾時に頭上HPバーを表示し続ける時間（秒）。HealthBarSystemが減算する

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

#ifdef _DEBUG
        //-------------------------------------------------------------
        //! @brief  当たり判定範囲を目視確認できるよう、XZ平面上の円をワイヤーフレームで描画する
        //-------------------------------------------------------------
        void DrawWireCircleXZ(Tsukino::Renderer::Renderer* renderer, const hlslpp::float3& center, float radius, const hlslpp::float4& color) {
            constexpr int segments = 24;
            Tsukino::GraphicsCommon::DebugVertex prev{
                {center.x + radius, center.y, center.z},
                {color.x, color.y, color.z, color.w}
            };
            for(int i = 1; i <= segments; ++i) {
                float angle = (2.0f * 3.14159265f) * (static_cast<float>(i) / static_cast<float>(segments));
                Tsukino::GraphicsCommon::DebugVertex next{
                    {center.x + std::cos(angle) * radius, center.y, center.z + std::sin(angle) * radius},
                    {color.x, color.y, color.z, color.w}
                };
                renderer->DrawDebugLine(prev, next);
                prev = next;
            }
        }
#endif
    }    // namespace

    //-------------------------------------------------------------
    //! @brief システムの更新
    //-------------------------------------------------------------
    void CombatSystem::Update(Tsukino::ECS::Registry& registry, float deltaTime) {
        auto* ctx      = registry.GetContext<Tsukino::EngineIntegration::EngineContext*>();
        auto* eventBus = registry.GetContext<Tsukino::ECS::EventBus*>();

        //-------------------------------------------------------------
        // 武器：所有者への追従、タイマー更新、攻撃発生時のカプセルオーバーラップ判定によるダメージ
        //-------------------------------------------------------------
        auto weaponView = registry.View<WeaponComponent, Tsukino::BuiltIn::ECS::TransformComponent>();
        weaponView.each([&](entt::entity entity, WeaponComponent& weapon, Tsukino::BuiltIn::ECS::TransformComponent& transform) {
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
                hlslpp::quaternion gripRotOffset = hlslpp::slerp(weapon.gripRotationOffset, weapon.attackGripRotationOffset, weapon.attackBlend);

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
                        hlslpp::quaternion worldRot = hlslpp::slerp(ownerTransform.rotation, handWorldRot, trackingWeight);

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
                    targetRotation = hlslpp::slerp(floatRotation, targetRotation, weapon.attackBlend);

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
                    transform.rotation   = hlslpp::slerp(transform.rotation, targetRotation, rotationLerpT);
                }
                transform.dirty = true;

#ifdef _DEBUG
                // 当たり判定カプセルの足跡（グリップ側・刃先側の断面円）を可視化する。
                // 当たり判定有効中は赤、それ以外はシアンにする
                if(ctx && ctx->renderer) {
                    hlslpp::float3 bladeDir = hlslpp::mul(hlslpp::float3(0.0f, 1.0f, 0.0f), transform.rotation);
                    hlslpp::float3 tipPos   = transform.position + bladeDir * weapon.range;
                    hlslpp::float4 color =
                        weapon.isActive ? hlslpp::float4(1.0f, 0.2f, 0.0f, 1.0f) : hlslpp::float4(0.0f, 1.0f, 1.0f, 1.0f);
                    DrawWireCircleXZ(ctx->renderer, transform.position, weapon.hitCapsuleRadius, color);
                    DrawWireCircleXZ(ctx->renderer, tipPos, weapon.hitCapsuleRadius, color);
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
            }

            if(weapon.isActive) {
                // 当たり判定が有効な間、毎フレーム武器の「今の」姿勢（スイング追従済みのtransform）を基準に
                // グリップ(transform.position)から刃の向き（ローカルY軸）へrangeだけ伸びるカプセルを構築し、
                // Jolt物理へオーバーラップ問い合わせする（PhysicsSystem::OverlapCapsule）。
                // 1回のアタックで同じ敵に何度も当たらないよう、既にヒットした敵はhitEnemiesThisAttackに記録してスキップする
                if(ctx && ctx->physicsSystem) {
                    //-------------------------------------------------------------
                    // 憤怒（スキル）の攻撃力倍率。持ち主から引く。
                    // ヒットごとではなく1回のオーバーラップ判定につき1回だけ引けばよいので、
                    // 下のヒットループの外で求めておく
                    //-------------------------------------------------------------
                    float skillAttackMultiplier = 1.0f;
                    if(weapon.owner != entt::null) {
                        if(auto* ownerSkills = registry.try_get<PlayerSkillComponent>(weapon.owner))
                            skillAttackMultiplier = ownerSkills->attackMultiplier;
                    }

                    hlslpp::float3 bladeDir      = hlslpp::mul(hlslpp::float3(0.0f, 1.0f, 0.0f), transform.rotation);
                    float          halfLen       = weapon.range * 0.5f;
                    hlslpp::float3 capsuleCenter = transform.position + bladeDir * halfLen;

                    std::vector<entt::entity> overlapping =
                        ctx->physicsSystem->OverlapCapsule(capsuleCenter, transform.rotation, weapon.hitCapsuleRadius, halfLen);

                    for(entt::entity hitEntity : overlapping) {
                        if(!registry.HasComponent<EnemyComponent>(hitEntity) || !registry.HasComponent<HealthComponent>(hitEntity))
                            continue;

                        auto& enemy       = registry.GetComponent<EnemyComponent>(hitEntity);
                        auto& enemyHealth = registry.GetComponent<HealthComponent>(hitEntity);
                        if(enemyHealth.isDead)
                            continue;

                        if(std::find(weapon.hitEnemiesThisAttack.begin(), weapon.hitEnemiesThisAttack.end(), hitEntity)
                           != weapon.hitEnemiesThisAttack.end())
                            continue;

                        // 実ダメージ＝武器の基礎ダメージ×連撃段の倍率（PlayerAnimationSystemが段ごとに書く）
                        //             ×スキル「憤怒」の攻撃力倍率
                        float dealtDamage = weapon.damage * weapon.damageMultiplier * skillAttackMultiplier;

                        enemyHealth.currentHealth -= dealtDamage;
                        if(enemyHealth.currentHealth <= 0.0f) {
                            enemyHealth.currentHealth = 0.0f;
                            enemyHealth.isDead         = true;
                        }
                        enemyHealth.hpBarVisibleTimer = kHpBarVisibleDuration;    // 被弾した瞬間だけ頭上HPバーを表示する
                        weapon.hitEnemiesThisAttack.push_back(hitEntity);

                        // 一定以上の単発ダメージでノックバックを要求する（BTのPlayKnockbackが消費する）。
                        // 既に硬直中なら再要求しない＝連撃で仰け反り続けるハメを防ぐ
                        if(!enemy.isKnockedBack && dealtDamage >= enemy.knockbackDamageThreshold)
                            enemy.pendingKnockback = true;

                        hlslpp::float3 hitPosition = capsuleCenter;
                        if(registry.HasComponent<Tsukino::BuiltIn::ECS::TransformComponent>(hitEntity))
                            hitPosition = registry.GetComponent<Tsukino::BuiltIn::ECS::TransformComponent>(hitEntity).position;

                        // ヒット通知（エフェクト・SE等の副作用処理用）を発火する
                        if(eventBus) {
                            eventBus->Publish(WeaponHitEvent{weapon.owner, entity, hitEntity, hitPosition, dealtDamage});
                        }

                        // ヒットストップを要求する（同一フレームで複数ヒットしても同じ値で上書きされるだけで問題ない）
                        ctx->hitStopTimer = kHitStopDuration;
                        ctx->hitStopScale = kHitStopScale;
                    }
                }

                weapon.activeTimer -= deltaTime;
                if(weapon.activeTimer <= 0.0f) {
                    weapon.activeTimer = 0.0f;
                    weapon.isActive    = false;
                }
            }
        });

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
        if(playerEntity != entt::null) {
            playerHealth     = &registry.GetComponent<HealthComponent>(playerEntity);
            playerTransform  = &registry.GetComponent<Tsukino::BuiltIn::ECS::TransformComponent>(playerEntity);
            playerController = &registry.GetComponent<Tsukino::BuiltIn::ECS::CharacterControllerComponent>(playerEntity);
            playerInvincible = registry.GetComponent<PlayerComponent>(playerEntity).isInvincible;
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
        // 直接カプセルの芯線を組み立てて幾何判定する。併せて、前フレームの手位置→今フレームの
        // 手位置の線分でスイープ判定し、速い振りやフレーム落ちで判定をすり抜けるのも防ぐ
        // （EnemyAttackHitboxComponent::prevHandPosition）
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

            ResolveBoneNodeIndex(ctx, registry, enemyEntity, hitbox.handBoneName, hitbox.resolvedAgainstModel, hitbox.handBoneNodeIndex);
            if(hitbox.handBoneNodeIndex == UINT32_MAX
               || !registry.HasComponent<Tsukino::BuiltIn::ECS::NodeWorldMatrixComponent>(enemyEntity))
                return;

            auto& enemyMatrices = registry.GetComponent<Tsukino::BuiltIn::ECS::NodeWorldMatrixComponent>(enemyEntity);
            if(hitbox.handBoneNodeIndex >= enemyMatrices.matrices.size())
                return;

            // モデルローカルのボーン姿勢 → ワールド空間。WeaponComponentの手ボーン追従と同じ規約
            // （hlsl++のクォータニオン積は行列積と合成順が逆なので、親を左に置くmul(q_parent, q_local)を使う。
            //   CombatSystem冒頭の武器アタッチ処理のコメント参照）
            hlslpp::float3     boneLocalPos;
            hlslpp::quaternion boneLocalRot;
            Tsukino::Core::Math::matrix::decomposePositionRotation(enemyMatrices.matrices[hitbox.handBoneNodeIndex], boneLocalPos, boneLocalRot);

            hlslpp::float3     boneWorldPos =
                enemyTransform.position + hlslpp::mul(boneLocalPos * enemyTransform.scale, enemyTransform.rotation);
            hlslpp::quaternion boneWorldRot = hlslpp::mul(enemyTransform.rotation, boneLocalRot);

            hlslpp::float3 hitCenter = boneWorldPos + hlslpp::mul(hitbox.boneLocalOffset, boneWorldRot);

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
            if(hitbox.hasPrevHandPosition) {
                // 前フレーム位置→今フレーム位置の線分でスイープ判定する
                distanceSq = DistanceSqBetweenSegments(hitbox.prevHandPosition, hitCenter, playerCapsuleBottom, playerCapsuleTop);
            } else {
                // 判定窓に入った最初のフレームは前フレーム位置が無いため、点判定にフォールバックする
                distanceSq = DistanceSqPointToSegment(hitCenter, playerCapsuleBottom, playerCapsuleTop);
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

            hitbox.prevHandPosition    = hitCenter;
            hitbox.hasPrevHandPosition = true;

            // 無敵時間中はすり抜ける。hasLandedThisAttackは立てない＝無敵が明けた後、
            // 同じ判定区間内であれば改めて当たり得る
            if(isHit && !playerInvincible) {
                playerHealth->currentHealth -= hitbox.damage;
                if(playerHealth->currentHealth <= 0.0f) {
                    playerHealth->currentHealth = 0.0f;
                    playerHealth->isDead         = true;
                }
                hitbox.hasLandedThisAttack = true;

                // 被弾演出（点滅・画面フラッシュ。PlayerDamageEffectSystem）用の通知。
                // ヒットストップは敵を殴ったとき（kHitStopDuration/kHitStopScale）より弱めにする
                if(eventBus) {
                    eventBus->Publish(PlayerDamagedEvent{enemyEntity, playerEntity, hitbox.damage, hitCenter});
                }
                ctx->hitStopTimer = kPlayerHitStopDuration;
                ctx->hitStopScale = kPlayerHitStopScale;
            }

#ifdef _DEBUG
            if(ctx->renderer) {
                hlslpp::float4 color = hitbox.hasLandedThisAttack ? hlslpp::float4(1.0f, 0.2f, 0.8f, 1.0f) : hlslpp::float4(0.3f, 0.6f, 1.0f, 1.0f);
                DrawWireCircleXZ(ctx->renderer, hitCenter, hitbox.radius, color);
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
