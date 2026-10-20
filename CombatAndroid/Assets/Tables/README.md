# Tables

ゲームの調整値テーブル。起動後、そのテーブルが初めて参照されたときに読む（`ECS/Utility/Table/TableJson.hpp` の `LoadTableJson`）。
**数値を変えるだけなら再ビルドは要らない**。Debug ビルドなら起動したまま保存し直すだけで反映される（下の「ホットリロード」）。
Release は起動し直せば反映される。

- 書式は「ルートキー → 種類名 → 値」。種類名は C++ の enum と同じ綴り（`Warhammer` `Greed` `SmallZombie` など）。
  名前の並びは各 `*Table.cpp` の名前配列（`kWeaponKeys` など）が持つ
- キーが欠けていても落ちない。その項目は既定値のまま読み、`Logs/Tsukino.log` に `Table not found` / `... is missing` などが出る。
  値を変えたら一度起動してログを確かめること
- 画面に出す文字列（`displayName` `description`）は UTF-8 のまま書いてよい

| ファイル | 読むコード | 中身 |
|---|---|---|
| `WeaponLevels.json` | `WeaponTable.cpp` | 武器の表示名と、Lv1〜5 の基礎ダメージ（`damage` は `kMaxWeaponLevel` 個ちょうど） |
| `WeaponEvolution.json` | `WeaponEvolutionTable.cpp` | 進化の条件（スキル名とLv）と、進化後の名前・倍率。倍率 1.0 / 貫通段階 0 は「変えない」 |
| `Skills.json` | `SkillTable.cpp` | スキルの名前・カード絵・アイコン・色と、Lv1〜5 の説明文と効果量（`levels` は `kMaxSkillLevel` 個ちょうど） |
| `EnemySpawn.json` | `EnemySpawnTable.cpp` | 敵ごとの出現比重と解禁秒数 |
| `EnemyDifficulty.json` | `EnemyDifficultyTable.cpp` | 危険度ランクごとの倍率と、終端より先の1段ぶんの増分 |
| `Elite.json` | `EliteEnemy.cpp` | エリートの出現率・強化倍率・同時数・発光色・武器の並べ方・敵ごとの呼び名 |
| `Sounds.json` | `SoundTable.cpp` | 効果音のパス・音量・最短間隔 |
| `UiTextSize.json` | `UiTextSize.cpp` | 画面の文字の大きさの6段階（Display / Title / Heading / Large / Body / Small）。文字はどれもこのどれかを使うので、1つ変えればその段階の文字が全画面でそろって変わる。各 System の調整値には文字の大きさを置かない |
| `Bosses/<名前>.json` | `BossMoveTable.cpp` | ボスの技の表（技ごとのクリップ・判定・予兆の形・出す距離・クールタイム・跳びかかり・呼ぶ雑魚の数）と激昂。`BossDirector.json` の `behavior` で指す。予兆の形は `Fan` `DiscForward` `DiscAtTarget` `Ring` `Line` `None`、技の種類 `kind` は `Strike`（その場で振る）`Charge`（突進）`Projectile`（弾。当たると鈍足）`Orbit`（旋回）。判定の半径と技を出す距離・旋回の半径にはボスの `sizeScale` が掛かる（突進と弾の速さ・距離には掛からない）。ダメージ0の技は体の判定を出さない |
| `Systems/<名前>.json` | `ECS/System/<領域>/<名前>System.cpp` | System ごとの演出・挙動の調整値（下の「System の調整値」） |

敵1体の素の値（HP・移動速度・当たり判定など）はテーブルではなく Prefab（`Assets/Prefabs/Enemy/<名前>/`）が持つ。
Paladin の武器ごとの攻撃は `Assets/Prefabs/Enemy/PaladinWeaponAttacks.json`。

## 種類を足すとき

- **武器**：`WeaponId` と `WeaponTable.cpp` の `kWeaponKeys` に1つ → `WeaponLevels.json` / `WeaponEvolution.json` /
  `PaladinWeaponAttacks.json` に1項目 → `Assets/Prefabs/Weapon/<名前>/` の Prefab と `WeaponSpawner.cpp` の `kWeaponPrefabNames`
