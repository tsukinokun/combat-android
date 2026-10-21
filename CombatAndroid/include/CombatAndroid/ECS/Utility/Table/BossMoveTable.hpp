//-------------------------------------------------------------
//! @file    BossMoveTable.hpp
//! @brief   ボスの技の表（Assets/Tables/Bosses/<名前>.json）の宣言
//-------------------------------------------------------------
#pragma once

#include <string>
#include <vector>

// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    //-------------------------------------------------------------
    //! @enum  BossAreaShape
    //! @brief 予兆として足元に出す範囲の形
    //-------------------------------------------------------------
    enum class BossAreaShape : int {
        Fan = 0,         //!< 足元を要にした前方の扇（薙ぎ払い）
        DiscForward,     //!< 前方に置いた円（叩きつけ）
        DiscAtTarget,    //!< 技の始めに決めた地点の円（跳びかかりの着地点）
        Ring,            //!< 自分を中心にした大きな円（咆哮）
        Line,            //!< 前方へ伸びる帯（突進の進路）
        None,            //!< 予兆を出さない（旋回など）
    };

    //-------------------------------------------------------------
    //! @enum  BossMoveKind
    //! @brief 技の種類（体の動かし方）
    //-------------------------------------------------------------
    enum class BossMoveKind : int {
        Strike = 0,    //!< その場（または跳びかかり）で攻撃モーションを振る。判定は体のボーン
        Charge,        //!< 予兆の後、決めた向きへ一直線に駆け抜ける。駆けている間ずっと判定が出る
        Projectile,    //!< 予兆の間に下がり、判定の時刻にプレイヤーへ向けて弾を撃つ
        Orbit,         //!< 攻撃せず、プレイヤーの周りを一定の距離で回る（息継ぎ）
    };

    //-------------------------------------------------------------
    //! @struct BossMoveHitbox
    //! @brief  技1つぶんの攻撃判定（EnemyAttackHitboxComponentへ書き写す値）
    //! @note   半径はボスの大きさの倍率を、ダメージは攻撃力の倍率を掛けてから使う
    //-------------------------------------------------------------
    struct BossMoveHitbox {
        std::string bone     = "mixamorig:RightHand";    //!< 判定の始点（球なら中心）のボーン
        std::string endBone;                             //!< 判定の終点のボーン。空なら球
        float       radius   = 45.0f;                    //!< 判定の半径（倍率を掛ける前）
        float       damage   = 20.0f;                    //!< ダメージ（倍率を掛ける前）
        float       start    = 0.5f;                     //!< 技を始めてから判定が出るまで（秒）。この間が予兆になる
        float       duration = 0.3f;                     //!< 判定が出ている時間（秒）
    };

    //-------------------------------------------------------------
    //! @struct BossMoveDef
    //! @brief  技1つぶんの定義
    //-------------------------------------------------------------
    struct BossMoveDef {
        std::string    name;                           //!< 技の名前（ログ・激昂の技の指定に使う）
        std::string    clip;                           //!< 再生するクリップ（Assetsのパス）
        int            animationIndex = -1;            //!< クリップの中のアニメーション番号。負ならPrefabのanimationIndex
        float          minRange      = 0.0f;           //!< プレイヤーとの距離がこの範囲にあるときだけ選ぶ
        float          maxRange      = 300.0f;
        float          weight        = 1.0f;           //!< 選ぶときの重み。0なら自分からは選ばない（激昂の咆哮など）
        float          cooldown      = 0.0f;           //!< この技を使ってから次に選べるまで（秒）
        float          length        = 2.0f;           //!< 技を始めてから終わるまで（秒）。クリップの長さに合わせる
        float          recovery      = 0.8f;           //!< 技の後の隙（秒）。この間は動かない
        BossMoveHitbox hitbox;
        BossAreaShape  area          = BossAreaShape::Fan;
        BossMoveKind   kind          = BossMoveKind::Strike;
        bool           loopClip      = false;          //!< クリップをループ再生するか（突進の走りなど、クリップより技が長いとき）

        float          leapStart       = -1.0f;        //!< 跳びかかり：移動を始める時刻（秒）。負なら移動しない
        float          leapEnd         = -1.0f;        //!< 跳びかかり：着地する時刻（秒）
        float          leapMaxDistance = 900.0f;       //!< 跳びかかり：一度に跳べる最大距離

        int            summonCount  = 0;               //!< 判定が出た瞬間に周りへ呼ぶ雑魚の数
        float          summonRadius = 400.0f;          //!< 呼ぶ雑魚を置く距離
        std::string    summonPrefab;                   //!< 呼ぶ雑魚のPrefab（Assets/Prefabs/ からの相対名）。空ならSmallZombie
        float          summonSizeScale   = 1.0f;       //!< 呼ぶ雑魚の大きさの倍率
        float          summonHealthScale = 1.0f;       //!< 呼ぶ雑魚のHPの倍率

        float          chargeStart    = 0.6f;          //!< 突進：駆け出す時刻（秒）。ここで向きを固定する
        float          chargeDuration = 0.8f;          //!< 突進：駆けている時間（秒）
        float          chargeSpeed    = 1400.0f;       //!< 突進：速さ（ユニット/秒）

        int            projectileCount  = 3;           //!< 弾：一度に撃つ数
        float          projectileSpread = 25.0f;       //!< 弾：両端の弾の開き（度）
        float          projectileSpeed  = 900.0f;      //!< 弾：速さ（ユニット/秒）
        float          projectileDamage = 10.0f;       //!< 弾：ダメージ（攻撃力の倍率を掛ける前）
        float          projectileRadius = 40.0f;       //!< 弾：当たりの半径
        float          projectileRange  = 1600.0f;     //!< 弾：届く距離
        float          slowSeconds      = 3.0f;        //!< 弾：当たったときの鈍足の長さ（秒）。0なら鈍足にしない
        float          slowScale        = 0.5f;        //!< 弾：鈍足中の移動速度の倍率
        float          retreatSpeed     = 0.0f;        //!< 弾：予兆の間に後ろへ下がる速さ（ユニット/秒）

        float          orbitSeconds = 3.0f;            //!< 旋回：回っている時間（秒）
        float          orbitRadius  = 500.0f;          //!< 旋回：プレイヤーから保つ距離（大きさの倍率を掛ける前）
        float          orbitSpeed   = 300.0f;          //!< 旋回：回る速さ（ユニット/秒）

        int            enragedRepeat = 1;              //!< 激昂中に続けて出す回数

        float          altitude          = -1.0f;      //!< 空を飛ぶボス：この技の間の高さ。負なら表の hoverHeight
        std::string    projectilePrefab;               //!< 弾のPrefab（Assets/Prefabs/ からの相対名）。空なら Enemy/WebShot
        bool           projectileAimDown = false;      //!< 弾：上下方向もプレイヤーの胸へ向けて撃つ（空中から撃ち下ろす）
    };

    //-------------------------------------------------------------
    //! @struct BossEnrageDef
    //! @brief  激昂（HPが減ると強くなる）の定義
    //-------------------------------------------------------------
    struct BossEnrageDef {
        float       healthRatio     = 0.5f;     //!< HPの割合がこれを切ったら激昂する。0以下なら激昂しない
        std::string move            = "Roar";   //!< 激昂の合図に出す技の名前
        float       moveSpeedScale  = 1.35f;    //!< 激昂中の移動速度の倍率
        float       cooldownScale   = 0.6f;     //!< 激昂中の技のクールタイムと隙の倍率
        float       fogDensityScale = 1.0f;     //!< 激昂中に夜霧を濃くする倍率（1なら変えない）
    };

    //-------------------------------------------------------------
    //! @struct BossBreakDef
    //! @brief  ブレイク（体勢崩し）の定義。体勢ゲージが満タンになるとダウンする
    //-------------------------------------------------------------
    struct BossBreakDef {
        float       gaugeRatio          = 0.12f;    //!< 体勢ゲージの最大値（最大HPに対する割合）。0以下ならブレイクしない
        float       growthPerBreak      = 0.25f;    //!< ダウンするたびに最大値を増やす割合（2回目は1.25倍…）
        float       decayRatioPerSecond = 0.04f;    //!< 何もしないと1秒あたりに減る量（最大値に対する割合）
        float       decayDelay          = 2.0f;     //!< 最後に当ててから減り始めるまで（秒）
        float       downSeconds         = 4.0f;     //!< ダウンしている時間（秒）
        std::string clip;                           //!< ダウン中に再生するクリップ。空ならのけぞりのクリップ
        int         animationIndex = -1;            //!< クリップの中のアニメーション番号。負ならPrefabのanimationIndex
        float       clipEnd        = 0.0f;          //!< クリップをこの秒数で止める（0なら最後まで）。倒れ込みの途中で止めて膝をつかせる
        float       recoverSeconds = 0.7f;          //!< ダウンが明けてから、その場で起き上がって動き出すまで（秒）
        float       recoverBlend   = 0.5f;          //!< 起き上がるとき、ダウンの姿勢から待機へ移す切り替えの秒数（短いと瞬間移動に見える）
    };

    //-------------------------------------------------------------
    //! @struct BossSecondForm
    //! @brief  第二形態の定義。HPが0になったら倒れずに変身し、技の表を差し替えて戦い直す
    //-------------------------------------------------------------
    struct BossSecondForm {
        std::string  key;                               //!< 第二形態の技の表の名前（Bosses/<key>.json）。空なら第二形態は無い
        std::wstring displayName;                       //!< 第二形態の呼び名（告知の帯・ボスHUDに出す）
        float        sizeMultiplier   = 1.4f;           //!< 体の大きさに掛ける倍率
        float        healthRatio      = 1.0f;           //!< 戻すHP（最大HPに対する割合）
        float        transformSeconds = 3.0f;           //!< 変身にかける秒数（無敵）
        float        collapseSeconds  = 1.0f;           //!< 変身のうち、崩れ落ちている秒数（この後に浮き上がって大きくなる）
        std::string  idleClip;                          //!< 第二形態の待機のクリップ。空なら今のまま
        std::string  walkClip;                          //!< 第二形態の移動のクリップ。空なら今のまま
    };

    //-------------------------------------------------------------
    //! @struct BossMoveSet
    //! @brief  ボス1体ぶんの技の表
    //-------------------------------------------------------------
    struct BossMoveSet {
        std::vector<BossMoveDef> moves;
        BossEnrageDef            enrage;
        BossBreakDef             breakDef;
        float                    turnSpeed      = 4.0f;    //!< 予兆の間にプレイヤーへ向き直る速さ（ラジアン/秒）
        float                    chaseStopRange = 180.0f;  //!< 追いかけるのをやめて立ち止まる距離
        BossSecondForm           secondForm;

        //-------------------------------------------------------------
        // 空を飛ぶボス（hoverHeight > 0）。技の間は高く浮き、技の後の隙とダウン中は landAltitude まで降りる
        //-------------------------------------------------------------
        float hoverHeight   = 0.0f;      //!< 普段の高さ（大きさの倍率を掛ける前）。0なら地上のボス
        float hoverBob      = 0.0f;      //!< 浮いている間の上下の揺れ幅
        float hoverBobSpeed = 1.6f;      //!< 上下の揺れの速さ（ラジアン/秒）
        float landAltitude  = 0.0f;      //!< 降りたときの高さ
        float climbSpeed    = 4.0f;      //!< 目標の高さへ寄せる速さ（1/秒、指数で寄せる）
    };

    //-------------------------------------------------------------
    //! @brief  ボスの技の表を得る（初回に Assets/Tables/Bosses/<key>.json を読み、以後は覚えておく。
    //!         Debugでは保存し直すと数値を読み直す。技の数・名前・クリップの変更は再起動が要る）
    //! @param  key [in] ボスの名前（例："Vampire"）
    //! @return 技の表。ファイルが無ければ技が空の表
    //-------------------------------------------------------------
    [[nodiscard]]
    const BossMoveSet& GetBossMoveSet(const std::string& key);

    //-------------------------------------------------------------
    //! @brief  技の名前から番号を引く
    //! @return 見つからなければ -1
    //-------------------------------------------------------------
    [[nodiscard]]
    int FindBossMove(const BossMoveSet& set, const std::string& name);
}    // namespace CombatAndroid::ECS
