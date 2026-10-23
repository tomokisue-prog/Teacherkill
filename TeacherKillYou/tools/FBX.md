# FBXの直接読み込み

ResourceManager::LoadModel() へFBXのパスを渡すと、そのファイルからメッシュ、
材質、画像、ボーン、全アニメーションクリップをまとめて読み込む。
GLBへの変換や外部の変換ツールは不要。
バイナリFBXとASCII FBXの両方に対応する。

```cpp
RM().LoadModel(ResourceKeys::Model_Enemy, "Data/Image/Monster_final_animations/MONSTER_run_final.fbx");
```

Enemyを差し替えるときは、Source/Resources/ResourceManager.cpp の GameResources 一覧にある
Model_Enemy のパス1か所を変更する。同じキーを繰り返し読み込んでも解析は一度だけ。
FBXでは LoadModelAnimations() の追加呼び出しは不要だが、既存の呼び出しにも対応する。
読み込み失敗時は理由をログへ出し、モデルを登録しないため再試行できる。

画像はFBXへの埋め込み、またはFBXから参照する外部ファイルを使う。
元の制作PCのパスが使えない場合、FBXと同じフォルダー、その textures、
textures/packed の順で同名画像も探す。拡張子がない参照はPNG/JPEG/TGA/BMPも探す。
画像がなければ材質色で描画する。
旧MONSTERが参照する textures/packed/MONSTER_001 には、MONSTER_001.png を使用する。
この画像は以前の monster test.glb に埋め込まれていた元のMONSTER画像から取り出したもの。
走行版・攻撃版の両方で同じUV画像を使用する。

FBXの頂点法線と材質の透明度を保持し、面の内側で法線を補間して陰影を計算する。
透明度0の毛などは色・深度を書き込まない。CPU変形とGPU変形で同じ描画を使う。
色画像に加えて法線マップを読み込み、画像にはミップマップと三線形フィルターを使う。
制作時の硬いエッジを勝手に丸めたり、ポリゴンを削減したりはしない。

ボーンの親変換、逆バインド行列、非一様な縮尺を含む変形行列を読み込み時に17ms間隔で保持する。
再生時は次を使う。フレーム番号とクリップ番号は0から始まる。

```cpp
RM().ApplyModelAnimation(ResourceKeys::Model_Enemy, frame, animationIndex);
DrawModel(RM().GetModel(ResourceKeys::Model_Enemy), position, scale, WHITE);
```

時間のあるクリップを優先し、その中で名前に walk/run を含む歩行を先頭へ配置する。
静止takeも残すが、名前だけがwalkingの静止takeを移動用として選ばない。
Enemyの既存処理は移動中に先頭のクリップを再生する。
256個以下のスキン結合を持つアニメーション付きFBXでは、必要な本数だけ行列uniformを持つシェーダーでGPU変形を試みる。
端末のuniform容量が足りずシェーダーを生成できない場合はCPUで変形する。
GPUのボーン属性位置はraylibが名前から割り当てるため、シェーダーで番号を固定しない。
配布ライブラリの配置とずれると、腕や脚のアニメーションが反映されなくなる。
大きなメッシュはraylibの16bitインデックスに収まるように分割し、三角形を欠落させない。

この実装は頂点ごとに影響の大きい4つのボーンを使い、重みを正規化する。
モデル全体のスキン結合上限は256個。複数の骨格やメッシュでは、
同じボーンでも逆バインド行列が異なる結合を別々に数える。
複数メッシュが同じ骨格を使う場合は、ワールド座標系へ揃えた逆バインド行列まで一致する結合だけを共有する。
`zombie 1`のようにメッシュごとに同じ骨が含まれるFBXでも、重複登録で結合上限に達しないようにする。
モーフ、頂点キャッシュ、制作ソフト専用シェーダーは取り込まない。

FBXと同じフォルダーの`<FBX名>.materials.glb`を優先して、材質名を照合し、色・法線・
粗さ／金属度の画像とUV番号を補完する。補助ファイルがなければ従来どおり同名GLBを使う。
画像のBIN範囲だけを読み、GLBのメッシュは読み込まない。
MONSTER最終版はこの補完で`automap`（TEXCOORD_1）と新しい肉質マテリアルを使用する。
配布・コミットにはFBXと対応する`.materials.glb`を含めること。
MONSTERの元GLB（各約181MB）はローカルに残し、`.gitignore`で共有対象から外している。
材質専用版は約5MBで、画像の圧縮や縮小、モデルのポリゴン削減は行っていない。

元GLBから補助ファイルを再生成する場合は、次を実行する（ゲーム実行時にPythonは不要）。

```powershell
python tools/extract_fbx_materials.py "Data/Image/Monster_final_animations"
```

元ファイルは上書きしない。生成済みの内容を変更して再出力する場合だけ`--force`を付ける。

敵の種類は`EnemyConfig::type`で指定する。既定は`EnemyType::Monster`、
女性ゾンビは`EnemyType::FemaleZombie`。各種類が専用のモデル・攻撃・死亡クリップを使う。
`FemaleZombie`という既存のID・リソースキーは互換性のため維持し、現在は新しい`zombie 1`を割り当てている。
移動は`zombie 1 running.fbx`、攻撃・被弾・死亡は同じフォルダーの`attack`・`hit`・`death`を使う。
各FBXと同名のGLBから新モデルの材質・画像・UVを補完する。
被弾クリップがある敵は、生存中の被弾で攻撃を中断して被弾動作を再生し、終了後に追跡を再開する。
被弾動作の途中で再び撃たれてもHPは減るが、クリップは先頭へ戻さず最後まで再生する。
`EnemyConfig::animationSpeed`は全クリップ共通の再生倍率（既定1.5倍）。
攻撃の命中時刻・被弾の硬直時間・死亡動作の終了時刻も倍率に合わせる。死体の保持時間は秒指定を維持する。
致死ダメージは被弾動作より死亡動作を優先する。モンスターには従来どおり被弾クリップを割り当てない。
現在は初期配置に1体ずつ生成し、同じAI・当たり判定・HP処理で更新する。
敵の高さと中心は変形前メッシュではなく、最初のアニメーション姿勢から読み込み時に計算する。
AIが移動を担当する敵の歩行では、takeの水平移動だけを除去し、足の動きや上下動を保持する。

既存のGLB/glTFは引き続きraylibで読み込む。

FBX解析には third_party/ufbx のufbx v0.23.0を使用する。
ライセンスと取得元は同じフォルダーの LICENSE と README.md に記載している。