- **スキル**：`SkillId` と `SkillTable.cpp` の `kSkillKeys` に1つ → `Skills.json` に1項目 → 効果を反映する `RecalculateSkillStats` の case 文
- **敵**：`EnemyTypeId` と `EnemySpawnTable.cpp` の `kEnemyTypes`（名前と生成関数）に1行 → `EnemySpawn.json` と `Elite.json` の `displayNames` に1項目
- **効果音**：`SoundId` と `SoundTable.cpp` の `kSoundKeys` に1つ → `Sounds.json` に1項目（音源は `Assets/Audio/generate_game_sounds.py`、戦闘中の音は `generate_combat_sounds.js`）

## System の調整値（`Systems/`）

System の `.cpp` の無名 namespace にある調整値は、ここへ出す。今あるのは

- UI：`GameLog` `TitleStage` `TitleMenu` `SkillSelect` `RunResult` `Tutorial` `PauseMenu` `InputPrompt` `ScreenFade`
- HUD：`PlayerHud` `PlayerSkillHud` `HealthBar` `PlayerDamageEffect` `BossHud`（画面下中央のボスのHP・体勢ゲージ）
- 戦闘・演出：`ExpOrb` `Pickup` `DamageNumber` `PaladinWeaponSwitch` `EnemyWeaponDrop` `EnemyAttackTelegraph` `EnemyAttackArea`
  `Combat` `EnemyAnimation` `PlayerAnimation` `AttackMotionBlur` `GroundVisual` `HitSound` `ChargeSound` `HitImpactEffect` `EliteIndicator`
  `PickupIndicator`
- ボス戦の駆け引き：`BossCombat`（ジャスト回避の受付・反撃・後隙・ダウン中の倍率と、隙を知らせる縁取りの色）。
  ボスごとの体勢ゲージ（ゲージの量・ダウンの秒数・ダウンのクリップ）は `Bosses/<名前>.json` の `break`
- 進行：`BossDirector`（中ボス・ラスボスの出る時間と中身・出現のカットシーン・告知）、
  `NightTransition`（ボス戦の夜。球の広がる速さ・夜の色合い・夜の太陽／空／霧）

出していない System（`EnemyStressTest` `WeaponGripDebug` `GrassField` `TpsCamera` `Projectile` `EnemySpawnDirector`）は、
デバッグ専用か、残っているのが下の「残す」ものだけか、調整値を既に Prefab 側に持っているもの。

JSON の書式の補足：

- 色は `{ "x":…, "y":…, "z":…, "w":… }`（float3 は `w` 無し）。`w` はアルファ
- `GameLog.json` の `accentColors` は種別名（`GameLogCategory` の綴り）ごとの色
- `TitleStage.json` の `launchStops` は武器の並び（`WeaponId` 順）の配列。**要素数は武器の数ちょうど**にする
- 1 回ぶんの送り（`GameLog` の段、`SkillSelect` のカード、`PlayerSkillHud` の行）は高さ＋隙間から計算するので、キーは無い

### 揃えておく値

別の System の JSON 同士で、見た目のつながりのために同じ値にしてあるもの。片方を変えたらもう片方も合わせる。

| 値 | 揃える相手 |
|---|---|
| `PlayerSkillHud.skillListLeftX` / `skillListTopY` | `PlayerHud` のバーの左端 / EXP バーの下端＋余白12（`hpBarTopY + hpBarHeight + expBarGapY + expBarHeight + 12`） |
| `EnemyAttackArea.areaColor` | `EnemyAttackTelegraph.telegraphColor`（体の光と足元の範囲を同じ赤にする） |
| `Tutorial.panelColor` | `GameLog.panelColor`（同じ黒い半透明） |
| `PickupIndicator.indicatorColor` | `Pickup.rimColor`（拾得アイテムのリムグローと同じシアン） |

`InputPrompt` の案内の横位置は `SkillSelect.cardWidth` から計算するので、揃える必要は無い。
`SkillAura`（体の輪郭の発光）と `SkillAuraParticle`（足元から立ちのぼる光の粒）の色も `Skills.json` の `panelColor` を
そのまま使う（`saturation` で鮮やかにしてから `colorBoost` 倍する）ので、色を変えたいときは `panelColor` 側を触る。

- **出す**：見た目・手触り・挙動の調整値（速度・時間・距離・高さ・色・強度・閾値）
- **残す**：数学定数（`kPi` など）／配列サイズ・分割数・プール数など構造に効くもの／
  他のコードや Prefab と値を揃える必要があるもの（`ExpOrbSystem` の必要EXP、`EnemyWeaponDropSystem` の接地高さ）

