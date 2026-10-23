#include "AudioManager.h"
#include <algorithm>
#include <cmath>

namespace {
// 音を追加するときはAudioManager.hのIDとこの一覧を同時に追加する。
constexpr const char* BgmFiles[] = { "title", "game" };
constexpr const char* SeFiles[] = { "shoot", "reload", "player_hit", "enemy_hit", "enemy_attack", "enemy_death",
                                   "door_open", "door_close", "flashlight", "teleport", "confirm", "dry_fire", "footstep", "enemy_growl" };
static_assert(sizeof(BgmFiles) / sizeof(BgmFiles[0]) == static_cast<std::size_t>(BgmId::Count), "BGM IDs and files must match");
static_assert(sizeof(SeFiles) / sizeof(SeFiles[0]) == static_cast<std::size_t>(SeId::Count), "SE IDs and files must match");

std::string FindAudioFile(const std::string& directory, const char* name, bool music) {
    // BGMはOGG、SEはWAVを優先する。同じ名前でMP3/FLACも配置できる。
    const std::array<const char*, 4> musicExtensions = { ".ogg", ".mp3", ".wav", ".flac" };
    const std::array<const char*, 4> soundExtensions = { ".wav", ".ogg", ".mp3", ".flac" };
    for (const char* extension : music ? musicExtensions : soundExtensions) {
        const std::string path = directory + "/" + name + extension;
        if (FileExists(path.c_str())) return path;
    }
    return {};
}

float ValidVolume(float volume, float fallback) {
    return std::isfinite(volume) ? (std::max)(0.0f, (std::min)(1.0f, volume)) : fallback;
}
}

AudioManager& AudioManager::GetInstance() {
    static AudioManager instance;
    return instance;
}

bool AudioManager::Init(const std::string& directory) {
    Shutdown();
    const bool existingDevice = IsAudioDeviceReady();
    if (!existingDevice) InitAudioDevice();
    ready_ = IsAudioDeviceReady();
    ownsDevice_ = ready_ && !existingDevice;
    if (!ready_) {
        TraceLog(LOG_WARNING, "AUDIO: Device unavailable; continuing without sound");
        return false;
    }
    seGains_.fill(1.0f);
    const auto ambiencePath = FindAudioFile(directory + "/BGM", "ambience", true);
    if (!ambiencePath.empty()) {
        ambience_ = LoadMusicStream(ambiencePath.c_str());
        if (IsMusicValid(ambience_)) {
            ambience_.looping = true;
            SetMusicVolume(ambience_, ambienceVolume_);
        }
    }
    int musicCount = 0, soundCount = 0;
    for (std::size_t i = 0; i < bgm_.size(); ++i) {
        const auto path = FindAudioFile(directory + "/BGM", BgmFiles[i], true);
        if (path.empty()) continue;
        bgm_[i] = LoadMusicStream(path.c_str());
        if (IsMusicValid(bgm_[i])) {
            bgm_[i].looping = true;
            SetMusicVolume(bgm_[i], bgmVolume_);
            ++musicCount;
        }
    }
    for (std::size_t i = 0; i < se_.size(); ++i) {
        const auto path = FindAudioFile(directory + "/SE", SeFiles[i], false);
        if (path.empty()) continue;
        se_[i] = LoadSound(path.c_str());
        if (IsSoundValid(se_[i])) { SetSoundVolume(se_[i], seVolume_); ++soundCount; }
    }
    TraceLog(LOG_INFO, "AUDIO: Loaded %i BGM and %i SE from %s (missing files remain silent)", musicCount, soundCount, directory.c_str());
    return true;
}

bool AudioManager::HasBgm(BgmId id) const {
    const auto index = static_cast<std::size_t>(id);
    return IsReady() && index < bgm_.size() && IsMusicValid(bgm_[index]);
}
bool AudioManager::HasSe(SeId id) const {
    const auto index = static_cast<std::size_t>(id);
    return IsReady() && index < se_.size() && IsSoundValid(se_[index]);
}
bool AudioManager::IsBgmPlaying() const {
    return HasBgm(currentBgm_) && IsMusicStreamPlaying(bgm_[static_cast<std::size_t>(currentBgm_)]);
}
bool AudioManager::IsSePlaying(SeId id) const {
    return HasSe(id) && IsSoundPlaying(se_[static_cast<std::size_t>(id)]);
}

