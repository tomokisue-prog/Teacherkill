# FBXの直接読み込み

ResourceManager::LoadModel() へFBXのパスを渡すと、そのファイルからメッシュ、
材質、画像、ボーン、全アニメーションクリップをまとめて読み込む。
GLBへの変換や外部の変換ツールは不要。
バイナリFBXとASCII FBXの両方に対応する。

```cpp
RM().LoadModel(ResourceKeys::Model_Enemy, "Data/Image/MONSTER   run.fbx");
```

Enemyを差し替えるときは、ResourceManager.cpp の LoadAll() 内にある
Model_Enemy のパス1か所を変更する。同じキーを繰り返し読み込んでも解析は一度だけ。
FBXでは LoadModelAnimations() の追加呼び出しは不要だが、既存の呼び出しにも対応する。
読み込み失敗時は理由をログへ出し、モデルを登録しないため再試行できる。

画像はFBXへの埋め込み、またはFBXから参照する外部ファイルを使う。
元の制作PCのパスが使えない場合、FBXと同じフォルダー、その textures、
textures/packed の順で同名画像も探す。拡張子がない参照はPNG/JPEG/TGA/BMPも探す。
画像がなければ材質色で描画する。
MONSTERが参照する textures/packed/MONSTER_001 には、MONSTER_001.png を使用する。
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

クリップ名に walk が含まれる場合、歩行を先頭へ配置する。他のクリップも保持する。
Enemyの既存処理は移動中に先頭のクリップを再生する。
128個以下のスキン結合を持つアニメーション付きFBXではGPUで頂点を変形し、
それを超える場合やシェーダーが使えない場合はCPUで変形する。
GPUのボーン属性位置はraylibが名前から割り当てるため、シェーダーで番号を固定しない。
配布ライブラリの配置とずれると、腕や脚のアニメーションが反映されなくなる。
大きなメッシュはraylibの16bitインデックスに収まるように分割し、三角形を欠落させない。

この実装は頂点ごとに影響の大きい4つのボーンを使い、重みを正規化する。
モデル全体のスキン結合上限は256個。複数の骨格やメッシュでは、
同じボーンでも逆バインド行列が異なる結合を別々に数える。
モーフ、頂点キャッシュ、制作ソフト専用シェーダーは取り込まない。

既存のGLB/glTFは引き続きraylibで読み込む。

FBX解析には third_party/ufbx のufbx v0.23.0を使用する。
ライセンスと取得元は同じフォルダーの LICENSE と README.md に記載している。
