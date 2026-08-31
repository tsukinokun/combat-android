//-------------------------------------------------------------
//! @file    WeaponSpawner.hpp
//! @brief   武器エンティティの生成処理の宣言
//! @author  山﨑愛
//! @note    以前は CombatAndroidScene::OnInitialize 内のローカルラムダ
//!          （spawnFloatingWeapon / spawnWorldWeapon）と、その直後に武器ごとへ
//!          直接書き下していた設定ブロックに分かれていたため、シーン構築時にしか
//!          武器を用意できなかった。敵（Paladin）が武器を持って湧き、撃破時に
//!          それを落とせるよう、EnemySpawnerと同じ流儀でここへ切り出している。
//!          「武器1種類をどう組み立てるか」は全てConfigureWeaponに集約してあり、
//!          手置き・敵の持ち物・死亡ドロップのどの経路で作られた武器も
//!          まったく同じ性能になる
//-------------------------------------------------------------
#pragma once

#include <CombatAndroid/ECS/Utility/WeaponTable.hpp>

#include <Tsukino/Core/ECS/Registry/Registry.hpp>
#include <Tsukino/Core/ECS/Entity/Entity.hpp>
#include <Tsukino/Core/Path.hpp>

#include <hlsl++.h>

// 前方宣言
namespace Tsukino::EngineIntegration {
    struct EngineContext;
}

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @struct WeaponSpawnDefinition
    //! @brief  武器1種類ぶんの静的な定義（メッシュ・表示名・握り・攻撃クリップ・特殊能力）
    //! @note   WeaponTableが「レベルごとの攻撃力」を持つのに対し、こちらは
    //!         「見た目と振り方」を持つ。分けてあるのは、攻撃力の調整（バランス）と
    //!         握り位置の調整（実機での見た目合わせ）が別作業だからである。
    //!         パスをconst char*で持っているのは、テーブルを読み取り専用の
    //!         静的データに保ち、動的確保を起こさないようにするため
    //-------------------------------------------------------------
    struct WeaponSpawnDefinition {
        WeaponId       id;             //!< 種類の識別子
        const char*    modelPath;      //!< 武器メッシュのパス
        const wchar_t* displayName;    //!< 落ちている時の「F : ○○ を拾う」に出す名前

        //-------------------------------------------------------------
        // 握りパラメータ。WeaponGripDebugSystem（_DEBUGビルドのF6調整モード）で
        // 実機の見た目を確認しながら詰めた確定値を焼き込む場所
        //-------------------------------------------------------------
        hlslpp::float3     gripPointLocal;              //!< 手のひらに一致させたい武器ローカル座標
        hlslpp::float3     attackLocalOffset;           //!< 攻撃中の手ボーンからの位置オフセット
        hlslpp::quaternion attackGripRotationOffset;    //!< 攻撃中の握り姿勢補正

        //-------------------------------------------------------------
        // プレイヤーが振るときの3段コンボ。1クリップに3段入っている構成を
        // 0 / 1x / 1.3x / 終端の比率で切り出す（PlayerAnimationSystem参照）
        //-------------------------------------------------------------
        const char* playerAttackClipPath;    //!< nullptrならPlayerAnimationSetComponentの既定クリップを使う
        float       attackClipDuration;      //!< クリップ全体の尺（秒）

        bool chargeAttackEnabled;    //!< 左クリック長押しの溜め攻撃に対応するか

        //-------------------------------------------------------------
        // 3段目フィニッシュのAoE(範囲攻撃)。areaAttackRadiusが0なら非対応
        //-------------------------------------------------------------
        float       areaAttackRadius;
        const char* areaAttackEffectPath;
        float       areaAttackEffectScale;

        //-------------------------------------------------------------
        // ノックバック（怯み＋吹っ飛ばし）。全て0/falseなら従来どおり
        // 「敵のknockbackDamageThresholdを超えた一撃だけがその場で怯む」挙動になる
        //-------------------------------------------------------------
        bool  knockbackIgnoresThreshold;    //!< trueなら敵の閾値を無視して必ず怯ませる（重い武器）
        float knockbackSpeed;               //!< 通常ヒットの押し出し初速（ユニット/秒）。0なら位置を動かさない
        float knockbackStun;                //!< 通常ヒットの追加スタン時間（秒）
        float areaKnockbackSpeed;           //!< AoEヒットの押し出し初速（3段目フィニッシュの吹っ飛ばし）
        float areaKnockbackStun;            //!< AoEヒットの追加スタン時間（秒）

        //-------------------------------------------------------------
        // 溜め攻撃の解放時に前方へ飛ばす斬撃弾。projectileEffectPathがnullptrなら非対応。
        // chargeAttackEnabledがfalseの武器に値を入れても撃つ機会が無いので意味を持たない
        //-------------------------------------------------------------
        const char* projectileEffectPath;         //!< 弾の見た目に使うEffekseerエフェクト
        float       projectileEffectScale;        //!< 上記の再生スケール（本作は1ユニット≒1cm規約のため100前後が要る）
        float       projectileEffectPlaySpeed;    //!< 上記の再生速度（1.0未満でゆっくり再生される）
        float       projectileSpeed;              //!< 飛翔速度（ユニット/秒）
        float       projectileRadius;             //!< 当たり判定カプセルの半径
        float       projectileLifetime;           //!< 寿命（秒）
        float       projectileMaxDistance;        //!< 最大飛距離（ユニット）
        float       projectileDamageMultiplier;   //!< 弾のダメージ倍率（武器の実ダメージへ更に掛ける）
        int         projectilePierceMinChargeStage;    //!< この溜め段階（1=白/2=青/3=紫）以上でのみ弾が貫通する
        float       projectileSpawnDelay;         //!< 解放から発射までの遅延（秒）。振り抜くタイミングに合わせる
        float       projectileSpawnHeight;        //!< 発射位置の高さ（所有者の足元原点からの相対）
        float       projectileSpawnForward;       //!< 発射位置を前方へずらす量（体に埋まって見えないようにする）
    };

    //-------------------------------------------------------------
    //! @brief  識別子から定義を引く関数
    //! @param  id [in] 引きたい武器の識別子
    //! @return 対応する定義（テーブルはidの並び順に定義されている）
    //-------------------------------------------------------------
    [[nodiscard]]
    const WeaponSpawnDefinition& GetWeaponSpawnDefinition(WeaponId id);

    //-------------------------------------------------------------
    //! @brief  既存エンティティのWeaponComponentへ、その武器種の設定一式を書き込む関数
    //! @param  registry     [in]     エンティティレジストリ
    //! @param  context      [in]     エンジンコンテキスト（AssetManagerの取得に使う）
    //! @param  weaponEntity [in]     対象エンティティ（WeaponComponentを持っていること）
    //! @param  weaponId     [in]     設定する武器の種類
    //! @note   「武器の性能をどう決めるか」の唯一の情報源。手置き・敵の持ち物・
    //!         死亡ドロップのどの経路もここを通すこと。通し忘れると、拾った武器だけ
    //!         専用モーションやAoEが効かない、という分かりにくい不具合になる
    //-------------------------------------------------------------
    void ConfigureWeapon(Tsukino::ECS::Registry& registry,
                         Tsukino::EngineIntegration::EngineContext& context,
                         Tsukino::ECS::Entity weaponEntity,
                         WeaponId weaponId);

    //-------------------------------------------------------------
    //! @brief  武器エンティティを1つ生成する関数
    //! @param  registry [in]     エンティティレジストリ
    //! @param  context  [in]     エンジンコンテキスト
    //! @param  weaponId [in]     生成する武器の種類
    //! @param  position [in]     初期位置（ownerを指定した場合は次のフレームに追従位置へ上書きされる）
    //! @param  owner    [in]     所有者。entt::nullなら地面に落ちている状態で作る
    //! @return 生成した武器エンティティ
    //! @note   ownerがentt::nullの場合はPickupComponentを付けて「拾える」状態にし、
    //!         非nullの場合は所有者の手ボーンへ追従する状態にする。
    //!         浮遊演出（floatEnabled）は呼び出し側で必要に応じて立てること
    //-------------------------------------------------------------
    Tsukino::ECS::Entity SpawnWeapon(Tsukino::ECS::Registry& registry,
                                     Tsukino::EngineIntegration::EngineContext& context,
                                     WeaponId weaponId,
                                     const hlslpp::float3& position,
                                     Tsukino::ECS::Entity owner = entt::null);

    //-------------------------------------------------------------
    //! @brief  所有者の手から外し、その場に落ちているピックアップへ戻す関数
    //! @param  registry       [in]     エンティティレジストリ
    //! @param  weaponEntity   [in]     対象の武器エンティティ
    //! @param  groundPosition [in]     落とす位置（yは接地高さへ差し替えてから渡すこと）
    //! @note   Paladinの死亡ドロップで使う。エンティティを作り直さずに所有状態だけ
    //!         戻すため、ConfigureWeaponで焼いた性能はそのまま引き継がれる。
    //!         PickupComponentを足す＝コンポーネント構成が変わるので、
    //!         必ずViewの反復の外側から呼ぶこと
    //-------------------------------------------------------------
    void DropWeaponToWorld(Tsukino::ECS::Registry& registry,
                           Tsukino::ECS::Entity weaponEntity,
                           const hlslpp::float3& groundPosition);
}    // namespace CombatAndroid::ECS
