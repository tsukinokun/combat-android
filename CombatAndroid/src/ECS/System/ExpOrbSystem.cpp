//-------------------------------------------------------------
//! @file   ExpOrbSystem.cpp
//! @brief  ExpOrbSystemクラスの実装
//-------------------------------------------------------------
#include <CombatAndroid/ECS/System/ExpOrbSystem.hpp>
#include <CombatAndroid/ECS/Component/ExpOrbComponent.hpp>
#include <CombatAndroid/ECS/Component/PlayerComponent.hpp>
#include <CombatAndroid/ECS/Component/PlayerExperienceComponent.hpp>

#include <Tsukino/BuiltIn/ECS/Component/TransformComponent.hpp>

#include <hlsl++.h>
#include <entt/entt.hpp>

#include <algorithm>
#include <cmath>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    namespace {
        // 落下（ポップアウト）演出のチューニング値
        constexpr float kSpawnHeightAboveGround = 70.0f;     //!< 死亡位置からどれだけ上に出現させるか
        constexpr float kRestHeightAboveGround  = 20.0f;     //!< 着地後に浮いていさせる高さ
        constexpr float kPopUpSpeedMin          = 220.0f;    //!< 出現時の上向き初速（下限）
        constexpr float kPopUpSpeedMax          = 320.0f;    //!< 出現時の上向き初速（上限）
        constexpr float kHorizontalSpeedMin     = 60.0f;     //!< 出現時の水平方向初速（下限）。複数の玉が重ならないよう散らす
        constexpr float kHorizontalSpeedMax     = 160.0f;    //!< 出現時の水平方向初速（上限）
        constexpr float kGravity                = 900.0f;    //!< 落下中の重力加速度
        constexpr float kFallSafetyDuration     = 1.2f;      //!< 万一着地判定に乗らなかった場合の保険（この秒数でHomingへ強制遷移）
        constexpr float kFallPopInDuration       = 0.12f;     //!< 出現直後、scaleが0から基準値まで膨らむ時間

        // ホーミング（プレイヤーへの吸い寄せ）演出のチューニング値
        constexpr float kHomingSpeedStart    = 250.0f;    //!< 吸い寄せ開始時の速度
        constexpr float kHomingSpeedEnd      = 900.0f;    //!< 吸い寄せが十分進んだ時点の速度（加速していく）
        constexpr float kHomingAccelDuration = 0.6f;      //!< 開始速度→終端速度まで加速しきるまでの時間
        constexpr float kHomingMaxDuration   = 1.5f;      //!< 万一プレイヤーへ追いつけない場合の保険（この秒数で強制吸収）
        constexpr float kAbsorbDistance      = 40.0f;     //!< プレイヤーとの距離がこれ未満になったら吸収する
        constexpr float kAbsorbHeightOffset  = 90.0f;     //!< 吸い寄せ先（プレイヤーの足元原点からの高さ）

        // 見た目のスケール（TransformComponent.scale。SpriteSpace::Worldではワールド単位＝cm換算になる。
        // テクスチャは32x32なので0.75で直径24cm相当）
        constexpr float kOrbBaseScale        = 0.75f;    //!< 通常時の基準スケール（直径約24cm）
        constexpr float kOrbHomingScaleEnd   = 1.0f;      //!< ホーミング終盤（吸収直前）のスケール（直径約32cm）。少し大きくして「寄ってくる」感を出す

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
    //! @brief EnemyDiedEventの購読を開始する
    //-------------------------------------------------------------
    void ExpOrbSystem::Initialize(Tsukino::ECS::EventBus& eventBus) {
        m_pending.reserve(kExpOrbPoolSize);
        m_diedConnection = eventBus.Subscribe<EnemyDiedEvent>([this](const EnemyDiedEvent& event) { OnEnemyDied(event); });
    }

    //-------------------------------------------------------------
    //! @brief 死亡通知のハンドラ
    //-------------------------------------------------------------
    void ExpOrbSystem::OnEnemyDied(const EnemyDiedEvent& event) {
        m_pending.push_back(event);
    }

    //-------------------------------------------------------------
    //! @brief システムの更新
    //-------------------------------------------------------------
    void ExpOrbSystem::Update(Tsukino::ECS::Registry& registry, float deltaTime) {
        auto view = registry.View<ExpOrbComponent, Tsukino::BuiltIn::ECS::TransformComponent>();

        //-------------------------------------------------------------
        // 保留中の死亡通知をスロットへ割り当てる
        //-------------------------------------------------------------
        for(const EnemyDiedEvent& pending : m_pending) {
            //-------------------------------------------------------------
            // 空きスロットを探す。全て使用中なら現在の状態に最も長く留まっている
            // （＝最も古いと見なせる）ものを再利用する
            //-------------------------------------------------------------
            entt::entity slotEntity = entt::null;
            float        oldest     = -1.0f;

            for(entt::entity entity : view) {
                const ExpOrbComponent& orb = view.get<ExpOrbComponent>(entity);

                if(!orb.active) {
                    slotEntity = entity;
                    break;
                }

                if(orb.stateTimer > oldest) {
                    oldest     = orb.stateTimer;
                    slotEntity = entity;
                }
            }

            if(slotEntity == entt::null)
                continue;    // プールが1つも用意されていない（シーン側の生成漏れ）

            ExpOrbComponent& orb = view.get<ExpOrbComponent>(slotEntity);

            //-------------------------------------------------------------
            // 出現直後：死亡位置から少し上に飛び出し、あとは重力に任せて落ちる。
            // 複数体まとめて倒したときに玉同士が重ならないよう、水平方向へランダムに散らす
            //-------------------------------------------------------------
            std::uniform_real_distribution<float> angleDist(0.0f, 6.2831853f);
            std::uniform_real_distribution<float> horizontalSpeedDist(kHorizontalSpeedMin, kHorizontalSpeedMax);
            std::uniform_real_distribution<float> popUpSpeedDist(kPopUpSpeedMin, kPopUpSpeedMax);

            float angle           = angleDist(m_rng);
            float horizontalSpeed = horizontalSpeedDist(m_rng);

            orb.active       = true;
            orb.state        = ExpOrbState::Falling;
            orb.expValue     = pending.expReward;
            orb.stateTimer   = 0.0f;
            orb.worldPosition = pending.position + hlslpp::float3(0.0f, kSpawnHeightAboveGround, 0.0f);
            orb.velocity      = hlslpp::float3(std::cos(angle) * horizontalSpeed, popUpSpeedDist(m_rng), std::sin(angle) * horizontalSpeed);
            orb.groundY       = pending.position.y + kRestHeightAboveGround;
        }
        m_pending.clear();

        //-------------------------------------------------------------
        // プレイヤーを特定する（単一プレイヤー前提。PickupSystemと同じ流儀）
        //-------------------------------------------------------------
        entt::entity                                playerEntity = entt::null;
        hlslpp::float3                               playerPosition{0.0f, 0.0f, 0.0f};
        PlayerExperienceComponent*                   playerExp    = nullptr;

        auto playerView = registry.View<PlayerComponent, PlayerExperienceComponent, Tsukino::BuiltIn::ECS::TransformComponent>();
        for(auto entity : playerView) {
            playerEntity   = entity;
            playerExp      = &playerView.get<PlayerExperienceComponent>(entity);
            playerPosition = playerView.get<Tsukino::BuiltIn::ECS::TransformComponent>(entity).position;
            break;
        }

        //-------------------------------------------------------------
        // 表示中のスロットを進める
        //-------------------------------------------------------------
        view.each([&](entt::entity, ExpOrbComponent& orb, Tsukino::BuiltIn::ECS::TransformComponent& transform) {
            if(!orb.active)
                return;

            orb.stateTimer += deltaTime;

            if(orb.state == ExpOrbState::Falling) {
                orb.velocity.y -= kGravity * deltaTime;
                orb.worldPosition += orb.velocity * deltaTime;

                bool grounded = orb.worldPosition.y <= orb.groundY;
                bool timedOut = orb.stateTimer >= kFallSafetyDuration;
                if(grounded || timedOut) {
                    orb.worldPosition.y = orb.groundY;
                    orb.state           = ExpOrbState::Homing;
                    orb.stateTimer      = 0.0f;
                }
            } else if(orb.state == ExpOrbState::Homing) {
                if(playerEntity != entt::null) {
                    hlslpp::float3 targetPosition = playerPosition + hlslpp::float3(0.0f, kAbsorbHeightOffset, 0.0f);
                    hlslpp::float3 toTarget        = targetPosition - orb.worldPosition;
                    float          distance        = hlslpp::length(toTarget);

                    // 開始はゆっくり、時間が経つほど吸い込まれる速度が増していく（加速イージング）
                    float speedT = SmoothStep01(orb.stateTimer / kHomingAccelDuration);
                    float speed  = kHomingSpeedStart + (kHomingSpeedEnd - kHomingSpeedStart) * speedT;

                    if(distance > 0.001f) {
                        hlslpp::float3 direction = toTarget / distance;
                        float          moveDist  = std::min(distance, speed * deltaTime);
                        orb.worldPosition += direction * moveDist;
                    }

                    bool reachedPlayer = distance <= kAbsorbDistance;
                    bool timedOut       = orb.stateTimer >= kHomingMaxDuration;
                    if(reachedPlayer || timedOut)
                        orb.state = ExpOrbState::Absorbed;
                } else {
                    // プレイヤーが見つからない（想定外）場合は、その場に留まり続ける
                }
            }

            if(orb.state == ExpOrbState::Absorbed) {
                if(playerExp)
                    playerExp->currentExp = std::min(playerExp->currentExp + orb.expValue, playerExp->requiredExp);
                // 【将来のレベルアップ実装用フック】ここでcurrentExp >= requiredExpを判定し、
                // レベルを進めてスキル選択へ入る形になる予定（今回はキャップするだけで頭打ち）

                orb.active      = false;
                transform.scale = hlslpp::float3(0.0f, 0.0f, 0.0f);
                transform.dirty = true;
                return;
            }

            //-------------------------------------------------------------
            // 見た目の反映。SpriteSpace::World（SpriteRenderSystem）はTransformComponent.positionを
            // そのまま3Dワールド座標として使い、主カメラを向くビルボードとして深度テスト付きで描画するため、
            // ここではposition/scaleを直接書けばよい（WorldAnchorSystem経由のスクリーン座標変換は不要）
            //-------------------------------------------------------------
            transform.position = orb.worldPosition;

            float scale;
            if(orb.state == ExpOrbState::Falling) {
                scale = kOrbBaseScale * SmoothStep01(orb.stateTimer / kFallPopInDuration);
            } else {
                float homingT = std::clamp(orb.stateTimer / kHomingMaxDuration, 0.0f, 1.0f);
                scale         = kOrbBaseScale + (kOrbHomingScaleEnd - kOrbBaseScale) * homingT;
            }

            transform.scale = hlslpp::float3(scale, scale, 1.0f);
            transform.dirty = true;
        });
    }
}    // namespace CombatAndroid::ECS
