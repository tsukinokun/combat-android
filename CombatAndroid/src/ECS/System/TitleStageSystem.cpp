//-------------------------------------------------------------
//! @file   TitleStageSystem.cpp
//! @brief  TitleStageSystemクラスの実装
//-------------------------------------------------------------
#include <CombatAndroid/ECS/System/TitleStageSystem.hpp>
#include <CombatAndroid/ECS/Component/TitleStageComponent.hpp>
#include <CombatAndroid/ECS/Event/SoundEvent.hpp>
#include <CombatAndroid/ECS/Utility/SoundTable.hpp>
#include <CombatAndroid/ECS/Utility/ScreenFade.hpp>
#include <CombatAndroid/Scene/CombatAndroidScene.hpp>

#include <Tsukino/EngineIntegration/EngineContext.hpp>
#include <Tsukino/EngineIntegration/ECS/System/EffectSystem.hpp>
#include <Tsukino/EngineIntegration/Scene/GameSceneManager.hpp>

#include <Tsukino/BuiltIn/ECS/Component/CameraComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/RimGlowComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/TransformComponent.hpp>

#include <Tsukino/Engine/Asset/AssetManager.hpp>

#include <Tsukino/Core/Path.hpp>
#include <Tsukino/Core/Window.hpp>

#include <entt/entt.hpp>

