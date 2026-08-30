//-------------------------------------------------------------
//! @file   WeaponComponent.hpp
//! @brief  WeaponComponent構造体の宣言
//! @author 山﨑愛
//-------------------------------------------------------------
#pragma once
#include <CombatAndroid/ECS/Component/PlayerAnimationSetComponent.hpp>
#include <CombatAndroid/ECS/Utility/WeaponTable.hpp>
#include <Tsukino/Core/ECS/Entity/Entity.hpp>
#include <Tsukino/Core/typedef.hpp>
#include <Tsukino/Core/Path.hpp>
#include <Tsukino/Engine/Asset/AssetHandle.hpp>

#include <hlsl++.h>

#include <string>
#include <vector>
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @struct WeaponComponent
    //! @brief  武器エンティティに付与するコンポーネント。
    //!         所有者（プレイヤー）の手ボーンにアタッチして追従しつつ、攻撃入力時に範囲内の敵へダメージを与える。
    //!         ボーンが解決できない場合は所有者のルートTransformからの固定オフセット追従にフォールバックする。
    //!         当たり判定はJolt物理のカプセルオーバーラップ判定（PhysicsSystem::OverlapCapsule）で行う
    //-------------------------------------------------------------
    struct WeaponComponent {
        Tsukino::ECS::Entity owner = entt::null;    //!< 武器を所持しているエンティティ

        //-------------------------------------------------------------
        // 武器種別・レベル。同じweaponIdの武器を重複して拾うと、新規枠を増やす代わりに
        // 既存の個体のlevelを上げてWeaponTableからdamageを引き直す（PickupSystemが行う）。
        // 初期値のWarhammer/1はいずれも各スポーン箇所で明示的に上書きする前提の暫定値
        //-------------------------------------------------------------
        WeaponId weaponId = WeaponId::Warhammer;    //!< 武器種別の識別子。スポーン箇所ごとに明示的に設定する
        int      level    = 1;                       //!< 現在のレベル（初期取得時は1、上限はkMaxWeaponLevel）

        //-------------------------------------------------------------
        // レベルアップ演出。同種武器を拾って吸い寄せられてきた個体がこの武器に重なった瞬間、
        // PickupSystemがこの値をkLevelUpFlashDurationにセットする。0より大きい間だけ
        // PickupSystemがHighlightComponentへリムライト発光値を書き込み、0秒に向けて減衰させる
        //-------------------------------------------------------------
        float levelUpFlashTimer = 0.0f;    //!< レベルアップ発光演出の残り時間（秒）。0以下で非発光

        std::string handBoneName      = "mixamorig:RightHand";    //!< アタッチ対象ボーン名
        u32          handBoneNodeIndex = UINT32_MAX;                //!< 解決済みノードindex（未解決/見つからない場合はUINT32_MAX）

        //-------------------------------------------------------------
        // ボーン解決は「どのモデルのnode配列に対して行ったか」をキャッシュする。
        // ノードindexは所有者のModelComponentが指すキャラクターモデル（描画に使われている
        // 実メッシュ）を基準に固定されており、再生中のクリップが切り替わっても変わらないため、
        // モデルが変わらない限り一度解決すれば再解決は不要（武器の持ち替え等でモデルが
        // 変わる場合のみ再解決される）
        //-------------------------------------------------------------
        Tsukino::Asset::AssetHandle resolvedAgainstModel;    //!< 最後にボーン解決を行った時点の所有者モデル（比較して再解決要否を判定する）

        hlslpp::float3     localOffset         = hlslpp::float3(30.0f, 100.0f, 60.0f);    //!< アタッチボーンのローカル空間での握り位置オフセット（ボーン未解決時はルートTransformからのオフセットとして使われる）
        hlslpp::quaternion gripRotationOffset = hlslpp::quaternion(0.0f, 0.0f, 0.0f, 1.0f);    //!< 握り方調整用の追加回転
        float               handTrackingWeight = 1.0f;    //!< 手ボーン位置への追従度（0=ルート位置に留まる, 1=手ボーンに完全追従）。
                                                            //!< アニメーションクリップの腕の振り幅が大きく武器が体から離れすぎる場合に下げて使う

        //-------------------------------------------------------------
        // 攻撃中（isAttacking=true）はlocalOffset/gripRotationOffsetの代わりにこちらを使う。
        // localOffsetは「ほぼ静止した基準点からの浮遊位置」として調整された値（170ユニット近く離れている）で、
        // 実際に振られる手ボーンにそのまま適用するとテコの原理で大きく・速く振り回されてしまう
        // （振った時に武器が暴れる不具合の原因）。攻撃時は手のひら付近の小さいオフセットを別途用意する
        //-------------------------------------------------------------
        float               attackHandTrackingWeight  = 1.0f;    //!< 攻撃中に代わりに使う追従度。振りの動きにしっかり追従させるため通常1.0
        hlslpp::float3     attackLocalOffset          = hlslpp::float3(0.0f, 0.0f, 0.0f);    //!< 攻撃中の握り位置オフセット（手ボーンローカル空間）。実機で見た目を確認しながら調整する
        hlslpp::quaternion attackGripRotationOffset  = hlslpp::quaternion(0.0f, 0.0f, 0.0f, 1.0f);    //!< 攻撃中の握り角度オフセット。実機で見た目を確認しながら調整する

        //-------------------------------------------------------------
        // 「手に持つ」のではなく所有者の周りをふわふわ浮遊させる演出用パラメータ。
        // localOffsetを浮遊位置として使い、その上に上下・左右のゆったりした漂いを加える
        // （旋回はしない。姿勢はgripRotationOffsetを基準にわずかに揺れるだけ）
        //-------------------------------------------------------------
        bool  floatEnabled        = false;    //!< trueで浮遊演出（ふわふわ漂う動き）を有効化する
        float floatBobAmplitude  = 6.0f;      //!< 上下に漂う振れ幅
        float floatBobSpeed      = 1.6f;      //!< 上下の漂いの角速度（rad/sec）
        float floatDriftAmplitude = 4.0f;      //!< 左右・前後に漂う振れ幅（上下と別周期でゆっくり揺れて円を描くように漂う）
        float floatDriftSpeed    = 0.9f;      //!< 左右・前後の漂いの角速度（rad/sec）
        float floatSwayAngle      = 0.12f;     //!< 姿勢が前後に傾く最大角度（ラジアン）。小さく保つことで縦向きをほぼ維持する
        float floatSwaySpeed      = 1.1f;      //!< 姿勢の揺れの角速度（rad/sec）
        float floatTime            = 0.0f;      //!< 浮遊演出用の経過時間（CombatSystemが毎フレーム加算する）

        bool  floatSelected            = false;    //!< 選択中の武器か。trueの間は浮遊位置を通常より高くする。
                                                     //!< PlayerSystemが武器切り替え時に更新する
        float floatSelectedHeightBoost = 40.0f;    //!< floatSelected中に追加で浮かせる高さ

        float damage           = 20.0f;    //!< 命中時に与える基礎ダメージ
        float damageMultiplier = 1.0f;     //!< 実ダメージ = damage * damageMultiplier。連撃の段ごとにPlayerAnimationSystemが書き換える
                                             //!< （AttackStep::damageMultiplier）。ノックバック閾値の判定もこの実ダメージで行う
        // 当たり判定はPhase B(Jolt物理)のカプセルで行う。グリップ位置(transform.position)を起点に
        // 武器のローカルY軸（＝刃の向き。モデルがエクスポート時点でY-upのため）方向へrangeだけ伸びる
        // カプセルを毎フレーム構築してCombatSystemがOverlapCapsuleへ渡す
        float range            = 90.0f;    //!< 当たり判定カプセルの長さ（グリップから刃先までの到達距離）
        float hitCapsuleRadius = 18.0f;    //!< 当たり判定カプセルの半径（刃の太さ相当）

        //-------------------------------------------------------------
        // 剣の振りはグリップの移動より回転が支配的（柄はほぼ同じ位置に留まり刃先が弧を描く）ため、
        // 現フレームのカプセル姿勢だけでは速い振りやフレーム落ちで敵をすり抜けてしまう
        // （トンネリング）ことがある。前フレームの武器姿勢（位置・回転）を保持しておき、
        // CombatSystemが今フレームの姿勢までを位置lerp・回転slerpで補間したサブステップに分割して
        // 判定することで、平行移動しか表現できないJoltのスイープ判定では捉えられない回転分の弧も補う
        //-------------------------------------------------------------
        hlslpp::float3     prevAttackPosition = hlslpp::float3(0.0f, 0.0f, 0.0f);          //!< 前フレームの武器グリップ位置（transform.position）
        hlslpp::quaternion prevAttackRotation = hlslpp::quaternion(0.0f, 0.0f, 0.0f, 1.0f); //!< 前フレームの武器姿勢（transform.rotation）
        bool                hasPrevAttackPose  = false;                                      //!< 上記2つが有効か（アタック開始直後のフレームはfalse）

        float activeDuration = 0.25f;    //!< 攻撃入力後、当たり判定が有効な時間（秒）
        float cooldown       = 0.4f;     //!< 攻撃後、再攻撃可能になるまでのクールダウン（秒）

        float nextActiveDurationOverride = -1.0f;    //!< 次にattackRequestedが消費される際、activeDurationの代わりに使う値（-1なら無効）。
                                                       //!< PlayerAnimationSystemが連撃の段ごとに設定する（AttackStep::hitWindowDuration）

        bool  attackRequested = false;    //!< 攻撃入力を受け取ったか（PlayerSystemがセットする）
        bool  isActive        = false;    //!< 現在当たり判定が有効か
        float activeTimer     = 0.0f;     //!< 当たり判定有効時間の残り
        float cooldownTimer   = 0.0f;     //!< クールダウンの残り

        std::vector<Tsukino::ECS::Entity> hitEnemiesThisAttack;    //!< このアタックで既にヒットした敵の記録（多重ヒット防止）。isActiveがtrueになった瞬間のみクリアする

        bool  isAttacking = false;    //!< 攻撃アニメーション再生中か（PlayerAnimationSystemが毎フレームセットする）。
                                       //!< trueの間はfloatEnabledによる浮遊演出を止め、attackHandTrackingWeightで手に追従させる

        //-------------------------------------------------------------
        // isAttackingの真偽値が瞬時に切り替わっても、握りオフセット・回転（浮遊姿勢↔手ボーン追従）が
        // 目標として不連続にジャンプしないよう、0(非攻撃)↔1(攻撃)へ連続的に遷移させるブレンド値。
        // 末尾の指数減衰補間（attachPositionLerpSpeed等）は「目標へ滑らかに近づく」役割であって
        // 「目標自体の飛び」は吸収しきれないため、目標を作る側で先に連続化する
        // （攻撃開始/終了の瞬間に武器がカクッとスナップして見える不具合の原因だった）
        //-------------------------------------------------------------
        float attackBlend      = 0.0f;     //!< 現在の攻撃ブレンド値（0=非攻撃の浮遊姿勢, 1=攻撃中の手ボーン追従）。CombatSystemが毎フレーム更新する
        float attackBlendSpeed = 25.0f;    //!< attackBlendがisAttackingの目標値へ遷移する速さ（大きいほど素早く切り替わる）。
                                             //!< 攻撃の立ち上がりで素早く手へ吸い付かせたいため、以前より大きめの値にしている

        //-------------------------------------------------------------
        // 攻撃モーションへの出入りやフォールバック切り替えでオフセットが瞬時に変わっても
        // 武器が瞬間移動しないよう、目標位置・姿勢へ指数減衰で追従させる際の速度
        // （大きいほど素早く吸い付く。PlayerComponent::turnLerpSpeedと同じ考え方）
        //-------------------------------------------------------------
        float attachPositionLerpSpeed = 18.0f;    //!< 目標位置への追従速度
        float attachRotationLerpSpeed = 18.0f;    //!< 目標姿勢への追従速度

        //-------------------------------------------------------------
        // 「手のひらのどこに柄を重ねるか」を武器メッシュ側のローカル空間で指定する握り点。
        // gripRotationOffset/attackGripRotationOffsetで姿勢を決めた後、この点が手のひら位置
        // （targetPosition）に来るよう位置側を引く。角度と位置を別々に調整できないと、
        // 角度を振るたびに位置オフセットを取り直す羽目になり実機調整が収束しないため分離した。
        // 攻撃中（attackBlend）のみ効かせるので、浮遊演出時の見た目には影響しない
        //-------------------------------------------------------------
        hlslpp::float3 gripPointLocal = hlslpp::float3(0.0f, 0.0f, 0.0f);    //!< 武器ローカル空間での握り点（柄の位置）。実機で見た目を確認しながら調整する

        //-------------------------------------------------------------
        // 攻撃中、目標位置・姿勢へ十分近づいたら指数減衰補間をやめて目標へ直接スナップする
        // （ビタ置き）。指数減衰は原理上どれだけ速くしても定常的な遅れが残り、速い振りでは
        // 「手から遅れて武器がついてくる」ように見えてしまうため、十分近い＝もう遅れが
        // 気にならない距離まで来た時点で追従方式を切り替える
        //-------------------------------------------------------------
        float attackSnapDistance      = 12.0f;    //!< スナップ開始の位置しきい値（目標とのワールド距離）
        float attackSnapAngleDeg      = 12.0f;    //!< スナップ開始の姿勢しきい値（目標との角度差）。位置だけでなく姿勢も十分近い時のみスナップさせ、回転がポップして見えないようにする
        float attackApproachLerpSpeed = 45.0f;    //!< スナップ前、attackBlendにより通常のattach*LerpSpeedから連続的にここへ遷移する接近速度（速いほど素早くスナップ圏内に入る）
        bool  isSnapped                = false;    //!< 現在ビタ置き中か。isAttackingがfalseに戻ると解除され、通常の指数減衰補間へ戻る（CombatSystemが管理）

        //-------------------------------------------------------------
        // 範囲攻撃(AoE)。半径0で無効（既定）。特定の武器・特定の連撃段でのみ使う
        // 追加ダメージ演出。weaponId/WeaponTable経由では持たず、既存のdamage等と同じ
        // 流儀でインスタンスごとに値を設定する（warhammerのスポーン箇所のみ設定）
        //-------------------------------------------------------------
        float areaAttackRadius = 0.0f;    //!< AoE判定半径。0ならこの武器はAoE非対応
        Tsukino::Asset::AssetHandle areaAttackEffectAsset;    //!< AoE発動時に再生するEffekseerエフェクト（未設定なら再生しない）
        Tsukino::Core::Path         areaAttackEffectPath;      //!< 上記エフェクトのファイルパス（EffectSystem::PlayEffectのテクスチャ解決に使う）
        float areaAttackEffectScale = 1.0f;    //!< 上記エフェクトの再生スケール。本作は1ユニット≒1cm規約だがEffekseer側は
                                                 //!< メートル単位で作られるため、単位合わせに100前後の値が要る（実機で見ながら調整する）

        bool  pendingAreaAttack      = false;    //!< 次にattackRequestedが消費される際、AoEを要求するか。PlayerAnimationSystemがAttackStep::areaAttackから設定する
        float pendingAreaAttackDelay = 0.35f;    //!< 上と同様。AoE発動までの遅延（秒）。AttackStep::areaAttackDelayから設定される

        bool  areaAttackArmed = false;    //!< 今回の攻撃でAoE発動待ちか（CombatSystemが管理。発動または攻撃終了で消費）
        float areaAttackTimer = 0.0f;      //!< AoE発動までの残り時間（秒）

        //-------------------------------------------------------------
        // 攻撃モーションの武器差し替え。未設定（!attackClip.IsValid()）ならプレイヤー既定の
        // PlayerAnimationSetComponent::attackSteps（Hammer Attack.fbx）をそのまま使う。
        // weaponId/WeaponTable経由では持たず、areaAttack等と同じ流儀でインスタンスごとに
        // 設定する（great swordのスポーン箇所のみ設定）
        //-------------------------------------------------------------
        Tsukino::Asset::AssetHandle attackClip;    //!< 未設定ならこの武器は既定クリップ（Hammer Attack）を使う
        u32   attackAnimationIndex = 1;             //!< Mixamo製FBX共通でindex 1が実モーション
        float attackStepStartTime[PlayerAnimationSetComponent::kAttackComboCount] = {};    //!< 各段の再生開始時刻（秒）
        float attackStepEndTime[PlayerAnimationSetComponent::kAttackComboCount]   = {};    //!< 各段の再生終了時刻（秒）

        //-------------------------------------------------------------
        // 溜め攻撃(チャージアタック)。有効な武器は左クリック長押しで溜め、離す（または一定時間経過）と
        // 解放する。weaponId/WeaponTable経由では持たず、attackClip等と同じ流儀でインスタンスごとに
        // 設定する（battleaxeのスポーン箇所のみtrueにする）
        //-------------------------------------------------------------
        bool chargeAttackEnabled = false;    //!< trueならこの武器は左クリック長押しで溜め攻撃できる
    };
}    // namespace CombatAndroid::ECS
