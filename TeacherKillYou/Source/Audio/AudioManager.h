#pragma once
#include "raylib.h"
#include <array>
#include <cstddef>
#include <string>

// Only the files supplied in the new sound package are used.
enum class BgmId { GameOver, Title, Count };
enum class SeId {
    Shoot, Reload, DryFire, WallImpact,
    PlayerFootstep1, PlayerFootstep2, PlayerFootstep3,
    FemaleFootstep1, FemaleFootstep2, FemaleFootstep3, MaleFootstep,
    FemaleHit, MaleHit, MaleDeath, MaleDeath2,
    Door, Pickup, Healing, Heartbeat,
    YouDied, PauseMenu, Hover, Confirm, Error,
    PlayerHit1, PlayerHit2, PlayerDeath, Thunder,
    Count
};

class AudioManager {
public:
    static AudioManager& GetInstance();
    bool Init(const std::string& directory = "Data/Audio");
    void Update();
    void Shutdown();

    bool PlayBgm(BgmId id);
    void StopBgm();
    void SetChaseActive(bool active);
    void StopChase();
    bool PlayAmbience();
    void StopAmbience();
    bool IsAmbiencePlaying() const;
    bool PlayWind();
    void StopWind();
    void SetWindMix(float gain);

    bool PlaySe(SeId id, float gain = 1.0f, float pitch = 1.0f, float pan = 0.5f);
    bool PlayUiSe(SeId id, float gain = 1.0f);
    void SetSeGain(SeId id, float gain);
    void StopSe(SeId id);
    void StopAllSe();
    bool HasBgm(BgmId id) const;
    bool HasSe(SeId id) const;
    bool IsBgmPlaying() const;
    bool IsSePlaying(SeId id) const;
    bool IsReady() const { return ready_ && IsAudioDeviceReady(); }
    void SetPaused(bool paused);
    bool IsPaused() const { return paused_; }
    void SetBgmVolume(float volume);
    void SetSeVolume(float volume);
    void SetAmbienceVolume(float volume);
    float GetBgmVolume() const { return bgmVolume_; }
    float GetSeVolume() const { return seVolume_; }

private:
    AudioManager() = default;
    AudioManager(const AudioManager&) = delete;
    AudioManager& operator=(const AudioManager&) = delete;
    bool PlayEffect(SeId id, float gain, float pitch, float pan, bool allowPaused);

    std::array<Music, static_cast<std::size_t>(BgmId::Count)> bgm_{};
    std::array<Sound, static_cast<std::size_t>(SeId::Count)> se_{};
    Music ambience_{};
    Music wind_{};
    Music chase_{};
    std::array<float, static_cast<std::size_t>(SeId::Count)> seGains_{};
    std::array<bool, static_cast<std::size_t>(SeId::Count)> pausedSe_{};
    BgmId currentBgm_ = BgmId::Count;
    float bgmVolume_ = 0.35f;
    float seVolume_ = 0.75f;
    float ambienceVolume_ = 0.3f;
    float windFade_ = 0.0f;
    float windMix_ = 1.0f;
    float bgmFade_ = 0.0f;
    float chaseFade_ = 0.0f;
    bool paused_ = false;
    bool pausedBgm_ = false;
    bool pausedAmbience_ = false;
    bool pausedWind_ = false;
    bool pausedChase_ = false;
    bool chaseWanted_ = false;
    bool chasePlaying_ = false;
    bool windPlaying_ = false;
    bool ready_ = false;
    bool ownsDevice_ = false;
};
inline AudioManager& AM() { return AudioManager::GetInstance(); }