#include <algorithm>
#include <cmath>
#include <memory>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    namespace {
        constexpr float kPi = 3.14159265f;

        //-------------------------------------------------------------
        // 抜ける動き。
        // 立ち上がりを速く、終わり際をゆるめて、最後に少しだけ行き過ぎてから戻す。
        // 行き過ぎ（kRiseOvershoot）が0だと、ただ持ち上がるだけの機械的な動きに見える
        //-------------------------------------------------------------
        constexpr float kRiseDuration  = 0.75f;    //!< 地面から抜けきるまでの秒数
        constexpr float kRiseOvershoot = 0.12f;    //!< 目標の高さを超える割合（0.12＝12%上まで上がって戻る）

        //! 抜ける間に回る量（ラジアン）。1回転半ぶん回してから止まる
        constexpr float kRiseSpinRadians = 3.0f * kPi;

        //-------------------------------------------------------------
        // 浮いてからの漂い
        //-------------------------------------------------------------
        constexpr float kIdleSpinSpeed  = 0.45f;    //!< 浮いている間の回転の速さ（ラジアン/秒）
        constexpr float kBobAmplitude   = 9.0f;     //!< 上下動の振れ幅（ユニット）
        constexpr float kBobFrequency   = 0.55f;    //!< 上下動の速さ（1秒あたりの往復）

        //-------------------------------------------------------------
        // 輪郭の光。抜ける瞬間が一番強く、浮いたあとは控えめに落ち着かせる
        //-------------------------------------------------------------
        const hlslpp::float3 kRimColor        = hlslpp::float3(1.0f, 0.88f, 0.62f);    //!< わずかに暖色を含んだ白
        constexpr float      kRimBurstAmount  = 1.6f;                                   //!< 抜ける瞬間の強さ
        constexpr float      kRimSettledAmount = 0.5f;                                  //!< 浮いてからの強さ
        constexpr float      kRimFadeDuration = 1.2f;                                    //!< 抜けた後、落ち着くまでの秒数

        //-------------------------------------------------------------
        // 抜けた瞬間に足元で弾ける光。土煙専用の素材が無いので、暖色で夕日に馴染む
        // グレートソードのAoEエフェクトを小さくして流用する
        //-------------------------------------------------------------
        constexpr const char* kBurstEffectPath  = "CombatAndroid/Assets/Effect/greatswordAttackCombo3.efkefc";
        constexpr float       kBurstEffectScale = 14.0f;    //!< 足元で弾ける程度の大きさ（戦闘のAoEは100）

        //-------------------------------------------------------------
        // 「はじめる」を選んでからロード画面へ移るまでの見せ場。
        // 真ん中の武器（ウォーハンマー）がカメラへ回転しながら飛んできて、
        // 目の前で弾け、その白い光のままロード画面へ切り替わる
        //-------------------------------------------------------------
        constexpr float kLaunchStagger    = 0.14f;    //!< 1本ごとに飛び出す時刻をずらす量
        constexpr float kLaunchWindUp     = 0.20f;    //!< 溜め（少し奥へ引く）の秒数
        constexpr float kLaunchFlight     = 0.45f;    //!< カメラへ飛んでくる秒数
        constexpr float kLaunchAfterBurst = 0.18f;    //!< 最後の1本が弾けてから暗転を始めるまでの余韻

        //! 3本とも弾け終わって余韻も過ぎる時刻。ここで黒フェード（ScreenFade）を頼む
        constexpr float kLaunchTotal =
            kLaunchStagger * static_cast<float>(kTitleStageWeaponCount - 1) + kLaunchWindUp + kLaunchFlight + kLaunchAfterBurst;

        constexpr float kLaunchWindUpDistance   = 70.0f;    //!< 溜めでカメラから遠ざかる距離
        constexpr float kLaunchSpinRadians      = 7.0f;     //!< 飛ぶ間に回る量（ラジアン）
        constexpr float kLaunchRimAmount        = 1.6f;     //!< 飛んでいる間の輪郭の強さ（上げすぎると白く飛んで金具が見えなくなる）
        constexpr float kLaunchBurstEffectScale = 26.0f;    //!< 目の前で弾けるので、地面の土煙（14）より大きくする

        //-------------------------------------------------------------
        //! @struct LaunchStop
        //! @brief  飛んできた武器が止まる場所。3本が同じ所へ重ならないよう散らす
        //-------------------------------------------------------------
        struct LaunchStop {
            float distance;      //!< カメラの手前これだけの所で止まる
            float sideOffset;    //!< 画面の右方向へのずれ（負なら左）
            float upOffset;      //!< 上方向へのずれ
        };

        //! 武器の並び（WeaponIdの並び）どおり
        const LaunchStop kLaunchStops[kTitleStageWeaponCount] = {
            {100.0f, 0.0f, 6.0f},       // ウォーハンマー：正面
            {118.0f, 62.0f, -14.0f},    // グレートソード：右下
            {88.0f, -58.0f, 18.0f},     // バトルアックス：左上
        };

        //-------------------------------------------------------------
        // カメラの揺らぎ。止まった絵に見えないよう、基準位置の周りをゆっくり往復させる
        //-------------------------------------------------------------
        constexpr float kCameraSwayPeriod = 17.0f;    //!< 一往復にかける秒数
        constexpr float kCameraSwayX      = 55.0f;    //!< 横の振れ幅（ユニット）
        constexpr float kCameraSwayY      = 22.0f;    //!< 縦の振れ幅（ユニット）

        //-------------------------------------------------------------
        //! @brief  0→1を「速く始まってゆるやかに終わる」曲線へ変える
        //! @param  t [in] 0〜1の進み具合
        //! @return 変換後の0〜1
        //-------------------------------------------------------------
        [[nodiscard]]
        float EaseOut(float t) {
            const float inverted = 1.0f - std::clamp(t, 0.0f, 1.0f);
            return 1.0f - inverted * inverted * inverted;
        }
    }    // namespace

    //-------------------------------------------------------------
    //! @brief システムの更新
    //-------------------------------------------------------------
    void TitleStageSystem::Update(Tsukino::ECS::Registry& registry, float deltaTime) {
        auto* ctx = registry.GetContext<Tsukino::EngineIntegration::EngineContext*>();
        if(!ctx)
            return;

        auto view = registry.View<TitleStageComponent>();
        for(entt::entity entity : view) {
            TitleStageComponent& stage = view.get<TitleStageComponent>(entity);

            stage.elapsed += deltaTime;

            //-------------------------------------------------------------
            // 武器。抜ける前は地面の下に沈めておく（地面の板が隠してくれる）
            //-------------------------------------------------------------
            for(int weaponIndex = 0; weaponIndex < kTitleStageWeaponCount; ++weaponIndex) {
                TitleStageWeapon& weapon = stage.weapons[weaponIndex];

                if(weapon.entity == entt::null || !registry.IsValid(weapon.entity))
                    continue;

                // 飛び出した後の位置と姿勢は下のUpdateLaunchが書くので、ここでは触らない
                if(stage.launchRequested)
                    continue;

                auto* transform = registry.try_get<Tsukino::BuiltIn::ECS::TransformComponent>(weapon.entity);
                if(!transform)
                    continue;

                const float sinceBurst = stage.elapsed - weapon.burstTime;

                //-------------------------------------------------------------
                // 抜け始める瞬間に1回だけ、足元で土煙を上げて音を鳴らす
                //-------------------------------------------------------------
                if(!weapon.burst && sinceBurst >= 0.0f) {
                    weapon.burst = true;

                    if(ctx->effectSystem && ctx->assetManager) {
                        const Tsukino::Core::Path   effectPath(kBurstEffectPath);
                        Tsukino::Asset::AssetHandle effectAsset = ctx->assetManager->Load(effectPath);
                        if(effectAsset.IsValid()) {
                            float position[3] = {weapon.groundPosition.x, 0.0f, weapon.groundPosition.z};
                            ctx->effectSystem->PlayEffect(registry, effectAsset, effectPath, position, false, kBurstEffectScale);
                        }
                    }

                    PlaySound(registry, SoundId::Swing);
                }

                //-------------------------------------------------------------
                // 高さ。沈んだ位置から浮かぶ高さへ、行き過ぎてから戻る形で上げる
                //-------------------------------------------------------------
                const float riseT    = std::clamp(sinceBurst / kRiseDuration, 0.0f, 1.0f);
                const float eased    = EaseOut(riseT);
                const float overshoot = std::sin(riseT * kPi) * kRiseOvershoot;

                float height = weapon.groundPosition.y + (weapon.hoverHeight - weapon.groundPosition.y) * (eased + overshoot);

                // 抜けきったら、その場でゆっくり上下に漂わせる
                if(sinceBurst > kRiseDuration) {
                    const float bobT = (stage.elapsed + weapon.bobPhase) * kBobFrequency * 2.0f * kPi;
                    height += std::sin(bobT) * kBobAmplitude;
                }

                transform->position = hlslpp::float3(weapon.groundPosition.x, height, weapon.groundPosition.z);

                //-------------------------------------------------------------
                // 姿勢。刃を下にして刺さっている状態（Z軸に180度）から、
                // 抜ける間にY軸で回しながら上下を戻す
                //-------------------------------------------------------------
                const float flipAngle = kPi * (1.0f - eased);
                const float spinAngle = weapon.spinPhase + kRiseSpinRadians * eased
                                        + std::max(sinceBurst - kRiseDuration, 0.0f) * kIdleSpinSpeed;

                transform->rotation = hlslpp::mul(hlslpp::quaternion::rotation_y(spinAngle), hlslpp::quaternion::rotation_z(flipAngle));
                transform->dirty    = true;

                //-------------------------------------------------------------
                // 輪郭の光。抜けた直後を強くして、そこから落ち着かせる
                //-------------------------------------------------------------
                if(auto* rim = registry.try_get<Tsukino::BuiltIn::ECS::RimGlowComponent>(weapon.entity)) {
                    if(sinceBurst < 0.0f) {
                        rim->active = false;
                    } else {
                        const float fadeT = std::clamp(sinceBurst / kRimFadeDuration, 0.0f, 1.0f);

                        rim->active       = true;
                        rim->rimColor     = kRimColor;
                        rim->rimIntensity = kRimBurstAmount + (kRimSettledAmount - kRimBurstAmount) * fadeT;
                        rim->rimPower     = 2.5f;
                    }
                }
            }

            //-------------------------------------------------------------
            // カメラ。基準位置の周りを横8の字に小さく往復させる
            //-------------------------------------------------------------
            if(stage.cameraEntity != entt::null && registry.IsValid(stage.cameraEntity)) {
                auto* cameraTransform = registry.try_get<Tsukino::BuiltIn::ECS::TransformComponent>(stage.cameraEntity);
                auto* camera          = registry.try_get<Tsukino::BuiltIn::ECS::CameraComponent>(stage.cameraEntity);

                if(cameraTransform && camera) {
                    const float swayT = stage.elapsed / kCameraSwayPeriod * 2.0f * kPi;

                    cameraTransform->position = stage.cameraBasePosition
                                                + hlslpp::float3(std::sin(swayT) * kCameraSwayX, std::sin(swayT * 2.0f) * kCameraSwayY, 0.0f);
                    cameraTransform->dirty = true;

                    camera->useLookAt    = true;
                    camera->lookAtTarget = stage.cameraLookAt;
                    camera->dirty        = true;
                }
            }

            //-------------------------------------------------------------
            // 「はじめる」の見せ場。カメラの位置が今フレームの値に決まった後に進める
            //-------------------------------------------------------------
            if(stage.launchRequested) {
                stage.launchElapsed += deltaTime;
                UpdateLaunch(registry, *ctx, stage);
            }
        }
    }

    //-------------------------------------------------------------
    //! @brief 「はじめる」の見せ場を1フレーム進める
    //-------------------------------------------------------------
    void TitleStageSystem::UpdateLaunch(Tsukino::ECS::Registry& registry, Tsukino::EngineIntegration::EngineContext& context,
                                        TitleStageComponent& stage) {
        //-------------------------------------------------------------
        // カメラの今フレームの位置と向き。武器はこの手前まで飛んでくる
        //-------------------------------------------------------------
        hlslpp::float3 cameraPosition = stage.cameraBasePosition;
        if(stage.cameraEntity != entt::null && registry.IsValid(stage.cameraEntity)) {
            if(auto* cameraTransform = registry.try_get<Tsukino::BuiltIn::ECS::TransformComponent>(stage.cameraEntity))
                cameraPosition = cameraTransform->position;
        }

        const hlslpp::float3 viewDir = hlslpp::normalize(stage.cameraLookAt - cameraPosition);
        const hlslpp::float3 upDir   = hlslpp::float3(0.0f, 1.0f, 0.0f);

        // 画面の右方向。3本を左右へ散らすのに使う
        const hlslpp::float3 rightDir = hlslpp::normalize(hlslpp::cross(upDir, viewDir));

        for(int weaponIndex = 0; weaponIndex < kTitleStageWeaponCount; ++weaponIndex) {
            TitleStageWeapon& weapon = stage.weapons[weaponIndex];
            if(weapon.entity == entt::null || !registry.IsValid(weapon.entity))
                continue;

            auto* transform = registry.try_get<Tsukino::BuiltIn::ECS::TransformComponent>(weapon.entity);
            if(!transform)
                continue;

            // 1本ずつ時間をずらして飛ばす。自分の番が来るまでは漂ったまま待つ
            const float elapsed = stage.launchElapsed - kLaunchStagger * static_cast<float>(weaponIndex);
            if(elapsed < 0.0f)
                continue;

            // 飛び出す前の位置は、自分の番が来た瞬間の（漂っている）位置をそのまま使う
            if(!weapon.launchStarted) {
                weapon.launchStarted = true;
                weapon.launchStart   = transform->position;

                PlaySound(registry, SoundId::Swing);
            }

            const LaunchStop&    stop      = kLaunchStops[weaponIndex];
            const hlslpp::float3 awayDir   = hlslpp::normalize(weapon.launchStart - cameraPosition);
            const hlslpp::float3 windUpEnd = weapon.launchStart + awayDir * kLaunchWindUpDistance;
            const hlslpp::float3 flightEnd =
                cameraPosition + viewDir * stop.distance + rightDir * stop.sideOffset + upDir * stop.upOffset;

            //-------------------------------------------------------------
            // 溜め（奥へ引く）→ 加速してカメラへ、の2段。飛ぶ間は縦にも回して勢いを出す
            //-------------------------------------------------------------
            float spin = weapon.spinPhase;

            if(elapsed < kLaunchWindUp) {
                const float t       = elapsed / kLaunchWindUp;
                transform->position = weapon.launchStart + (windUpEnd - weapon.launchStart) * EaseOut(t);
            } else {
                const float t = std::clamp((elapsed - kLaunchWindUp) / kLaunchFlight, 0.0f, 1.0f);

                // 終わりへ向けて加速させる（等速だとスローに見える）
                const float accelerated = t * t * t;

                transform->position = windUpEnd + (flightEnd - windUpEnd) * accelerated;
                spin += kLaunchSpinRadians * accelerated;

                transform->rotation = hlslpp::mul(hlslpp::quaternion::rotation_y(spin),
                                                  hlslpp::quaternion::rotation_x(kLaunchSpinRadians * accelerated));
            }

            transform->dirty = true;

            // 飛んでいる間は輪郭を光らせる
            if(auto* rim = registry.try_get<Tsukino::BuiltIn::ECS::RimGlowComponent>(weapon.entity)) {
                rim->active   = true;
                rim->rimColor = kRimColor;
                rim->rimIntensity =
                    kRimBurstAmount + (kLaunchRimAmount - kRimBurstAmount) * std::clamp(elapsed / kLaunchWindUp, 0.0f, 1.0f);
                rim->rimPower = 2.0f;
            }

            //-------------------------------------------------------------
            // 目の前で弾ける。エフェクトと音は1本につき1回だけ
            //-------------------------------------------------------------
            if(!weapon.launchBurst && elapsed >= kLaunchWindUp + kLaunchFlight) {
                weapon.launchBurst = true;

                if(context.effectSystem && context.assetManager) {
                    const Tsukino::Core::Path   effectPath(kBurstEffectPath);
                    Tsukino::Asset::AssetHandle effectAsset = context.assetManager->Load(effectPath);
                    if(effectAsset.IsValid()) {
                        float position[3] = {flightEnd.x, flightEnd.y, flightEnd.z};
                        context.effectSystem->PlayEffect(registry, effectAsset, effectPath, position, false, kLaunchBurstEffectScale);
                    }
                }

                PlaySound(registry, SoundId::WeaponEvolve);
            }
        }

        //-------------------------------------------------------------
        // 3本とも弾けて余韻も過ぎたら、黒フェードで戦闘シーンへ渡す（アセットは起動時のロード画面で読み済み）。
        // タイトルから始めたときだけ操作の案内を出す（リトライからは出さない）
        //-------------------------------------------------------------
        if(!stage.launchHandedOff && stage.launchElapsed >= kLaunchTotal) {
            stage.launchHandedOff = true;

            RequestSceneChangeWithFade(registry, []() { return std::make_unique<CombatAndroid::CombatAndroidScene>(true); });
        }
    }
}    // namespace CombatAndroid::ECS
