#pragma once
#include "raylib.h"
#include <array>
#include <cstddef>
#include <string>

// AudioManager.cppのファイル名一覧と同じ順番で定義する。
enum class BgmId { Title, Game, Count };
enum class SeId { Shoot, Reload, PlayerHit, EnemyHit, EnemyAttack, EnemyDeath,
                  DoorOpen, DoorClose, Flashlight, Teleport, Confirm, DryFire, Count };

// BGMはストリーム再生、SEは起動時に読み込む。更新・解放はSceneManagerが担当する。
class AudioManager {
public:
    static AudioManager& GetInstance();
    bool Init(const std::string& directory = "Data/Audio");
    void Update(); // ストリームを途切れさせないため毎フレーム呼ぶ。
    void Shutdown();

    // 未配置の音はfalseを返すだけ。PlayBgmは曲が変わる場合に前の曲を止める。
    bool PlayBgm(BgmId id);
    void StopBgm();
    bool PlaySe(SeId id);
    void StopSe(SeId id);
    void StopAllSe();
    bool HasBgm(BgmId id) const;
    bool HasSe(SeId id) const;
    bool IsBgmPlaying() const;
    bool IsSePlaying(SeId id) const;
    bool IsReady() const { return ready_ && IsAudioDeviceReady(); }
    BgmId GetCurrentBgm() const { return currentBgm_; }

    void SetBgmVolume(float volume); // 0.0～1.0。変更値は再読み込み後も維持する。
    void SetSeVolume(float volume);
    float GetBgmVolume() const { return bgmVolume_; }
    float GetSeVolume() const { return seVolume_; }

private:
    AudioManager() = default;
    AudioManager(const AudioManager&) = delete;
    AudioManager& operator=(const AudioManager&) = delete;
    std::array<Music, static_cast<std::size_t>(BgmId::Count)> bgm_{};
    std::array<Sound, static_cast<std::size_t>(SeId::Count)> se_{};
    BgmId currentBgm_ = BgmId::Count;
    float bgmVolume_ = 0.35f;
    float seVolume_ = 0.75f;
    bool ready_ = false;
    bool ownsDevice_ = false; // 他の処理が初期化したデバイスは閉じない。
};

inline AudioManager& AM() { return AudioManager::GetInstance(); }
