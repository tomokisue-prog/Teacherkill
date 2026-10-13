# BGM・SEの配置

下の名前で音声ファイルを置き、ゲームを再起動すると読み込まれます。
未配置の音は無音で動作します。空のWAVなどを作る必要はありません。

## BGM（ループ再生）

| 配置するファイル | 再生する場面 |
| --- | --- |
| `BGM/title.ogg` | タイトル画面 |
| `BGM/game.ogg` | ゲーム中 |

BGMはストリーム再生し、画面を切り替えると前の曲を止めて次の曲を再生します。

## SE

| 配置するファイル | 再生する場面 |
| --- | --- |
| `SE/shoot.wav` | 弾を消費して射撃したとき |
| `SE/reload.wav` | リロードを開始したとき |
| `SE/player_hit.wav` | プレイヤーにダメージが入ったとき |
| `SE/enemy_hit.wav` | 敵にダメージが入ったとき |
| `SE/enemy_attack.wav` | 敵の攻撃アニメーションの命中判定のタイミング |
| `SE/enemy_death.wav` | 敵のHPが0になったとき（1回のみ） |
| `SE/door_open.wav` | ドアを開き始めたとき |
| `SE/door_close.wav` | ドアを閉め始めたとき |
| `SE/flashlight.wav` | Fキーで懐中電灯を切り替えたとき |
| `SE/teleport.wav` | フェード中に移動先へテレポートしたとき |
| `SE/confirm.wav` | タイトルからゲームを開始したとき |

同じファイル名で `.ogg` / `.mp3` / `.wav` / `.flac` を使えます。
BGMの優先順はOGG → MP3 → WAV → FLAC、SEはWAV → OGG → MP3 → FLACです。
SEは一度読み込んだ音を再利用します。同じSEを再生中に再び呼ぶと、先頭から再生します。

## プログラムからの操作

```cpp
#include "AudioManager.h"

AM().PlayBgm(BgmId::Game);
AM().PlaySe(SeId::Shoot);
AM().SetBgmVolume(0.35f); // 0～1
AM().SetSeVolume(0.75f);
AM().StopBgm();
AM().StopSe(SeId::Reload);
```

初期音量はBGMが0.35、SEが0.75です。
初期化・毎フレームのBGM更新・終了時の解放は `SceneManager` に組み込んであります。
追加する音のIDとファイル名は `AudioManager.h` と `AudioManager.cpp` に定義します。
音声デバイスを別の処理で用意する場合は、デバイスを閉じる前に `AM().Shutdown()` を呼んでください。
