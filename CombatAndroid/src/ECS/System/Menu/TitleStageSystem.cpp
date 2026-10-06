//-------------------------------------------------------------
//! @file   TitleStageSystem.cpp
//! @brief  TitleStageSystemクラスの実装
//-------------------------------------------------------------
#include <CombatAndroid/ECS/System/Menu/TitleStageSystem.hpp>
#include <CombatAndroid/ECS/Component/Menu/TitleStageComponent.hpp>
#include <CombatAndroid/ECS/Event/Audio/SoundEvent.hpp>
#include <CombatAndroid/ECS/Serialization/Common/SerializationHelper.hpp>
#include <CombatAndroid/ECS/Utility/Table/SoundTable.hpp>
#include <CombatAndroid/ECS/Utility/UI/ScreenFade.hpp>
#include <CombatAndroid/ECS/Utility/Table/TableJson.hpp>
#include <CombatAndroid/Scene/CombatAndroidScene.hpp>

#include <Tsukino/EngineIntegration/EngineContext.hpp>
#include <Tsukino/EngineIntegration/ECS/System/EffectSystem.hpp>
#include <Tsukino/EngineIntegration/Scene/GameSceneManager.hpp>

#include <Tsukino/BuiltIn/ECS/Component/CameraComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/RimGlowComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/TransformComponent.hpp>

#include <Tsukino/Engine/Asset/AssetManager.hpp>

#include <Tsukino/Core/Math/Serialization/HlslppSerialization.hpp>
#include <Tsukino/Core/Path.hpp>
#include <Tsukino/Core/Window.hpp>