出し方（`ExpOrbSystem.cpp` が見本）:

1. 無名 namespace の `constexpr` を、既定値つきの構造体 `<名前>Params` のメンバへ移す（**既定値は元の値のまま**。コメントも移す）
2. `load()` を書き、全メンバを `LoadField` で読む。キー名は `k` を外した lowerCamel（`kFallSafetyDuration` → `fallSafetyDuration`）
3. `GetParams()` で `return GetSystemParams<T>("<名前>");`（`TableJson.hpp`）を返し、使う側は毎回 `GetParams()` から `params.xxx` を読む
   （メンバ変数やラムダへ値を写すと、ホットリロードしても変わらない）。
   `Initialize` があればそこで1度呼んでおく（初回フレームでファイルを読まないため）
4. `Systems/<名前>.json` をルートキー `<名前>` で書く。色などのベクトルは `{ "x":…, "y":…, "z":… }`（Prefab と同じ書式）

キーが欠けていてもコードの既定値で動く。ファイルが無い・壊れているときは `Table not found` / `Table is broken` がログに出る。

## ホットリロード（Debug のみ）

Debug ビルドで起動している間、テーブルの JSON を保存し直すと 0.5〜1 秒で読み直す。再起動は要らない。
仕組みはエンジン側にある（どのゲームでも使える）。変更の検知は `Tsukino::IO::FileWatcher`（更新時刻のポーリング）、
読み直しと差分のログは `Tsukino::IO::HotReload`、値のキャッシュは `Tsukino::IO::HotReloadable<T>`。
`EngineAPI::Update` がシーンの更新の前にだけ読み直すので、システムの途中で値が変わることはない。
起動直後のロード画面の間は止めてある（`LoadingScene.cpp`）。
有効にするかは `DebugFeatures.hpp` の `TSUKINO_ENABLE_HOT_RELOAD`（Debug の既定。`TSUKINO_DISABLE_HOT_RELOAD` で落とせる）。
このゲーム側に残っているのは、表の置き場所（`TablePath`）と `GetSystemParams<T>`（`TableJson.hpp`）、ボスの技の並びの検査だけ。

`Logs/Tsukino.log` にはこう出る。

```
[Info] Hot reloaded: CombatAndroid/Assets/Tables/Bosses/Vampire.json
[Info] Hot reload changed: CombatAndroid/Assets/Tables/Bosses/Vampire.json  break.downSeconds: 4 -> 6.5
```

- `Hot reload changed:` は前に読めた中身との差分で、1回に最大20行まで出す。中身が同じなら `(no value changed)` と出る
- **JSON が壊れていたら前の値のまま動き続ける**（`Table is broken` と `Hot reload failed, keeping previous values`）。
  直して保存すれば読み直す
- まだ一度も参照されていない表（例：出てくる前のボスの `Bosses/<名前>.json`）は監視していないが、初めて参照されたときに最新の中身を読むので困らない

**AI が実行中に値を調整するとき**は、`bin\Debug\CombatAndroid.exe --autostart` を起動したまま JSON を書き換え、
`Logs/Tsukino.log` の `Hot reload changed:` の行で、意図した値が入ったかを確かめる。
画面は見えないので、挙動まで確かめたいときは、その値を読んでいる箇所に一時的なログを足す（確かめたら消す）。
ボスを早く出したいときは `Systems/BossDirector.json` の `midBossSeconds` を小さくし、`midBoss` の `prefab` / `behavior` を出したいボスにすればよい
（これもホットリロードで効く）。終わったら JSON を元に戻すこと。

### その場で反映されるもの・されないもの

「次から」と書いたものは、読み直した後に作られた・計算し直された分から効く（今いる敵や持っている武器は前の値のまま）。

