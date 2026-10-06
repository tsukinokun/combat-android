# Fonts

ゲームの文字は2つの書体を使い分ける。見出しは世界側（中世の武器・聖騎士）の重い明朝、
それ以外は人造人間が見ている画面として、幅の広いすっきりしたゴシックにしている。

| 書体 | ファイル | 使うところ |
|---|---|---|
| Noto Serif JP Black（太い明朝） | `Heading.dfont` → `NotoSerifJP-Black.otf` | タイトル・画面の見出し（PAUSE・オプション・操作説明・リザルト・NOW LOADING）・メニューの選択肢・LEVEL UP! |
| Zen Kaku Gothic New Bold（ゴシック） | `Hud.dfont` → `ZenKakuGothicNew-Bold.ttf` | それ以外すべて（HUD・取得ログ・ダメージ数値・キー案内・チュートリアル・説明文） |

## どこで指定しているか

- 文字の Prefab（`Assets/Prefabs/UI/*/FontComponent.json`）の `fontHandle` に `.dfont` のパスを書く。
  見出しは `UI/HeadingText`・`UI/SkillHeadingText`、それ以外の文字 Prefab は全部 `Hud.dfont`
- C++ で文字を作るときは `CreateUiTextEntity(..., UiTextFont::Heading)` で見出し書体になる（既定は HUD 書体）
- `fontHandle` を書かない Prefab はエンジンの既定フォント（Noto Sans CJK JP）になる

## 注意

- **フォールバックが無い。** フォントに入っていない字は描かれない。新しい文言を足したら、使った字が両方の書体に入っているか確かめること
- `.dfont` の `Size` は文字の基準の大きさ（ピクセル）。画面上の大きさは「Size × 文字の段階の倍率（`Assets/Tables/UiTextSize.json`）」なので、既定フォントと同じ 32 に揃えてある。変えると、その書体を使う文字がすべて同じ比率で大きく/小さくなる
- 書体を入れ替えるときは ttf/otf を差し替えて `.dfont` の `SourceFile` を書き換える（`FaceName` はログ用の名前で、描画には使われない）。
  可変フォントは既定の太さで固定されるので、静的な ttf/otf を使う

## ライセンス

どちらも SIL Open Font License 1.1。配布物に同梱する義務があるので、ライセンス全文を一緒に置いてある。

- Noto Serif JP：Copyright 2017-2024 Adobe — `OFL-NotoSerifJP.txt`
  （入手元: https://github.com/notofonts/noto-cjk の `Serif/SubsetOTF/JP/` ）
- Zen Kaku Gothic New：Copyright 2022 The Zen Project Authors — `OFL-ZenKakuGothicNew.txt`
  （入手元: https://github.com/google/fonts/tree/main/ofl/zenkakugothicnew ）