#include <cereal/types/array.hpp>
#include <entt/entt.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    namespace {
        constexpr float kPi = 3.14159265f;

        //-------------------------------------------------------------
        // 抜けた瞬間に足元で弾ける光。土煙専用の素材が無いので、暖色で夕日に馴染む
        // グレートソードのAoEエフェクトを小さくして流用する。
        // パスはロード画面の先読み（AssetPreloader.cpp）と揃えるのでコードに残す
        //-------------------------------------------------------------
        constexpr const char* kBurstEffectPath = "CombatAndroid/Assets/Effect/greatswordAttackCombo3.efkefc";

        //-------------------------------------------------------------
        //! @struct LaunchStop
        //! @brief  飛んできた武器が止まる場所。3本が同じ所へ重ならないよう散らす
        //-------------------------------------------------------------
        struct LaunchStop {
            float distance   = 100.0f;    //!< カメラの手前これだけの所で止まる
            float sideOffset = 0.0f;      //!< 画面の右方向へのずれ（負なら左）
            float upOffset   = 0.0f;      //!< 上方向へのずれ
        };

        template <class Archive>
        void load(Archive& archive, LaunchStop& stop) {
            LoadField(archive, "distance", stop.distance);
            LoadField(archive, "sideOffset", stop.sideOffset);
            LoadField(archive, "upOffset", stop.upOffset);
        }

        //-------------------------------------------------------------
        //! @struct TitleStageParams
        //! @brief  演出のチューニング値（Assets/Tables/Systems/TitleStage.json。ここの初期値はJSONにキーが無いときの既定値）
        //-------------------------------------------------------------
        struct TitleStageParams {
            //-------------------------------------------------------------
            // 抜ける動き。
            // 立ち上がりを速く、終わり際をゆるめて、最後に少しだけ行き過ぎてから戻す。
            // 行き過ぎ（riseOvershoot）が0だと、ただ持ち上がるだけの機械的な動きに見える
            //-------------------------------------------------------------
            float riseDuration  = 0.75f;    //!< 地面から抜けきるまでの秒数
            float riseOvershoot = 0.12f;    //!< 目標の高さを超える割合（0.12＝12%上まで上がって戻る）
            float riseSpinTurns = 1.5f;     //!< 抜ける間に回る量（回転数）。1回転半ぶん回してから止まる

            // 浮いてからの漂い
            float idleSpinSpeed = 0.45f;    //!< 浮いている間の回転の速さ（ラジアン/秒）
            float bobAmplitude  = 9.0f;     //!< 上下動の振れ幅（ユニット）
            float bobFrequency  = 0.55f;    //!< 上下動の速さ（1秒あたりの往復）

            // 輪郭の光。抜ける瞬間が一番強く、浮いたあとは控えめに落ち着かせる
            hlslpp::float3 rimColor          = hlslpp::float3(1.0f, 0.88f, 0.62f);    //!< わずかに暖色を含んだ白
            float          rimBurstAmount    = 1.6f;                                   //!< 抜ける瞬間の強さ
            float          rimSettledAmount  = 0.5f;                                   //!< 浮いてからの強さ
            float          rimFadeDuration   = 1.2f;                                   //!< 抜けた後、落ち着くまでの秒数
            float          rimPower          = 2.5f;                                   //!< 浮いている間の輪郭の鋭さ

            float burstEffectScale = 14.0f;    //!< 足元で弾ける光の大きさ（戦闘のAoEは100）

            //-------------------------------------------------------------
            // 「はじめる」を選んでからロード画面へ移るまでの見せ場。
            // 真ん中の武器（ウォーハンマー）がカメラへ回転しながら飛んできて、
            // 目の前で弾け、その白い光のままロード画面へ切り替わる
            //-------------------------------------------------------------
            float launchStagger    = 0.14f;    //!< 1本ごとに飛び出す時刻をずらす量
            float launchWindUp     = 0.20f;    //!< 溜め（少し奥へ引く）の秒数
            float launchFlight     = 0.45f;    //!< カメラへ飛んでくる秒数
            float launchAfterBurst = 0.18f;    //!< 最後の1本が弾けてから暗転を始めるまでの余韻

            float launchWindUpDistance   = 70.0f;    //!< 溜めでカメラから遠ざかる距離
            float launchSpinRadians      = 7.0f;     //!< 飛ぶ間に回る量（ラジアン）
            float launchRimAmount        = 1.6f;     //!< 飛んでいる間の輪郭の強さ（上げすぎると白く飛んで金具が見えなくなる）
            float launchRimPower         = 2.0f;     //!< 飛んでいる間の輪郭の鋭さ
            float launchBurstEffectScale = 26.0f;    //!< 目の前で弾けるので、地面の土煙（14）より大きくする

            //! 飛んできた武器が止まる場所。武器の並び（WeaponIdの並び）どおり
            std::array<LaunchStop, kTitleStageWeaponCount> launchStops = {{
                {100.0f, 0.0f, 6.0f},       // ウォーハンマー：正面
                {118.0f, 62.0f, -14.0f},    // グレートソード：右下
                {88.0f, -58.0f, 18.0f},     // バトルアックス：左上
            }};

            // カメラの揺らぎ。止まった絵に見えないよう、基準位置の周りをゆっくり往復させる
            float cameraSwayPeriod = 17.0f;    //!< 一往復にかける秒数
            float cameraSwayX      = 55.0f;    //!< 横の振れ幅（ユニット）
            float cameraSwayY      = 22.0f;    //!< 縦の振れ幅（ユニット）
        };

        template <class Archive>
        void load(Archive& archive, TitleStageParams& params) {
            LoadField(archive, "riseDuration", params.riseDuration);
            LoadField(archive, "riseOvershoot", params.riseOvershoot);
            LoadField(archive, "riseSpinTurns", params.riseSpinTurns);
            LoadField(archive, "idleSpinSpeed", params.idleSpinSpeed);
            LoadField(archive, "bobAmplitude", params.bobAmplitude);
            LoadField(archive, "bobFrequency", params.bobFrequency);
            LoadField(archive, "rimColor", params.rimColor);
            LoadField(archive, "rimBurstAmount", params.rimBurstAmount);
            LoadField(archive, "rimSettledAmount", params.rimSettledAmount);
            LoadField(archive, "rimFadeDuration", params.rimFadeDuration);
            LoadField(archive, "rimPower", params.rimPower);
            LoadField(archive, "burstEffectScale", params.burstEffectScale);
            LoadField(archive, "launchStagger", params.launchStagger);
            LoadField(archive, "launchWindUp", params.launchWindUp);
            LoadField(archive, "launchFlight", params.launchFlight);
            LoadField(archive, "launchAfterBurst", params.launchAfterBurst);
            LoadField(archive, "launchWindUpDistance", params.launchWindUpDistance);
            LoadField(archive, "launchSpinRadians", params.launchSpinRadians);
            LoadField(archive, "launchRimAmount", params.launchRimAmount);
            LoadField(archive, "launchRimPower", params.launchRimPower);
            LoadField(archive, "launchBurstEffectScale", params.launchBurstEffectScale);
            LoadField(archive, "launchStops", params.launchStops);
            LoadField(archive, "cameraSwayPeriod", params.cameraSwayPeriod);
            LoadField(archive, "cameraSwayX", params.cameraSwayX);
            LoadField(archive, "cameraSwayY", params.cameraSwayY);
        }

        //-------------------------------------------------------------
        //! @brief  チューニング値を得る関数（初回の呼び出しで1度だけ読む）
        //-------------------------------------------------------------
        const TitleStageParams& GetParams() {
            static const TitleStageParams s_params = LoadSystemParams<TitleStageParams>("TitleStage");
            return s_params;
        }

        //-------------------------------------------------------------
        //! @brief  3本とも弾け終わって余韻も過ぎる時刻を求める。ここで黒フェード（ScreenFade）を頼む
        //! @param  params [in] チューニング値
        //-------------------------------------------------------------
        [[nodiscard]]
        float CalculateLaunchTotal(const TitleStageParams& params) {
            return params.launchStagger * static_cast<float>(kTitleStageWeaponCount - 1) + params.launchWindUp + params.launchFlight
                   + params.launchAfterBurst;
        }

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

        const TitleStageParams& params = GetParams();

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
                            ctx->effectSystem->PlayEffect(registry, effectAsset, effectPath, position, false, params.burstEffectScale);
                        }
                    }

                    PlaySound(registry, SoundId::Swing);
                }

                //-------------------------------------------------------------
                // 高さ。沈んだ位置から浮かぶ高さへ、行き過ぎてから戻る形で上げる
                //-------------------------------------------------------------
                const float riseT    = std::clamp(sinceBurst / params.riseDuration, 0.0f, 1.0f);
                const float eased    = EaseOut(riseT);
                const float overshoot = std::sin(riseT * kPi) * params.riseOvershoot;

                float height = weapon.groundPosition.y + (weapon.hoverHeight - weapon.groundPosition.y) * (eased + overshoot);

                // 抜けきったら、その場でゆっくり上下に漂わせる
                if(sinceBurst > params.riseDuration) {
                    const float bobT = (stage.elapsed + weapon.bobPhase) * params.bobFrequency * 2.0f * kPi;
                    height += std::sin(bobT) * params.bobAmplitude;
                }

                transform->position = hlslpp::float3(weapon.groundPosition.x, height, weapon.groundPosition.z);

                //-------------------------------------------------------------
                // 姿勢。刃を下にして刺さっている状態（Z軸に180度）から、
                // 抜ける間にY軸で回しながら上下を戻す
                //-------------------------------------------------------------
                const float flipAngle = kPi * (1.0f - eased);
                const float spinAngle = weapon.spinPhase + (params.riseSpinTurns * 2.0f * kPi) * eased
                                        + std::max(sinceBurst - params.riseDuration, 0.0f) * params.idleSpinSpeed;

                transform->rotation = hlslpp::mul(hlslpp::quaternion::rotation_y(spinAngle), hlslpp::quaternion::rotation_z(flipAngle));
                transform->dirty    = true;

                //-------------------------------------------------------------
                // 輪郭の光。抜けた直後を強くして、そこから落ち着かせる
                //-------------------------------------------------------------
                if(auto* rim = registry.try_get<Tsukino::BuiltIn::ECS::RimGlowComponent>(weapon.entity)) {
                    if(sinceBurst < 0.0f) {
                        rim->active = false;
                    } else {
                        const float fadeT = std::clamp(sinceBurst / params.rimFadeDuration, 0.0f, 1.0f);

                        rim->active       = true;
                        rim->rimColor     = params.rimColor;
                        rim->rimIntensity = params.rimBurstAmount + (params.rimSettledAmount - params.rimBurstAmount) * fadeT;
                        rim->rimPower     = params.rimPower;
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
                    const float swayT = stage.elapsed / params.cameraSwayPeriod * 2.0f * kPi;

                    cameraTransform->position = stage.cameraBasePosition
                                                + hlslpp::float3(std::sin(swayT) * params.cameraSwayX, std::sin(swayT * 2.0f) * params.cameraSwayY, 0.0f);
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
        const TitleStageParams& params = GetParams();

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
            const float elapsed = stage.launchElapsed - params.launchStagger * static_cast<float>(weaponIndex);
            if(elapsed < 0.0f)
                continue;

            // 飛び出す前の位置は、自分の番が来た瞬間の（漂っている）位置をそのまま使う
            if(!weapon.launchStarted) {
                weapon.launchStarted = true;
                weapon.launchStart   = transform->position;

                PlaySound(registry, SoundId::Swing);
            }

            const LaunchStop&    stop      = params.launchStops[weaponIndex];
            const hlslpp::float3 awayDir   = hlslpp::normalize(weapon.launchStart - cameraPosition);
            const hlslpp::float3 windUpEnd = weapon.launchStart + awayDir * params.launchWindUpDistance;
            const hlslpp::float3 flightEnd =
                cameraPosition + viewDir * stop.distance + rightDir * stop.sideOffset + upDir * stop.upOffset;

            //-------------------------------------------------------------
            // 溜め（奥へ引く）→ 加速してカメラへ、の2段。飛ぶ間は縦にも回して勢いを出す
            //-------------------------------------------------------------
            float spin = weapon.spinPhase;

            if(elapsed < params.launchWindUp) {
                const float t       = elapsed / params.launchWindUp;
                transform->position = weapon.launchStart + (windUpEnd - weapon.launchStart) * EaseOut(t);
            } else {
                const float t = std::clamp((elapsed - params.launchWindUp) / params.launchFlight, 0.0f, 1.0f);

                // 終わりへ向けて加速させる（等速だとスローに見える）
                const float accelerated = t * t * t;

                transform->position = windUpEnd + (flightEnd - windUpEnd) * accelerated;
                spin += params.launchSpinRadians * accelerated;

                transform->rotation = hlslpp::mul(hlslpp::quaternion::rotation_y(spin),
                                                  hlslpp::quaternion::rotation_x(params.launchSpinRadians * accelerated));
            }

            transform->dirty = true;

            // 飛んでいる間は輪郭を光らせる
            if(auto* rim = registry.try_get<Tsukino::BuiltIn::ECS::RimGlowComponent>(weapon.entity)) {
                rim->active   = true;
                rim->rimColor = params.rimColor;
                rim->rimIntensity =
                    params.rimBurstAmount + (params.launchRimAmount - params.rimBurstAmount) * std::clamp(elapsed / params.launchWindUp, 0.0f, 1.0f);
                rim->rimPower = params.launchRimPower;
            }

            //-------------------------------------------------------------
            // 目の前で弾ける。エフェクトと音は1本につき1回だけ
            //-------------------------------------------------------------
            if(!weapon.launchBurst && elapsed >= params.launchWindUp + params.launchFlight) {
                weapon.launchBurst = true;

                if(context.effectSystem && context.assetManager) {
                    const Tsukino::Core::Path   effectPath(kBurstEffectPath);
                    Tsukino::Asset::AssetHandle effectAsset = context.assetManager->Load(effectPath);
                    if(effectAsset.IsValid()) {
                        float position[3] = {flightEnd.x, flightEnd.y, flightEnd.z};
                        context.effectSystem->PlayEffect(registry, effectAsset, effectPath, position, false, params.launchBurstEffectScale);
                    }
                }

                PlaySound(registry, SoundId::WeaponEvolve);
            }
        }

        //-------------------------------------------------------------
        // 3本とも弾けて余韻も過ぎたら、黒フェードで戦闘シーンへ渡す（アセットは起動時のロード画面で読み済み）。
        // タイトルから始めたときだけ操作の案内を出す（リトライからは出さない）
        //-------------------------------------------------------------
        if(!stage.launchHandedOff && stage.launchElapsed >= CalculateLaunchTotal(params)) {
            stage.launchHandedOff = true;

            RequestSceneChangeWithFade(registry, []() { return std::make_unique<CombatAndroid::CombatAndroidScene>(true); });
        }
    }
}    // namespace CombatAndroid::ECS