| ファイル | 反映 |
|---|---|
| `Systems/*.json` | その場で反映される。例外は `EnemyAnimation` と `PlayerAnimation`。システムを作ったときにクリップの切り替えへ値を写すので、**次の走行から**（シーンを作り直したとき） |
| `Bosses/<名前>.json` | 技の数値（長さ・判定・予兆・クールタイム・距離・突進・弾）、激昂、`break`（体勢ゲージ・ダウン秒数）、`turnSpeed` などはその場で反映される。**技の数・並び・名前・`clip` を変えたときは読み直しを拒否する**（`Restart required` が出る。湧いたボスは技の番号でクリップとクールタイムを持っているため）。この変更は再起動で反映する |
| `EnemySpawn.json` | その場で反映される（次の抽選から） |
| `EnemyDifficulty.json` | 危険度の段の時刻はその場で反映される。HP などの倍率は湧いたときに敵へ掛けるので、**次に湧く敵から** |
| `Elite.json` | 出現率・同時数はその場で反映される。強化倍率は**次に湧くエリートから** |
| `WeaponLevels.json` / `WeaponEvolution.json` | 武器のダメージは拾ったときやレベルが上がったとき（`RecalculateWeaponStats`）に計算するので、**次に拾う・上がる・進化する武器から**。表示名などはその場で反映される |
| `Skills.json` | カードの説明文・絵はその場で反映される。効果量はスキルを取ったとき（`RecalculateSkillStats`）に計算するので、**次にスキルを取ったときから** |
| `Sounds.json` | 音量・最短間隔はその場で反映される。`path` は初めて鳴らしたときに読んだものを使い続けるので、**再起動が要る** |
| `UiTextSize.json` | 文字を作り直したときから（すでに出ている文字はそのまま） |
| Prefab（`Assets/Prefabs/`）・`PaladinWeaponAttacks.json` | 対象外。生成時にしか読まないので**再起動が要る** |

新しく表を足すときは、関数内 `static const` で値を覚えず、`Tsukino::IO::HotReloadable<T>`（パスは `TablePath`）か `GetSystemParams<T>` を使えば同じように読み直せる。

## 数値を触るときの約束

読み込み時に検査し、破っていれば `Log::Error` を出す（ゲームは止めない）。

- **EnemyDifficulty**
  - 最初の段は解禁 0 秒・倍率すべて 1.0（Prefab の素の値）
  - 解禁秒数は昇順、各倍率は下がらない
  - `knockbackThresholdScale` は `healthScale` を超えない。閾値が HP より速く伸びると
    「ひるむ一撃＝必ず致死」になり、ノックバックのモーションが再生されなくなる（`continuation` も同じ）
  - `continuation.unlockTimeSeconds` は読まない（`rankIntervalSeconds` が使われる）
- **Elite**：`thresholdScale` は `healthScale` 以下。閾値だけ伸ばすと一撃で怯まない硬さが体力以上に跳ね上がる
- **Skills**：`description` はそのままカードに出るので、`value` を変えたら文言も合わせる

### 数値の狙い（元のコードのコメントから）

- **EnemyDifficulty**：プレイヤーの与ダメージは走行中に約3.2倍まで伸びる（ウォーハンマー 38→70 で1.84倍 × 憤怒Lv5 の1.75倍）。
  HP はそのわずかに先を行かせ、危険度10の圧力の主役は湧き間隔の詰め（3.0秒→0.6秒）にしている。
  攻撃倍率を中盤以降で上向きにしてあるのは、傲慢（被ダメージ-30%＝実効HP1.43倍）を相殺するため。
  ただし傲慢を引けない周回もあるので、相殺は控えめ（危険度10で 1.64→1.85）に留めてある
- **EnemySpawn**：比重は相対値（合計を100に揃える必要はない）。比重0の敵は抽選に出ないが表には残せる。
  BigZombie は序盤30秒は出さない。Paladin は報酬が大きいので比重5（おおよそ20体に1体）で、序盤60秒は出さない
- **Elite**：危険度2から出始め、1段ごとに+1%（上限10%）。湧き間隔が中盤で約1秒なので、5%でおよそ1分に3体。ボス戦中は率1.5倍
- **Skills / 傲慢**：刻みを先細りにしているのは、被ダメージ軽減が実効HPに対して非線形なため（-30%は1.43倍、-50%なら2.0倍）
- **Skills / 怠惰**：唯一の「諸刃」スキル。回復量は等間隔、攻撃力ペナルティ（`value2`）の伸びは先細りにしてあり、伸ばしきる方が報われる
- **WeaponEvolution**：ウォーハンマー×憤怒は衝撃波を広げて吹き飛ばす、グレートソード×傲慢は刃を長く太く、
  バトルアックス×嫉妬は斬撃弾を溜め1段目から貫通させ、太く遠くまで飛ばす
- **Sounds**：`minInterval` は同じ音が同じ瞬間に重なって割れるのを防ぐためのもの。撃破音は一度に何体も出るので少し長め
