# アイテム用の補完マテリアル

既存の画像がない材質だけに使用する。元のFBX・GLB・アイコンは変更しない。
対応するモデルキーと材質名、色、粗さ、金属度は `Source/Items/ItemMaterials.cpp` で調整する。
UVがない手続き型材質にも画像を表示できるよう、補完対象のメッシュにだけボックス投影を使う。
画像は共有し、ミップマップ付きで読み込み、ResourceManagerが一度だけ解放する。

- `worn_paint.png`: 本、PC、弾薬、回復キットなどの、傷と汚れがある塗装面。材質ごとの色を乗せる。
- `rusted_steel.png`: ホラー調の机の、錆びた鋼板面。

2026-11-06、組み込み image_gen を使用して新規生成。生成後の画像加工は行っていない。

## 生成プロンプト

### worn_paint.png

Create a single seamless tileable PBR base-color/albedo texture for worn painted school furniture, to be used on 3D furniture and small props in a realistic abandoned-school survival-horror game. Full frame orthographic flat material scan, no object, no perspective, no lighting gradients, no ambient occlusion, no shadows, no specular highlights. Neutral warm very light gray painted surface (average RGB around 190,185,172) so it can be tinted different colors in a game. Fine worn grain, subtle accumulated grime, delicate abrasions and scattered tiny paint chips revealing muted gray underneath. Restrained low-contrast variation, no large stains or distinct repeating features, no large rust. Edge-to-edge seamless tile, square 1024x1024, physically plausible scale, photographic surface detail. No text, labels, borders or watermark.

### rusted_steel.png

Create a single seamless tileable PBR base-color/albedo texture for old corroded steel furniture in an abandoned-school survival-horror game. Full frame orthographic flat material scan, no object, no perspective, no lighting gradients, no shadows or specular highlights. A believable mottled gray-brown steel surface with medium-fine reddish-brown rust, faded olive-gray paint remnants, and slight grime, restrained muted colors. Keep average brightness medium (not black), localized subtle rust blooms but no giant recognizable features, no drips dominating composition. Edge-to-edge seamless tile, square 1024x1024, photographic high-quality material scan. No text, labels, borders or watermark.
