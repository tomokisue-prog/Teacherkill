#include "AudioManager.h"
#include <algorithm>
#include <cmath>

namespace {
struct BgmAsset {
    const char* filename;
    float gain;
};
constexpr BgmAsset BgmAssets[] = {
    { "game_over_screen.mp3", 0.75f },
    { "Game Intro.mp3", 0.78f }
};
constexpr const char* SeFiles[] = {
    "shoot.mp3", "Reload.mp3", "Empty_GunShot.mp3", "Hitting_wall.mp3",
    "Player_Footstep.wav", "extra_footsteps/Player_Footstep_02.wav", "extra_footsteps/Player_Footstep_03.wav",
    "Female_monster_fottstep.wav", "extra_footsteps/Female_monster_fottstep_02.wav", "extra_footsteps/Female_monster_fottstep_03.wav", "MaleMonster_footstep.wav",
    "Female_Monster_hit.wav", "MaleMonster_hit.mp3", "MaleMonster_dead.mp3", "MaleMonster_dead2.wav",
    "Locked and Unlocked.wav", "pickup.ogg", "Healing.mp3", "heartbeat_30to100_volume.wav",
    "You and Died impact.mp3", "pause_menu.ogg", "hover_cursor.ogg", "confirm.ogg", "error_click.wav",
    "Player_hit1.wav", "Player_hit2.wav", "Player_dead.wav", "Thunder.mp3"
};
static_assert(sizeof(BgmAssets) / sizeof(BgmAssets[0]) == static_cast<std::size_t>(BgmId::Count), "BGM file list mismatch");
static_assert(sizeof(SeFiles) / sizeof(SeFiles[0]) == static_cast<std::size_t>(SeId::Count), "SE file list mismatch");
constexpr float FadeSeconds = 0.8f;
float Volume(float value, float fallback) {
    return std::isfinite(value) ? (std::max)(0.0f, (std::min)(1.0f, value)) : fallback;
}
std::string Path(const std::string& dir, const char* name) { return dir + "/" + name; }
}

AudioManager& AudioManager::GetInstance() { static AudioManager instance; return instance; }

bool AudioManager::Init(const std::string& directory) {
    Shutdown();
    const bool existingDevice = IsAudioDeviceReady();
    if (!existingDevice) InitAudioDevice();
    ready_ = IsAudioDeviceReady();
    ownsDevice_ = ready_ && !existingDevice;
    if (!ready_) { TraceLog(LOG_WARNING, "AUDIO: Device unavailable"); return false; }
    seGains_.fill(1.0f);

    const auto ambiencePath = Path(directory, "bgm_atmosphere_15percent volume.mp3");
    if (FileExists(ambiencePath.c_str())) {
        ambience_ = LoadMusicStream(ambiencePath.c_str());
        if (IsMusicValid(ambience_)) { ambience_.looping = true; SetMusicVolume(ambience_, ambienceVolume_); }
    } else TraceLog(LOG_WARNING, "AUDIO: Missing %s", ambiencePath.c_str());
    const auto windPath = Path(directory, "game cover wind_loop.wav");
    if (FileExists(windPath.c_str())) {
        wind_ = LoadMusicStream(windPath.c_str());
        if (IsMusicValid(wind_)) { wind_.looping = true; SetMusicVolume(wind_, 0.0f); }
    } else TraceLog(LOG_WARNING, "AUDIO: Missing %s", windPath.c_str());
    for (std::size_t i = 0; i < bgm_.size(); ++i) {
        const auto path = Path(directory, BgmAssets[i].filename);
        if (!FileExists(path.c_str())) { TraceLog(LOG_WARNING, "AUDIO: Missing %s", path.c_str()); continue; }
        bgm_[i] = LoadMusicStream(path.c_str());
        if (IsMusicValid(bgm_[i])) { bgm_[i].looping = true; SetMusicVolume(bgm_[i], 0.0f); }
    }
    for (std::size_t i = 0; i < se_.size(); ++i) {
        const auto path = Path(directory, SeFiles[i]);
        if (!FileExists(path.c_str())) { TraceLog(LOG_WARNING, "AUDIO: Missing %s", path.c_str()); continue; }
        se_[i] = LoadSound(path.c_str());
        if (IsSoundValid(se_[i])) SetSoundVolume(se_[i], seVolume_);
        else TraceLog(LOG_WARNING, "AUDIO: Could not load %s", path.c_str());
    }
    return true;
}

