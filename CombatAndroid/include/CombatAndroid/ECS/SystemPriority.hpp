//-------------------------------------------------------------
//! @file    SystemPriority.hpp
//! @brief   CombatAndroidのシステム実行順の定義
//! @detail  値そのものより「なぜその順なのか」が重要なため、各項目に理由を添えています。
//!          並びを変えるときは必ず理由を読んでから変更してください。
//! @author  山﨑愛
//-------------------------------------------------------------
#pragma once
// 名前空間 : CombatAndroid::ECS
namespace CombatAndroid::ECS {
    enum class SystemPriority : int {
        RunClock = -5,                // 走行の経過時間と危険度ランクを進める。全システムの先頭に置く。
                                      // EnemySpawnが同じフレームでこの危険度を読んで敵を強化し、
                                      // PlayerHudが同じ値を表示するため、ここが最初に進んでいないと
                                      // 「画面に出ている危険度」と「実際に湧いた敵の危険度」が1フレームずれる
        EnemySpawn = -4,               // サバイバーの湧き潰し（フォグの外から比重抽選で敵を湧かせる）。
                                      // 生成・破棄はStressTestと同じく全システムの先頭で済ませ、その回の
                                      // フレームからTransform/Animation/Physicsが新しい敵を正しく扱えるように
                                      // する。StressTestより手前に置くのは、F1で負荷試験が数を作り直す前に
                                      // 本Systemの生存数の数え上げを終わらせておくため
        StressTest = -3,              // （負荷試験ビルドのみ）敵の大量スポーン。生成・破棄を
                                      // 全システムの先頭で済ませることで、その回のフレームから
                                      // Transform/Animation/Physicsが新しい敵を正しく扱える
        MotionVectorSnapshot = -2,    // モーションブラー用に前フレームのworldMatrix/ボーン行列を退避する。
                                      // TransformSystem・AnimationSystemが今フレームの値を書く「前」に読むことで、
                                      // ダブルバッファなしに前フレームの値を取り出している。ここより後ろへ動かすと
                                      // 速度が常にゼロになりブラーが効かなくなる
        Transform = 0,    // 一番最初に計算する
        SkillSelect,      // レベルアップ時のスキル選択メニュー。PlayerSystem（Movement）が
                          // 同じフレームのスペース（回避）・ホイール（武器切替）・左クリック（攻撃）を
                          // 消費する前に割り込んで入力を横取りする必要があるため、Movementより前に置く。
                          // 併せてメニュー表示中はCharacterControllerComponent::moveInputを毎フレーム潰す：
                          // PhysicsSystemはdeltaTimeが0以下でも1/60秒ぶん必ずステップするため、
                          // シーン側でdeltaTime=0にするだけではキャラクタが滑り続けてしまう
        Movement,         // プレイヤー入力・敵AIの移動をTransformの後、Physicsの前に反映する
        Gameplay,         // ダメージ処理・アニメーション更新は移動確定後に行う
        WeaponGripDebug,  // （デバッグビルドのみ）握り位置調整はisAttackingを上書きするため、
                          // PlayerAnimationSystem（Gameplay）が今フレームのisAttackingを確定させた後、
                          // CombatSystem（WeaponAttach）がそれを読む前に割り込む
        WeaponAttach,     // 武器の追従（ボーンアタッチ）はAnimationSystemが今フレームのボーン姿勢を書き込んだ後に行う
        Projectile,       // 斬撃弾の移動と当たり判定。弾を生成するCombatSystem（WeaponAttach）の後、
                          // ヒットの結果を読むHealthBar/DamageNumberの前に置く。この並びでないと
                          // 撃った弾が飛び始めるのも、HPバーとダメージ数値が出るのも1フレーム遅れる
        HealthBar,        // 頭上HPバーの表示可否・残量幅の更新。被弾（WeaponAttachでCombatSystemが
                          // hpBarVisibleTimerをセット）の後、WorldAnchorSystemが座標を確定させる前に行う
        DamageNumber,     // ダメージ数値のスロット割り当てとアニメーション更新。被弾（WeaponAttachで
                          // CombatSystemがWeaponHitEventをPublish）の後でなければ表示が1フレーム遅れる。
                          // またWorldAnchorSystemが座標を確定させ、TransformUIがworldMatrixへ焼き込む前に
                          // fixedWorldPosition/screenOffset/scaleを書き終えている必要がある
                          // （さもないとポップの拡大率が1フレーム古い値で描かれてガタつく）
        ExpOrb,           // EXP玉のスロット割り当てと落下→ホーミング→吸収の状態更新。敵の死亡通知
                          // （EnemyDiedEvent、Movementで動くBTから発火）の後、かつプレイヤーの今フレームの
                          // 位置（Movementで確定済み）を吸い寄せ先に使うため、DamageNumberと同じ並びでよい
        EnemyWeaponDrop,    // 敵が持っていた武器を地面へ落とす。EXP玉と同じくEnemyDiedEventを
                            // 購読するだけなので、ExpOrbと同じ並びでよい
        PlayerHud,        // 画面左上のHP/EXPバー更新。HP（WeaponAttachでCombatSystemが確定）とEXP
                          // （ExpOrbが確定）の両方より後に置く
        TransformLate,    // Movement/WeaponAttachで更新したposition/rotationをworldMatrixへ反映する2回目のTransformSystem。
                          // これが無いと、このフレームで更新された所有者の回転がworldMatrix（描画に使われる）へ
                          // 反映されるのは次フレームになり、武器はowner.rotationを直接読むため1フレーム分
                          // 先行してしまい、回転中（旋回中）だけ武器の位置が体からずれて見える不具合が起きる
        Camera3D,         // TPS/デバッグカメラの追従は移動確定後、カメラ行列計算の前に行う
        Camera,           // カメラ行列は描画前に計算する
        WorldAnchor,      // WorldAnchorComponentを持つエンティティ（拾得プロンプト・HPバー等）の
                          // ワールド→スクリーン座標変換は今フレームのカメラ行列を使うためCameraの後に置く
        TransformUI,      // WorldAnchorSystemが書いたUI要素のposition（画面ピクセル座標）をworldMatrixへ反映する。
                          // FontRendererSystemはworldMatrix[3]を読むため、これが無いと1フレーム遅れて表示がスウィムする
        AttackMotionBlur,    // 攻撃の進行度（CombatSystemが更新するattackBlend）をブラー強度へ反映する。
                             // WeaponAttachより後、MotionBlurより前
        MotionBlur,          // ブラーパラメータをRendererへ転送し、MotionVectorComponentを自動アタッチする。
                             // ModelSystem（Render）が描画コマンドを積む前である必要がある
        Render,
        Font,    // 文字の描画コマンドを積む。SpriteRenderSystem（Render）との前後関係は
                 // もう登録順では決まらない：RendererがOverlayパスを DrawCommand::sortOrder で
                 // 並べ替えるため、スプライトと文字の重なりは
                 // CombatAndroid/UI/UiSortOrder.hpp の層の値だけで決まる。
                 // ここがRenderの後ろにあるのは同じ層同士の並びが積んだ順で決まる名残であり、
                 // 正しさの条件ではない。一方でworldMatrixを書くTransformUIより後、という
                 // 条件は引き続き必須（FontRendererSystemはworldMatrix[3]から描画位置を読む）
        Audio,
        Physics,    // コリジョンの更新は最後に行う
        Light,      // ディレクショナル/点光源/スポットをまとめてRendererへ渡す。
                    // worldMatrixから位置を取るのでTransform系より後である必要がある
        SkyAtmosphere,
        Fog,    // フォグはカメラ位置と太陽方向を使うため、Camera / Light より後に置く
    };
}    // namespace CombatAndroid::ECS
