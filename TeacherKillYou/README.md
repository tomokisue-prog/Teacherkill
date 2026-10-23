# TeacherKillYou

raylibを使用した学校脱出ホラーゲームです。Visual Studioで `TeacherKillYou.sln` を開き、x64のDebugまたはReleaseでビルドします。
起動時の作業フォルダは、このREADMEと `Data` があるプロジェクトフォルダです。

## ソースの配置

`main.cpp` が起動入口です。ゲームの実装は `Source` にまとめ、同じクラスの `.h` と `.cpp` を同じ役割のフォルダへ配置しています。
ヘッダーに宣言、cppに実装を置きます。Visual Studioのフォルダ表示も実際の配置に合わせています。

| フォルダ | 内容 |
| --- | --- |
| `Source/Core` | ゲーム全体の状態、共通寸法、設定 |
| `Source/Scenes` | タイトル、プレイ、クリア、シーン遷移 |
| `Source/Actors/Player` | プレイヤーの移動、銃、HP、スタミナ |
| `Source/Actors/Enemy` | 敵のAI、移動、攻撃、アニメーション |
| `Source/World` | ステージ、ドア、テレポート、ゴール |
| `Source/Camera` | プレイヤー用・デバッグ用カメラ |
| `Source/Items` | アイテムとインベントリ |
| `Source/Audio` | BGM、SE、足音、環境音、敵の唸り |
| `Source/Graphics` | ライト、画面効果、血のエフェクト |
| `Source/Resources` | モデル、マテリアル、FBXとアニメーションの読み込み |
| `Source/UI` | 日本語フォント、会話、ポーズ、ゲームオーバー画面 |
| `Source/Save` | シーン配置、設定、チェックポイントの保存と読み込み |
| `Source/Debug` | ImGuiのデバッグ操作とギズモ |

`TeacherKillYou.vcxproj` の `GameIncludeDirectories` に、ゲーム用ヘッダーの検索先を定義しています。
既存の `#include "Player.h"` などはそのまま使えます。

## マップとキャラクターの大きさ

共通の寸法は [WorldScale.cpp](Source/Core/WorldScale.cpp) にあります。
値はこのマップのワールド単位で、メートル換算値ではありません。現行校舎の天井高約4.65を基準に、通路を通れる体格へ合わせています。

| 対象 | 高さ | 当たり判定の半径 |
| --- | ---: | ---: |
| プレイヤー | 2.90 | 0.36 |
| MONSTER | 3.10 | 0.42 |
| ゾンビ | 2.90 | 0.36 |

目線はプレイヤーの身長の91%です。敵のモデル表示と当たり判定は、同じ高さを参照します。
マップの描画倍率は1.25で、既存の配置を基準にキャラクターを合わせています。
変更前に作成したチェックポイントは、読み込み時に敵の寸法を移行してHPや所持品を引き継ぎます。

## データと外部ライブラリ

- `Data/Image`：マップ、キャラクター、銃、UI、テクスチャ。FBXと材質補助ファイルは同じフォルダで管理します。
- `Data/Audio`：BGM・SE。[音源の配置方法](Data/Audio/README.md)
- `Data/Font`：日本語フォント。
- `Data/Json`：会話データ。
- `Data/Save`：プレイヤー個人の設定と進行。Gitの管理対象外です。
- `scene.json`：配置編集用のシーンデータ。
- `imgui`、`imguizmo`、`rlImGui`、`nlohmann`、`third_party`、`packages`：外部ライブラリ。Visual Studioでは `External` に表示します。
- `tools`：素材の補助スクリプトと説明書。[ゲームの操作](tools/Gameplay.md) ／ [FBXの読み込み](tools/FBX.md)
- `build`、`x64`：検証用ファイルとビルド出力。

## 起動と読み込み

タイトルでは日本語フォントと音声を準備し、3D素材はゲーム開始後に進捗を表示しながら読み込みます。
タイトルへ戻って再開する場合、ResourceManagerのモデルとアニメーションは再読み込みしません。
進捗は素材の処理件数に基づき、残り時間ではありません。大きい素材の処理中は一時的に止まることがあります。
開発時にもFBX取り込みが遅くならないよう、Debug構成ではFBX取り込み用の2ファイルのみ最適化しています。
FbxLoader.cppとufbx.cのステップ実行では最適化の影響を受けます。ゲーム本体のDebug設定は従来どおりです。
実際に遊ぶ速度を確認する場合は、Visual Studioで `Release / x64` を選びます。
出力の `LOADING:` ログには、各素材の読み込みにかかった秒数が表示されます。