bool AudioManager::HasBgm(BgmId id) const {
    const auto i = static_cast<std::size_t>(id);
    return IsReady() && i < bgm_.size() && IsMusicValid(bgm_[i]);
}
bool AudioManager::HasSe(SeId id) const {
    const auto i = static_cast<std::size_t>(id);
    return IsReady() && i < se_.size() && IsSoundValid(se_[i]);
}
bool AudioManager::IsBgmPlaying() const {
    return HasBgm(currentBgm_) && IsMusicStreamPlaying(bgm_[static_cast<std::size_t>(currentBgm_)]);
}
bool AudioManager::IsSePlaying(SeId id) const {
    return HasSe(id) && IsSoundPlaying(se_[static_cast<std::size_t>(id)]);
}
bool AudioManager::PlayBgm(BgmId id) {
    if (!HasBgm(id)) return false;
    if (id == currentBgm_ && (IsBgmPlaying() || pausedBgm_)) return true;
    StopBgm();
    currentBgm_ = id;
    bgmFade_ = 0.0f;
    Music& music = bgm_[static_cast<std::size_t>(id)];
    SetMusicVolume(music, 0.0f);
    PlayMusicStream(music);
    if (paused_) { PauseMusicStream(music); pausedBgm_ = true; }
    return true;
}
void AudioManager::StopBgm() {
    if (HasBgm(currentBgm_)) StopMusicStream(bgm_[static_cast<std::size_t>(currentBgm_)]);
    currentBgm_ = BgmId::Count;
    pausedBgm_ = false;
    bgmFade_ = 0.0f;
}
bool AudioManager::PlayAmbience() {
    if (!IsReady() || !IsMusicValid(ambience_)) return false;
    if (IsAmbiencePlaying() || pausedAmbience_) return true;
    SetMusicVolume(ambience_, ambienceVolume_);
    PlayMusicStream(ambience_);
    if (paused_) { PauseMusicStream(ambience_); pausedAmbience_ = true; }
    return true;
}
void AudioManager::StopAmbience() {
    if (IsReady() && IsMusicValid(ambience_)) StopMusicStream(ambience_);
    pausedAmbience_ = false;
}
bool AudioManager::IsAmbiencePlaying() const {
    return IsReady() && IsMusicValid(ambience_) && IsMusicStreamPlaying(ambience_);
}
bool AudioManager::PlayWind() {
    if (!IsReady() || !IsMusicValid(wind_)) return false;
    if (windPlaying_) return true;
    windFade_ = 0.0f;
    windMix_ = 1.0f;
    SetMusicVolume(wind_, 0.0f);
    PlayMusicStream(wind_);
    windPlaying_ = true;
    if (paused_) { PauseMusicStream(wind_); pausedWind_ = true; }
    return true;
}
void AudioManager::StopWind() {
    if (IsReady() && IsMusicValid(wind_)) StopMusicStream(wind_);
    windPlaying_ = pausedWind_ = false;
    windFade_ = 0.0f;
    windMix_ = 1.0f;
}
void AudioManager::SetWindMix(float gain) {
    windMix_ = Volume(gain, windMix_);
    if (IsReady() && IsMusicValid(wind_))
        SetMusicVolume(wind_, bgmVolume_ * windFade_ * windMix_);
}
void AudioManager::Update() {
    if (!IsReady() || paused_) return;
    const float dt = std::isfinite(GetFrameTime()) ? (std::max)(0.0f, (std::min)(GetFrameTime(), 0.1f)) : 0.0f;
    if (IsMusicValid(ambience_) && IsMusicStreamPlaying(ambience_)) UpdateMusicStream(ambience_);
    if (windPlaying_ && IsMusicValid(wind_)) {
        windFade_ = (std::min)(1.0f, windFade_ + dt / FadeSeconds);
        // The default BGM slider is 0.35, so the wind plays at 35% by default.
        SetMusicVolume(wind_, bgmVolume_ * windFade_ * windMix_);
        UpdateMusicStream(wind_);
    }
    if (HasBgm(currentBgm_)) {
        bgmFade_ = (std::min)(1.0f, bgmFade_ + dt / FadeSeconds);
        const auto i = static_cast<std::size_t>(currentBgm_);
        Music music = bgm_[i];
        SetMusicVolume(music, bgmVolume_ * BgmAssets[i].gain * bgmFade_);
        UpdateMusicStream(music);
    }
}

