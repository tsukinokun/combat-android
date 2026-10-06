# CombatAndroid — AI エージェント向けガイド

自作エンジン **TsukinoEngine**（`External/TsukinoEngine`、submodule）を使う C++20 / DX11 の
3D アクション。エンジン側の規約とAPIの引き方は `External/TsukinoEngine/CLAUDE.md` を見る。
ここには**ゲーム側の話だけ**を書く（規約はコピーしない）。

## ビルドと実行

```
.\build.bat            Debug をビルド
.\build.bat Release
.\run.bat              ビルド済みexeを起動（引数はそのまま渡る）
```

- **MSBuild を直接叩かない。** 静音フラグを忘れると数千行出る。`build.bat` は成功時 0 行、
  失敗時はエラー行だけを返す。NuGet の復元も必要なときだけ自動で走る。
- **ソースファイルを新規追加したら** `External\TsukinoEngine\vendor\premake5.exe vs2022`
  で再生成する（`build.bat` は .sln が無いときしか再生成しない）。IDE を開くなら `open.bat`。
- 実行して結果を見たいときは `Tsukino::Core::Log::SetLogFile("Logs/Tsukino.log")` を呼ぶ。
  呼ばないと Log は `OutputDebugStringA` にしか出ず、「Prefab file not found」のような
  致命的な警告が完全に不可視になる。
- **一時ログ・調査用の出力は `Logs/` 配下に出す。** リポジトリ直下に `ofstream` で吐かない
  （`diag_*.txt` は gitignore 済み）。

## 読まない・grep しない場所

`.claude/settings.json` と `.ignore` で機械的に塞いであるが、理由を書いておく。

| 場所 | 理由 |
|---|---|
| `CombatAndroid/Assets/**/*.efkproj` | テキストXML。101ファイルで約129万行あり、grep の結果が壊れる |
| `External/TsukinoEngine/External/` | vendored 3rd party 約4,600ファイル・180MB（Effekseer / Jolt / entt ほか） |
| `.build/` `bin/` `bin-int/` `Cache/` | ビルド生成物 |
| `.claude/worktrees/` | 孤立した git worktree。丸ごとの複製チェックアウト（約4.5GB）で、上記の除外パターンはリポジトリ直下基準のためここには届かない |
| `Logs/` | 実行ログ。デバッグ時に意図的に Read するのは正規の手順だが、23MB超あるので grep 対象からは外す |

`.fbx` などアセットのファイル名は Glob で普通に引ける。`.efkproj` の一覧だけは
除外に入っているので `ls CombatAndroid/Assets/Effect` で取ること（中身は開かない）。

エンジンの API はヘッダを片端から読まず `External/TsukinoEngine/Docs/` の索引を引く。

## コードの地図

`CombatAndroid/src/` と `CombatAndroid/include/CombatAndroid/` が対になっている。

| ディレクトリ | 中身 |
|---|---|
| `ECS/Component/<領域>/` | データのみ |
| `ECS/System/<領域>/` | ロジック（`*System.hpp` / `.cpp` のペア） |
| `ECS/Utility/<分類>/` | テーブル・スポナー・判定などの共通実装 |
| `ECS/Event/<領域>/` | イベント定義 |
| `ECS/Serialization/<領域>/` | Prefab の読み書き。エンジン側コンポーネントの分は `BuiltIn/`、共通の補助は `Common/` |
| `ECS/AI/` | 敵の行動（ビヘイビアツリー） |
| `Scene/` | `CombatAndroidScene.cpp`（シーン構築）と `CombatAndroidSceneSystems.cpp`（システム登録） |
| `UI/` | `UiSortOrder.hpp`（描画の重なり順） |

Component・System・Event・Serialization は同じ領域名で分けてある（namespace はどれも `CombatAndroid::ECS`。フォルダは include のパスにだけ効く）:

| 領域 | 入っているもの |
|---|---|
| `Player/` | プレイヤーの入力・移動・アニメーション |
| `Enemy/` | 敵本体・湧き・攻撃予兆・Paladin の武器持ち替え |
| `Weapon/` | 武器のデータ・落ちている武器の拾得・敵が落とす武器 |
| `Combat/` | 当たり判定（`CombatSystem`）・HP・ヒットストップ・斬撃弾 |
| `Progression/` | 走行の時計・EXP 玉・スキル選択・経験値とスキルの所持 |
| `UI/` | 戦闘中の HUD（HP バー・ダメージ数値・取得ログ・操作案内・画面外の矢印・チュートリアル） |
| `Menu/` | 画面の流れ（タイトル・ポーズ・リザルト・暗転・カットシーン） |
| `World/` | カメラ・地面・草・霧 |
| `Effect/` | モーションブラー・ヒットの火花・被弾の赤フラッシュ |
| `Audio/` | 効果音の鳴らし分け |
| `Debug/` | 握り位置の調整・武器レベル表示・負荷試験 |

Utility の分類は `Table/`（調整値テーブル）・`UI/`（メニュー・スプライトの部品）・`Spawn/`・`Asset/`（Prefab・先読み）・
`Combat/`・`Time/`（スロー・停止）・`AI/`・`Save/`（設定・記録）・`Audio/`（BGM）・`Common/`。
新しいファイルは一番近い領域へ置き、直下には置かない。

**名前から辿れないもの**（ここを知らないと探索が空振りする）:

- **当たり判定は3か所に分かれている。** 共通実装 `ECS/Utility/Combat/CombatHit.cpp`、
  プレイヤー武器側 `ECS/System/Combat/CombatSystem.cpp`、敵側 `ECS/AI/ZombieBehavior.cpp`（Paladin も含む）
- `EnemyBehaviorSystem.cpp` はディスパッチャだけ。中身は `ZombieBehavior.cpp`
- `PlayerAnimationSystem.cpp` はアニメだけでなく攻撃コンボの状態遷移も持つ
- 武器・スキル・敵は**テーブル駆動**。調整値は `CombatAndroid/Assets/Tables/*.json`（再ビルド不要）、
  エンティティの初期値は `CombatAndroid/Assets/Prefabs/`。数値を C++ に戻さない。武器種ごとにクラスを増やさない
- System の無名 namespace にある演出・挙動の調整値は `Assets/Tables/Systems/<名前>.json` へ出してよい
  （線引きと手順は `Assets/Tables/README.md`）
- システムの実行順とその理由は `ECS/SystemPriority.hpp` に集約してある

## よくある作業

- **システムを1本足す** → `/add-system` を使う（触るのは常に同じ5ファイル）
- **武器・スキル・敵を足す** → enum と `ECS/Utility/Table/*Table.cpp` の名前配列に1つ、
  `Assets/Tables/*.json` に1項目（手順は `Assets/Tables/README.md`）
