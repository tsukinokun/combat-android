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
| `External/TsukinoEngine/External/` | vendored 3rd party 19,000ファイル（Effekseer / Jolt / entt ほか） |
| `.build/` `bin/` `bin-int/` `Cache/` | ビルド生成物 |

`.fbx` などアセットのファイル名は Glob で普通に引ける。`.efkproj` の一覧だけは
除外に入っているので `ls CombatAndroid/Assets/Effect` で取ること（中身は開かない）。

エンジンの API はヘッダを片端から読まず `External/TsukinoEngine/Docs/` の索引を引く。

## コードの地図

`CombatAndroid/src/` と `CombatAndroid/include/CombatAndroid/` が対になっている。

| ディレクトリ | 中身 |
|---|---|
| `ECS/Component/` | データのみ（32個） |
| `ECS/System/` | ロジック（26個。`*System.hpp` / `.cpp` のペア） |
| `ECS/Utility/` | テーブル・スポナー・判定の共通実装 |
| `ECS/Event/` `ECS/AI/` | イベント定義 / 敵の行動（ビヘイビアツリー） |
| `Scene/` | `CombatAndroidScene.cpp`（シーン構築）と `CombatAndroidSceneSystems.cpp`（システム登録） |
| `UI/` | `UiSortOrder.hpp`（描画の重なり順） |

**名前から辿れないもの**（ここを知らないと探索が空振りする）:

- **当たり判定は3か所に分かれている。** 共通実装 `ECS/Utility/CombatHit.cpp`、
  プレイヤー武器側 `ECS/System/CombatSystem.cpp`、敵側 `ECS/AI/ZombieBehavior.cpp`（Paladin も含む）
- `EnemyBehaviorSystem.cpp` はディスパッチャだけ。中身は `ZombieBehavior.cpp`
- `PlayerAnimationSystem.cpp` はアニメだけでなく攻撃コンボの状態遷移も持つ
- 武器・スキル・敵は**テーブル駆動**。`ECS/Utility/WeaponTable.cpp` などの配列を直す。
  武器種ごとにクラスを増やさない
- システムの実行順とその理由は `ECS/SystemPriority.hpp` に集約してある

## よくある作業

- **システムを1本足す** → `/add-system` を使う（触るのは常に同じ5ファイル）
- **武器・スキル・敵を足す** → 対応する `ECS/Utility/*Table.cpp` の配列に1行
