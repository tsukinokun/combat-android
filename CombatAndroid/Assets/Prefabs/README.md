# Prefabs

エンティティの調整値（Component のパラメータ）を JSON で持つ。エンジンの `PrefabFactory` が読み込む。
ゲーム側 Component の登録は `CombatAndroid/src/ECS/Utility/GamePrefab.cpp`（`RegisterGameComponents`）。
JSON の書式は `Prefab.json`（コンポーネント名 → 個別 JSON のパス）＋コンポーネントごとの JSON。

**注意：アタッチだけするComponent（`AnimationController` / `SkeletonOutput` / `PlayerExperience` / `PlayerSkill`）は
保存する項目が無いので `CaptureEntity` では書き出されず、`Prefab.json` には `"value": "null"` と手で書いてある。
`CaptureEntity` で `Player/Prefab.json` を書き直すとこの行が消え、キャラクターがTポーズのまま動かず
`PlayerAnimationSystem` が `GetComponent<AnimationControllerComponent>` で落ちる。書き直したら必ず戻すこと。**

**手で書かず、`PrefabFactory::CaptureEntity(registry, entity, outDir)` で生きているエンティティから書き出すと楽。**
フィールドを足したときは `ECS/Serialization/*ComponentSerialization.hpp` の save と load の両方に足す
（load は `LoadField` で読むので、古い JSON にキーが無くても既定値のまま読める）。

## Environment/

`InstantiateEnvironment(registry, context, "Combat" | "Title")`（`GamePrefab.hpp`）が
Ground / Sun / Sky / Fog / Particles / Grass を生成する。空（`Sky`）だけ両シーン共通。
`UiCamera2D` は Title / Combat / Loading の3シーンで共通（`InstantiateUiCamera2D`）。

1ユニット≒1cm。長さの次元を持つ値はこの縮尺で入れている。

- **Fog**：戦闘範囲（〜500）は素通しで、それより奥を霞ませる。敵の湧き半径（900〜1300）を隠す役目は
  `heightDensity` ではなく `density`（`startDistance` 以遠にだけ効く）に寄せてある。`heightDensity` は地面高さに
  一様にかかるので、上げると足元の草の色まで灰色に潰れる。色は HDR 値で、草より明るいと「霞み」でなく「上塗り」になる。
  タイトルは色を同じにして、濃さだけ薄く（`density` 0.0005）・遠く（`startDistance` 900）から掛けている。
- **Grass**：近景（一辺 `fieldSize`）と遠景（`farFieldSize`）の2層。`lodBlendStart/End` は敵の湧き半径の外側に置き、
  通常の戦闘中に切替帯へ入らないようにしてある。風向き（`windDirection`）は **Fog と必ず揃える**
  （ずれると霧と草が別々の風になびく）。`fadeStartRatio` は 1.0 未満にすること。
  種の色は空由来のアンビエントが青に偏っている分、B を低めに置いている。
- **Particles**：火の粉。暖色の HDR 加算で、上昇させるのが肝。横方向は Fog の風と揃える。
- **Ground**：戦闘は Kinematic の箱（`GroundFollowSystem` がプレイヤーへ追従させる）。タイトルは見た目用でコライダー無し。
- **Sun**：タイトルは武器が逆光で潰れないよう正面から当てる（戦闘と向きが逆）。
