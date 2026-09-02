---
description: CombatAndroid に ECS システムを1本追加する（Component / System / 優先度 / 登録の定型5点セット）
---

# システムを追加する

引数: $ARGUMENTS （システム名。`System` を含めない。例: `EnemyStun` → `EnemyStunSystem`）

名前が渡されていなければ、何をするシステムかを先に確認すること。

## 触るファイルはこの5つだけ

既存の似たシステムを1本だけ読んで真似る（全部読まない）。
軽いものは `CombatAndroid/src/ECS/System/HitStopSystem.cpp`、
イベント購読があるものは `CombatAndroid/src/ECS/System/ExpOrbSystem.cpp` が手本になる。

1. `CombatAndroid/include/CombatAndroid/ECS/Component/<名前>Component.hpp`
   データのみ。関数を持たせない。
2. `CombatAndroid/include/CombatAndroid/ECS/System/<名前>System.hpp`
   `Tsukino::ECS::ISystem` を継承する。イベントを購読するなら `Initialize(EventBus&)` を用意する。
3. `CombatAndroid/src/ECS/System/<名前>System.cpp`
4. `CombatAndroid/include/CombatAndroid/ECS/SystemPriority.hpp`
   enum に1行。**どの位置に入れるかの理由をコメントで必ず書く**（既存の全項目が理由付きになっている）。
   既存の並びの意味を読まずに末尾へ足さないこと。
5. `CombatAndroid/src/Scene/CombatAndroidSceneSystems.cpp`
   `RegisterSystems` に `m_scene.AddSystem(...)` を1行。
   イベント購読があるなら `{ auto s = std::make_shared<...>(); m_scene.AddSystem(s, ...); s->Initialize(eventBus); }` の形。

UI を描くシステムなら、重なり順のために
`CombatAndroid/include/CombatAndroid/UI/UiSortOrder.hpp` にも1行足す。

## 仕上げ

- **新規 .cpp を足したので premake の再生成が要る。** `build.bat` は `.build\*.sln` が
  無いときしか再生成しないので、`External\TsukinoEngine\vendor\premake5.exe vs2022` を明示的に実行する。
- `.\build.bat Debug` が 0 行で通ることを確認する。
- 規約（命名・コメント・include順）は `External/TsukinoEngine/CODING_GUIDELINES.md` と
  `tsukino-doc-comment` スキルに従う。
