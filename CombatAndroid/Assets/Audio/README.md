# Assets/Audio

## 効果音（生成物）

`generate_hit_sounds.py` / `generate_charge_sounds.py` / `generate_game_sounds.py` が
プロシージャル生成した `.wav` を置いています。音を変えたいときは、各スクリプトの頭にある
数値をいじって実行し直してください（標準ライブラリだけで動きます）。

```
python generate_game_sounds.py
```

どの場面でどの音を鳴らすかは `CombatAndroid/ECS/Utility/Table/SoundTable.cpp` の表にあります。

## BGM（ここへ置くと鳴ります）

BGM は長く鳴り続けるため合成音だと耳障りになりやすく、生成していません。
次の名前で置くと、そのまま鳴ります（置かなければ無音のまま動きます）。

| ファイル名 | 鳴る場面 |
|---|---|
| `Title.mp3` | タイトル画面 |
| `InGame.mp3` | 戦闘中 |

- **形式は `.wav` か `.mp3`** です（初回ロード時に `.xwb` へ変換されます。mp3 はその前に PCM へ展開）。
  ogg などそれ以外は wav か mp3 へ変換してから置いてください。ファイル名は `Bgm.hpp` の定数と合わせること
- ループ前提で鳴らすので、頭と尻が自然につながる素材が向いています
- 音量は `CombatAndroid/ECS/Utility/Audio/Bgm.hpp` の `kBgmVolume`（既定 0.35）で調整できます
- ファイルサイズの目安（.xwb は mp3 でも PCM 展開後の大きさになります）：44.1kHz・16bit・モノラルで 1 分あたり約 5MB
