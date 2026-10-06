# Tables

ゲームの調整値テーブル。起動後、そのテーブルが初めて参照されたときに1度だけ読む（`ECS/Utility/Table/TableJson.hpp` の `LoadTableJson`）。
**数値を変えるだけなら再ビルドは要らない**（ゲームを起動し直せば反映される）。

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
| `Systems/<名前>.json` | `ECS/System/<領域>/<名前>System.cpp` | System ごとの演出・挙動の調整値（下の「System の調整値」） |

敵1体の素の値（HP・移動速度・当たり判定など）はテーブルではなく Prefab（`Assets/Prefabs/Enemy/<名前>/`）が持つ。
Paladin の武器ごとの攻撃は `Assets/Prefabs/Enemy/PaladinWeaponAttacks.json`。

## 種類を足すとき

- **武器**：`WeaponId` と `WeaponTable.cpp` の `kWeaponKeys` に1つ → `WeaponLevels.json` / `WeaponEvolution.json` /
  `PaladinWeaponAttacks.json` に1項目 → `Assets/Prefabs/Weapon/<名前>/` の Prefab と `WeaponSpawner.cpp` の `kWeaponPrefabNames`
- **スキル**：`SkillId` と `SkillTable.cpp` の `kSkillKeys` に1つ → `Skills.json` に1項目 → 効果を反映する `RecalculateSkillStats` の case 文
- **敵**：`EnemyTypeId` と `EnemySpawnTable.cpp` の `kEnemyTypes`（名前と生成関数）に1行 → `EnemySpawn.json` と `Elite.json` の `displayNames` に1項目
- **効果音**：`SoundId` と `SoundTable.cpp` の `kSoundKeys` に1つ → `Sounds.json` に1項目（音源は `Assets/Audio/generate_game_sounds.py`）

## System の調整値（`Systems/`）

System の `.cpp` の無名 namespace にある調整値は、ここへ出す。今あるのは

- UI：`GameLog` `TitleStage` `TitleMenu` `SkillSelect` `RunResult` `Tutorial` `PauseMenu` `InputPrompt` `ScreenFade`
- HUD：`PlayerHud` `PlayerSkillHud` `HealthBar` `PlayerDamageEffect`
- 戦闘・演出：`ExpOrb` `Pickup` `DamageNumber` `PaladinWeaponSwitch` `EnemyWeaponDrop` `EnemyAttackTelegraph` `EnemyAttackArea`
  `Combat` `EnemyAnimation` `PlayerAnimation` `AttackMotionBlur` `GroundVisual` `HitSound` `ChargeSound` `HitImpactEffect` `EliteIndicator`
  `PickupIndicator`

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

- **出す**：見た目・手触り・挙動の調整値（速度・時間・距離・高さ・色・強度・閾値）
- **残す**：数学定数（`kPi` など）／配列サイズ・分割数・プール数など構造に効くもの／
  他のコードや Prefab と値を揃える必要があるもの（`ExpOrbSystem` の必要EXP、`EnemyWeaponDropSystem` の接地高さ）

出し方（`ExpOrbSystem.cpp` が見本）:

1. 無名 namespace の `constexpr` を、既定値つきの構造体 `<名前>Params` のメンバへ移す（**既定値は元の値のまま**。コメントも移す）
2. `load()` を書き、全メンバを `LoadField` で読む。キー名は `k` を外した lowerCamel（`kFallSafetyDuration` → `fallSafetyDuration`）
3. `GetParams()` で `LoadSystemParams<T>("<名前>")`（`TableJson.hpp`）を関数内 static に1度だけ読み、使う側は `params.xxx`。
   `Initialize` があればそこで1度呼んでおく（初回フレームでファイルを読まないため）
4. `Systems/<名前>.json` をルートキー `<名前>` で書く。色などのベクトルは `{ "x":…, "y":…, "z":… }`（Prefab と同じ書式）

キーが欠けていてもコードの既定値で動く。ファイルが無い・壊れているときは `Table not found` / `Table is broken` がログに出る。

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
- **Elite**：危険度2から出始め、1段ごとに+1%（上限10%）。湧き間隔が中盤で約1秒なので、5%でおよそ1分に3体。ラスト1分は率1.5倍
- **Skills / 傲慢**：刻みを先細りにしているのは、被ダメージ軽減が実効HPに対して非線形なため（-30%は1.43倍、-50%なら2.0倍）
- **Skills / 怠惰**：唯一の「諸刃」スキル。回復量は等間隔、攻撃力ペナルティ（`value2`）の伸びは先細りにしてあり、伸ばしきる方が報われる
- **WeaponEvolution**：ウォーハンマー×憤怒は衝撃波を広げて吹き飛ばす、グレートソード×傲慢は刃を長く太く、
  バトルアックス×嫉妬は斬撃弾を溜め1段目から貫通させ、太く遠くまで飛ばす
- **Sounds**：`minInterval` は同じ音が同じ瞬間に重なって割れるのを防ぐためのもの。撃破音は一度に何体も出るので少し長め