bool AudioManager::PlayBgm(BgmId id) {
    if (!IsReady() || static_cast<std::size_t>(id) >= bgm_.size()) return false;
    if (id == currentBgm_ && (IsBgmPlaying() || pausedBgm_)) return true;
    StopBgm();
    if (!HasBgm(id)) return false;
    currentBgm_ = id;
    PlayMusicStream(bgm_[static_cast<std::size_t>(id)]);
    if (paused_) { PauseMusicStream(bgm_[static_cast<std::size_t>(id)]); pausedBgm_ = true; }
    return true;
}
void AudioManager::Update() {
    if (!paused_ && IsReady() && IsMusicValid(ambience_)) UpdateMusicStream(ambience_);
    if (!paused_ && HasBgm(currentBgm_)) UpdateMusicStream(bgm_[static_cast<std::size_t>(currentBgm_)]);
}
void AudioManager::StopBgm() {
    if (HasBgm(currentBgm_)) StopMusicStream(bgm_[static_cast<std::size_t>(currentBgm_)]);
    currentBgm_ = BgmId::Count;
    pausedBgm_ = false;
}
bool AudioManager::PlaySe(SeId id, float gain, float pitch, float pan) {
    if (paused_ || !HasSe(id)) return false;
    const auto index = static_cast<std::size_t>(id);
    seGains_[index] = ValidVolume(gain, 1.0f);
    SetSoundVolume(se_[index], seVolume_ * seGains_[index]);
    SetSoundPitch(se_[index], std::isfinite(pitch) ? std::clamp(pitch, .5f, 2.0f) : 1.0f);
    SetSoundPan(se_[index], ValidVolume(pan, .5f));
    PlaySound(se_[index]);
    return true;
}
void AudioManager::StopSe(SeId id) {
    if (HasSe(id)) StopSound(se_[static_cast<std::size_t>(id)]);
    if (static_cast<std::size_t>(id) < pausedSe_.size()) pausedSe_[static_cast<std::size_t>(id)] = false;
}
void AudioManager::StopAllSe() {
    pausedSe_.fill(false);
    if (!IsReady()) return;
    for (Sound sound : se_) if (IsSoundValid(sound)) StopSound(sound);
}

void AudioManager::SetBgmVolume(float volume) {
    bgmVolume_ = ValidVolume(volume, bgmVolume_);
    if (IsReady()) for (Music music : bgm_) if (IsMusicValid(music)) SetMusicVolume(music, bgmVolume_);
}
void AudioManager::SetSeVolume(float volume) {
    seVolume_ = ValidVolume(volume, seVolume_);
    if (IsReady()) for (std::size_t i = 0; i < se_.size(); ++i)
        if (IsSoundValid(se_[i])) SetSoundVolume(se_[i], seVolume_ * seGains_[i]);
}

void AudioManager::Shutdown() {
    // ミキサーより先にストリームとSEを解放する。重複呼び出しも安全にする。
    StopBgm();
    StopAllSe();
    StopAmbience();
    paused_ = false;
    if (IsAudioDeviceReady()) {
        if (IsMusicValid(ambience_)) UnloadMusicStream(ambience_);
        for (Music music : bgm_) if (IsMusicValid(music)) UnloadMusicStream(music);
        for (Sound sound : se_) if (IsSoundValid(sound)) UnloadSound(sound);
    }
    ambience_ = {};
    bgm_ = {};
    se_ = {};
    if (ownsDevice_ && IsAudioDeviceReady()) CloseAudioDevice();
    ready_ = ownsDevice_ = false;
}

bool AudioManager::PlayAmbience()
{
    if (!IsReady() || !IsMusicValid(ambience_)) return false;
    if (IsAmbiencePlaying() || pausedAmbience_) return true;
    PlayMusicStream(ambience_);
    if (paused_) { PauseMusicStream(ambience_); pausedAmbience_ = true; }
    return true;
}

void AudioManager::StopAmbience()
{
    if (IsReady() && IsMusicValid(ambience_)) StopMusicStream(ambience_);
    pausedAmbience_ = false;
}

bool AudioManager::IsAmbiencePlaying() const
{
    return IsReady() && IsMusicValid(ambience_) && IsMusicStreamPlaying(ambience_);
}

bool AudioManager::IsPaused() const { return paused_; }

void AudioManager::SetAmbienceVolume(float volume)
{
    ambienceVolume_ = ValidVolume(volume, ambienceVolume_);
    if (IsReady() && IsMusicValid(ambience_)) SetMusicVolume(ambience_, ambienceVolume_);
}

void AudioManager::SetPaused(bool paused)
{
    if (paused_ == paused) return;
    paused_ = paused;
    if (!IsReady()) return;
    if (paused) {
        pausedBgm_ = IsBgmPlaying();
        pausedAmbience_ = IsAmbiencePlaying();
        if (pausedBgm_) PauseMusicStream(bgm_[static_cast<std::size_t>(currentBgm_)]);
        if (pausedAmbience_) PauseMusicStream(ambience_);
        for (std::size_t i = 0; i < se_.size(); ++i) {
            pausedSe_[i] = HasSe(static_cast<SeId>(i)) && IsSoundPlaying(se_[i]);
            if (pausedSe_[i]) PauseSound(se_[i]);
        }
    }
    else {
        if (pausedBgm_ && HasBgm(currentBgm_)) ResumeMusicStream(bgm_[static_cast<std::size_t>(currentBgm_)]);
        if (pausedAmbience_ && IsMusicValid(ambience_)) ResumeMusicStream(ambience_);
        for (std::size_t i = 0; i < se_.size(); ++i)
            if (pausedSe_[i] && HasSe(static_cast<SeId>(i))) ResumeSound(se_[i]);
        pausedBgm_ = pausedAmbience_ = false;
        pausedSe_.fill(false);
    }
}