bool AudioManager::PlayEffect(SeId id, float gain, float pitch, float pan, bool allowPaused) {
    if ((!allowPaused && paused_) || !HasSe(id)) return false;
    const auto i = static_cast<std::size_t>(id);
    if (allowPaused && paused_) pausedSe_[i] = false;
    seGains_[i] = Volume(gain, 1.0f);
    SetSoundVolume(se_[i], seVolume_ * seGains_[i]);
    SetSoundPitch(se_[i], std::isfinite(pitch) ? (std::max)(0.5f, (std::min)(2.0f, pitch)) : 1.0f);
    SetSoundPan(se_[i], Volume(pan, 0.5f));
    PlaySound(se_[i]);
    return true;
}
bool AudioManager::PlaySe(SeId id, float gain, float pitch, float pan) { return PlayEffect(id, gain, pitch, pan, false); }
bool AudioManager::PlayUiSe(SeId id, float gain) { return PlayEffect(id, gain, 1.0f, 0.5f, true); }
void AudioManager::SetSeGain(SeId id, float gain) {
    if (!HasSe(id)) return;
    const auto i = static_cast<std::size_t>(id);
    seGains_[i] = Volume(gain, 1.0f);
    SetSoundVolume(se_[i], seVolume_ * seGains_[i]);
}
void AudioManager::StopSe(SeId id) {
    const auto i = static_cast<std::size_t>(id);
    if (HasSe(id)) StopSound(se_[i]);
    if (i < pausedSe_.size()) pausedSe_[i] = false;
}
void AudioManager::StopAllSe() {
    pausedSe_.fill(false);
    if (IsReady()) for (Sound sound : se_) if (IsSoundValid(sound)) StopSound(sound);
}
void AudioManager::SetBgmVolume(float volume) {
    bgmVolume_ = Volume(volume, bgmVolume_);
    if (!IsReady()) return;
    if (IsMusicValid(wind_)) SetMusicVolume(wind_, bgmVolume_ * windFade_ * windMix_);
    if (HasBgm(currentBgm_)) {
        const auto i = static_cast<std::size_t>(currentBgm_);
        SetMusicVolume(bgm_[i], bgmVolume_ * BgmAssets[i].gain * bgmFade_);
    }
}
void AudioManager::SetSeVolume(float volume) {
    seVolume_ = Volume(volume, seVolume_);
    if (IsReady()) for (std::size_t i = 0; i < se_.size(); ++i)
        if (IsSoundValid(se_[i])) SetSoundVolume(se_[i], seVolume_ * seGains_[i]);
}
void AudioManager::SetAmbienceVolume(float volume) {
    ambienceVolume_ = Volume(volume, ambienceVolume_);
    if (IsReady() && IsMusicValid(ambience_)) SetMusicVolume(ambience_, ambienceVolume_);
}
void AudioManager::SetPaused(bool paused) {
    if (paused_ == paused) return;
    paused_ = paused;
    if (!IsReady()) return;
    if (paused) {
        pausedBgm_ = IsBgmPlaying();
        pausedAmbience_ = IsAmbiencePlaying();
        pausedWind_ = windPlaying_ && IsMusicValid(wind_) && IsMusicStreamPlaying(wind_);
        if (pausedBgm_) PauseMusicStream(bgm_[static_cast<std::size_t>(currentBgm_)]);
        if (pausedAmbience_) PauseMusicStream(ambience_);
        if (pausedWind_) PauseMusicStream(wind_);
        for (std::size_t i = 0; i < se_.size(); ++i) {
            pausedSe_[i] = HasSe(static_cast<SeId>(i)) && IsSoundPlaying(se_[i]);
            if (pausedSe_[i]) PauseSound(se_[i]);
        }
    } else {
        if (pausedBgm_ && HasBgm(currentBgm_)) ResumeMusicStream(bgm_[static_cast<std::size_t>(currentBgm_)]);
        if (pausedAmbience_ && IsMusicValid(ambience_)) ResumeMusicStream(ambience_);
        if (pausedWind_ && IsMusicValid(wind_)) ResumeMusicStream(wind_);
        for (std::size_t i = 0; i < se_.size(); ++i) if (pausedSe_[i] && HasSe(static_cast<SeId>(i))) ResumeSound(se_[i]);
        pausedBgm_ = pausedAmbience_ = pausedWind_ = false;
        pausedSe_.fill(false);
    }
}
void AudioManager::Shutdown() {
    StopBgm(); StopAmbience(); StopWind(); StopAllSe();
    paused_ = pausedBgm_ = pausedAmbience_ = pausedWind_ = false;
    if (IsAudioDeviceReady()) {
        if (IsMusicValid(ambience_)) UnloadMusicStream(ambience_);
        if (IsMusicValid(wind_)) UnloadMusicStream(wind_);
        for (Music music : bgm_) if (IsMusicValid(music)) UnloadMusicStream(music);
        for (Sound sound : se_) if (IsSoundValid(sound)) UnloadSound(sound);
    }
    ambience_ = {}; wind_ = {}; bgm_ = {}; se_ = {};
    if (ownsDevice_ && IsAudioDeviceReady()) CloseAudioDevice();
    ready_ = ownsDevice_ = false;
}
