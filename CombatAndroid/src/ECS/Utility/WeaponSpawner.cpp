//-------------------------------------------------------------
//! @file    WeaponSpawner.cpp
//! @brief   武器エンティティの生成処理の実装
//! @author  山﨑愛
//-------------------------------------------------------------
#include <CombatAndroid/ECS/Utility/WeaponSpawner.hpp>

#include <CombatAndroid/ECS/Component/PickupComponent.hpp>
#include <CombatAndroid/ECS/Component/WeaponComponent.hpp>

#include <Tsukino/EngineIntegration/EngineContext.hpp>
#include <Tsukino/Engine/Asset/AssetManager.hpp>

#include <Tsukino/BuiltIn/ECS/Component/HighlightComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/ModelComponent.hpp>
#include <Tsukino/BuiltIn/ECS/Component/TransformComponent.hpp>

#include <entt/entt.hpp>

#include <iterator>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    namespace {
        //! 地面に落ちている武器を横たわらせる回転（X軸90度）
        const hlslpp::quaternion kLyingRotation = hlslpp::quaternion::rotation_x(1.5708f);

        //-------------------------------------------------------------
        // 握りパラメータの共通値。3種とも同じメッシュ規約（エクスポート時の軸）で
        // 作られているため、WeaponGripDebugSystemで詰めた値をそのまま共有できている。
        // メッシュごとにずれが出たら、この定数を使うのをやめて武器ごとの値へ分けること
        //-------------------------------------------------------------
        //-------------------------------------------------------------
        // 所有者に持たれているときの既定の追従パラメータ。
        // 「手ボーンには追従せず、所有者のルートTransformから見て右肩の斜め上・やや後ろに置く」
        // という持ち方で、floatEnabledを立てるとそこを基準にふわふわ漂う。
        // 攻撃中はattackBlendによりattackHandTrackingWeight（＝手ボーンへ完全追従）側へ
        // 連続的に切り替わる（CombatSystem参照）
        //-------------------------------------------------------------
        const hlslpp::float3 kDefaultCarryOffset{35.0f, 170.0f, -20.0f};

        const hlslpp::float3     kCommonGripPointLocal{0.0f, 0.0f, 10.0f};
        const hlslpp::float3     kCommonAttackLocalOffset{0.0f, 0.0f, 0.0f};
        const hlslpp::quaternion kCommonAttackGripRotation{0.5f, 0.5f, -0.5f, 0.5f};

        //-------------------------------------------------------------
        // プレイヤーの連撃クリップは3種とも「1クリップに3段入り」構成で、
        // 0 / 1x / 1.3x / 終端の比率で切り出している。尺は Hammer Attack.fbx の
        // 実測値（30fps / 106フレーム = 3.5333秒）で、他2本も同尺と仮定した暫定値。
        // ずれる場合はWeaponGripDebugSystemのF10/F11コマ送りで武器ごとに詰める
        //-------------------------------------------------------------
        constexpr float kAttackClipDuration = 3.5333f;

        //-------------------------------------------------------------
        // 武器定義テーブル本体。
        //
        // ★ 武器を1種追加するときはここへ1行足すだけでよい ★
        //
        // WeaponTable.cppのkWeaponTableと同じく、WeaponIdの並び順どおりに定義すること
        // （GetWeaponSpawnDefinitionがenum値で直接添字を引くため）
        //-------------------------------------------------------------
        const WeaponSpawnDefinition kWeaponSpawnTable[] = {
            {WeaponId::Warhammer, "CombatAndroid/Assets/Models/warhammer.fbx", L"ウォーハンマー",
             kCommonGripPointLocal, kCommonAttackLocalOffset, kCommonAttackGripRotation,
             "CombatAndroid/Assets/Anims/Player/Hammer Attack.fbx", kAttackClipDuration,
             false,
             // 3段目フィニッシュのAoE(範囲攻撃)。スケール100は1ユニット≒1cm規約への単位合わせ
             160.0f, "CombatAndroid/Assets/Effect/warhammerAttackCombo3.efkefc", 100.0f,
             // 斬撃弾は非対応
             nullptr, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 2, 0.0f, 0.0f, 0.0f},

            {WeaponId::Greatsword, "CombatAndroid/Assets/Models/greatsword.fbx", L"グレートソード",
             kCommonGripPointLocal, kCommonAttackLocalOffset, kCommonAttackGripRotation,
             "CombatAndroid/Assets/Anims/Player/Great Sword Slash.fbx", kAttackClipDuration,
             false,
             0.0f, nullptr, 1.0f,
             nullptr, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 2, 0.0f, 0.0f, 0.0f},

            {WeaponId::Battleaxe, "CombatAndroid/Assets/Models/battleaxe.fbx", L"バトルアックス",
             kCommonGripPointLocal, kCommonAttackLocalOffset, kCommonAttackGripRotation,
             "CombatAndroid/Assets/Anims/Player/Standing Melee Attack Backhand.fbx", kAttackClipDuration,
             // バトルアックスのみ、左クリック長押しの溜め攻撃に対応する
             true,
             0.0f, nullptr, 1.0f,
             //-------------------------------------------------------------
             // 溜め攻撃の解放で前方へ飛ばす斬撃弾。エフェクトは座標移動を持たないので
             // 弾エンティティが運ぶ（ProjectileSystem）。再生速度は等倍
             // （1未満にすると尺が伸びるぶん見た目が間延びする）。
             // スケール100は1ユニット≒1cm規約への単位合わせ
             //-------------------------------------------------------------
             "CombatAndroid/Assets/Effect/battleaxeAttackEffect.efkefc",
             /* Scale */ 100.0f, /* PlaySpeed */ 1.0f,
             /* Speed */ 700.0f, /* Radius */ 70.0f,
             /* Lifetime */ 1.2f, /* MaxDistance */ 1000.0f,
             /* DamageMultiplier */ 1.0f,
             // 1段階目（白）の弾は1体で止まり、2段階目（青）以上でのみ貫通する
             /* PierceMinChargeStage */ 2,
             /* SpawnDelay */ 0.25f, /* SpawnHeight */ 100.0f, /* SpawnForward */ 60.0f},
        };

        // 種類を足したのにテーブルへ書き忘れる事故を防ぐ（WeaponTable.cppと同じ作法）
        static_assert(std::size(kWeaponSpawnTable) == static_cast<size_t>(WeaponId::Count),
                      "WeaponId に種類を足したら kWeaponSpawnTable にも1行足すこと");
        //-------------------------------------------------------------
        //! @brief  所有者に持たれるときの追従パラメータを既定へ書き戻すヘルパー
        //! @param  weapon [in,out] 対象のコンポーネント
        //! @note   生成時（SpawnWeapon）と、持ち主の手から離れるとき（DropWeaponToWorld）の
        //!         両方から呼ぶ。落とした武器を次に拾った者が「前の持ち主の持ち方」を
        //!         引き継いでしまわないようにするための一本化で、実際これを戻し忘れて
        //!         「敵から拾った武器だけプレイヤーの手ボーンに張り付いて暴れる」不具合を出している。
        //!         攻撃中用のフィールド（attackLocalOffset等）は武器種ごとの値なので
        //!         ConfigureWeaponの担当。ここでは触らない
        //-------------------------------------------------------------
        void ApplyDefaultCarryPose(WeaponComponent& weapon) {
            weapon.localOffset              = kDefaultCarryOffset;
            weapon.gripRotationOffset       = kLyingRotation;
            weapon.handTrackingWeight       = 0.0f;    // 非攻撃時はルートTransformからの固定オフセット追従
            weapon.attackHandTrackingWeight = 1.0f;    // 攻撃中は手ボーンへ完全追従
            weapon.isAttacking              = false;
        }
    }    // namespace

    //-------------------------------------------------------------
    //! @brief 識別子から定義を引く
    //-------------------------------------------------------------
    const WeaponSpawnDefinition& GetWeaponSpawnDefinition(WeaponId id) {
        int index = static_cast<int>(id);
        if(index < 0 || index >= static_cast<int>(WeaponId::Count))
            index = 0;

        return kWeaponSpawnTable[index];
    }

    //-------------------------------------------------------------
    //! @brief 既存エンティティのWeaponComponentへ、その武器種の設定一式を書き込む
    //-------------------------------------------------------------
    void ConfigureWeapon(Tsukino::ECS::Registry& registry,
                         Tsukino::EngineIntegration::EngineContext& context,
                         Tsukino::ECS::Entity weaponEntity,
                         WeaponId weaponId) {
        WeaponComponent&              weapon       = registry.GetComponent<WeaponComponent>(weaponEntity);
        const WeaponSpawnDefinition&  definition   = GetWeaponSpawnDefinition(weaponId);
        Tsukino::Asset::AssetManager& assetManager = *context.assetManager;

        weapon.weaponId = weaponId;

        // 実効ステータス（damage）は常にWeaponTableから導出する（加算では積み上げない）
        RecalculateWeaponStats(weapon);

        //-------------------------------------------------------------
        // 握りパラメータ。WeaponGripDebugSystem（_DEBUGビルドのF6調整モード）で
        // 実機の見た目を確認しながら調整した確定値
        //-------------------------------------------------------------
        weapon.gripPointLocal           = definition.gripPointLocal;
        weapon.attackLocalOffset        = definition.attackLocalOffset;
        weapon.attackGripRotationOffset = definition.attackGripRotationOffset;

        //-------------------------------------------------------------
        // 武器専用の攻撃モーション。1クリップに3段入っている構成を
        // 0 / 1x / 1.3x / 終端の比率で切り出す（PlayerAnimationSystemが
        // WeaponComponent::attackClipの有無を見て既定クリップから差し替える）
        //-------------------------------------------------------------
        if(definition.playerAttackClipPath != nullptr) {
            float stepLength = definition.attackClipDuration / 3.0f;

            weapon.attackClip           = assetManager.Load(Tsukino::Core::Path(definition.playerAttackClipPath));
            weapon.attackAnimationIndex = 1;    // Mixamo製FBXはindex 0が1tickのスタブ、index 1が実モーション

            weapon.attackStepStartTime[0] = stepLength * 0.0f;
            weapon.attackStepEndTime[0]   = stepLength * 1.0f;

            weapon.attackStepStartTime[1] = stepLength * 1.0f;
            weapon.attackStepEndTime[1]   = stepLength * 1.3f;

            weapon.attackStepStartTime[2] = stepLength * 1.3f;
            weapon.attackStepEndTime[2]   = definition.attackClipDuration;
        }

        //-------------------------------------------------------------
        // 3段目フィニッシュのAoE(範囲攻撃)。半径0の武器は非対応なので何も設定しない
        //-------------------------------------------------------------
        weapon.areaAttackRadius = definition.areaAttackRadius;
        if(definition.areaAttackRadius > 0.0f && definition.areaAttackEffectPath != nullptr) {
            Tsukino::Core::Path effectPath(definition.areaAttackEffectPath);

            weapon.areaAttackEffectAsset = assetManager.Load(effectPath);
            weapon.areaAttackEffectPath  = effectPath;
            weapon.areaAttackEffectScale = definition.areaAttackEffectScale;
        }

        weapon.chargeAttackEnabled = definition.chargeAttackEnabled;

        //-------------------------------------------------------------
        // 溜め攻撃の解放で飛ばす斬撃弾。エフェクトが未設定の武器は非対応なので何も設定しない
        //-------------------------------------------------------------
        if(definition.projectileEffectPath != nullptr) {
            Tsukino::Core::Path projectileEffectPath(definition.projectileEffectPath);

            weapon.projectileEffectAsset     = assetManager.Load(projectileEffectPath);
            weapon.projectileEffectPath      = projectileEffectPath;
            weapon.projectileEffectScale     = definition.projectileEffectScale;
            weapon.projectileEffectPlaySpeed = definition.projectileEffectPlaySpeed;
            weapon.projectileSpeed           = definition.projectileSpeed;
            weapon.projectileRadius          = definition.projectileRadius;
            weapon.projectileLifetime        = definition.projectileLifetime;
            weapon.projectileMaxDistance     = definition.projectileMaxDistance;
            weapon.projectileDamageMultiplier    = definition.projectileDamageMultiplier;
            weapon.projectilePierceMinChargeStage = definition.projectilePierceMinChargeStage;
            weapon.projectileSpawnDelay      = definition.projectileSpawnDelay;
            weapon.projectileSpawnHeight     = definition.projectileSpawnHeight;
            weapon.projectileSpawnForward    = definition.projectileSpawnForward;
        }
    }

    //-------------------------------------------------------------
    //! @brief 武器エンティティを1つ生成する
    //-------------------------------------------------------------
    Tsukino::ECS::Entity SpawnWeapon(Tsukino::ECS::Registry& registry,
                                     Tsukino::EngineIntegration::EngineContext& context,
                                     WeaponId weaponId,
                                     const hlslpp::float3& position,
                                     Tsukino::ECS::Entity owner) {
        const WeaponSpawnDefinition& definition = GetWeaponSpawnDefinition(weaponId);

        Tsukino::ECS::Entity weaponEntity = registry.CreateEntity();

        Tsukino::BuiltIn::ECS::TransformComponent& transform =
            registry.AddComponent<Tsukino::BuiltIn::ECS::TransformComponent>(weaponEntity);
        transform.position = position;
        // 所有者がいる場合の姿勢は次のフレームからCombatSystemが上書きするため、ここでは
        // 落ちている武器と同じ「横たわった」姿勢を初期値にしておく
        transform.rotation = (owner == entt::null) ? kLyingRotation : hlslpp::quaternion(0.0f, 0.0f, 0.0f, 1.0f);
        transform.scale    = hlslpp::float3(1.0f, 1.0f, 1.0f);
        transform.dirty    = true;
        transform.parent   = entt::null;

        Tsukino::BuiltIn::ECS::ModelComponent& model = registry.AddComponent<Tsukino::BuiltIn::ECS::ModelComponent>(weaponEntity);
        model.modelHandle                            = context.assetManager->Load(Tsukino::Core::Path(definition.modelPath));
        model.visible                                = true;

        WeaponComponent& weapon = registry.AddComponent<WeaponComponent>(weaponEntity);
        weapon.owner            = owner;
        weapon.level            = 1;
        ApplyDefaultCarryPose(weapon);
        weapon.floatEnabled = false;    // 浮遊演出が要る場合は呼び出し側で立てる

        // 武器種ごとの性能（攻撃力・専用モーション・AoE・溜め攻撃）を焼き込む
        ConfigureWeapon(registry, context, weaponEntity, weaponId);

        // 所有者がいなければ「ワールドに落ちている＝拾える」状態にする
        if(owner == entt::null) {
            PickupComponent& pickup = registry.AddComponent<PickupComponent>(weaponEntity);
            pickup.displayName      = definition.displayName;
        }

        // レベルアップ時・ピックアップ対象時のリムライト発光演出（PickupSystem）の土台。
        // active既定falseのため通常時の見た目には影響しない
        registry.AddComponent<Tsukino::BuiltIn::ECS::HighlightComponent>(weaponEntity);

        return weaponEntity;
    }

    //-------------------------------------------------------------
    //! @brief 所有者の手から外し、その場に落ちているピックアップへ戻す
    //-------------------------------------------------------------
    void DropWeaponToWorld(Tsukino::ECS::Registry& registry,
                           Tsukino::ECS::Entity weaponEntity,
                           const hlslpp::float3& groundPosition) {
        // 敵が間引かれた等で武器が既に破棄されている場合に備えて存在を確かめる
        // （HasComponentは無効なエンティティに対して呼べない）
        if(!registry.IsValid(weaponEntity) || !registry.HasComponent<WeaponComponent>(weaponEntity))
            return;

        WeaponComponent& weapon = registry.GetComponent<WeaponComponent>(weaponEntity);

        // 未所有＝CombatSystemの追従処理（owner != entt::nullが条件）に入らなくなり、その場に留まる
        weapon.owner        = entt::null;
        weapon.floatEnabled = false;
        weapon.isSnapped    = false;
        weapon.attackBlend  = 0.0f;
        // 追従が止まる以上、前フレーム姿勢からのスイープ判定も無効にしておく
        weapon.hasPrevAttackPose = false;

        // 持ち方を既定へ戻す。前の持ち主（敵）が握りパラメータを変えていた場合、
        // ここで戻さないと次に拾ったプレイヤーがその持ち方を引き継いでしまう
        ApplyDefaultCarryPose(weapon);

        if(registry.HasComponent<Tsukino::BuiltIn::ECS::TransformComponent>(weaponEntity)) {
            Tsukino::BuiltIn::ECS::TransformComponent& transform =
                registry.GetComponent<Tsukino::BuiltIn::ECS::TransformComponent>(weaponEntity);
            transform.position = groundPosition;
            transform.rotation = kLyingRotation;    // 手置きの武器と同じく地面に横たわらせる
            transform.dirty    = true;
        }

        // 既にPickupComponentを持っている（＝二重にドロップされた）場合は付け直さない。
        // AddComponentし直すと拾いかけのハイライト演出の状態が飛んでしまう
        if(!registry.HasComponent<PickupComponent>(weaponEntity)) {
            PickupComponent& pickup = registry.AddComponent<PickupComponent>(weaponEntity);
            pickup.displayName      = GetWeaponSpawnDefinition(weapon.weaponId).displayName;
        }
    }
}    // namespace CombatAndroid::ECS
