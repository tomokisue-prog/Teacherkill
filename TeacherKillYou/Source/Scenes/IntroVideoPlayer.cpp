#include "IntroVideoPlayer.h"
#include <algorithm>
#include <cmath>

bool IntroVideoPlayer::Start(const char* framesPath, const char* audioPath, float volume)
{
    Stop();
    if (!FileExists(framesPath))
    {
        TraceLog(LOG_WARNING, "INTRO: Frame file not found: %s", framesPath);
        return false;
    }

    int fileSize = 0;
    bytes_ = LoadFileData(framesPath, &fileSize);
    if (!bytes_ || fileSize < 16)
    {
        TraceLog(LOG_WARNING, "INTRO: Cannot read frame file: %s", framesPath);
        Stop();
        return false;
    }
    byteCount_ = static_cast<std::size_t>(fileSize);
    if (bytes_[0] != 'R' || bytes_[1] != 'I' || bytes_[2] != 'N' || bytes_[3] != '1')
    {
        TraceLog(LOG_WARNING, "INTRO: Invalid frame file header");
        Stop();
        return false;
    }

    const std::uint32_t frameCount = ReadU32(4);
    fps_ = ReadU32(8);
    const std::uint32_t durationMs = ReadU32(12);
    if (frameCount == 0 || frameCount > 10000 || fps_ == 0 || fps_ > 120 ||
        durationMs == 0 || durationMs > 3600000)
    {
        TraceLog(LOG_WARNING, "INTRO: Invalid frame count or duration");
        Stop();
        return false;
    }
    duration_ = durationMs / 1000.0;
    frames_.reserve(frameCount);
    std::size_t pos = 16;
    for (std::uint32_t i = 0; i < frameCount; ++i)
    {
        if (pos > byteCount_ || byteCount_ - pos < 4)
        {
            TraceLog(LOG_WARNING, "INTRO: Frame file is incomplete");
            Stop();
            return false;
        }
        const std::uint32_t length = ReadU32(pos);
        pos += 4;
        if (length == 0 || length > byteCount_ - pos || length > 0x7fffffffU)
        {
            TraceLog(LOG_WARNING, "INTRO: Invalid frame data");
            Stop();
            return false;
        }
        frames_.push_back({ pos, static_cast<int>(length) });
        pos += length;
    }
    if (!LoadFrame(0))
    {
        TraceLog(LOG_WARNING, "INTRO: Could not decode the first frame");
        Stop();
        return false;
    }

    volume_ = (std::max)(0.0f, (std::min)(1.0f, volume));
    if (IsAudioDeviceReady() && FileExists(audioPath))
    {
        audio_ = LoadMusicStream(audioPath);
        if (IsMusicValid(audio_))
        {
            audioLoaded_ = true;
            audio_.looping = false;
            SetMusicVolume(audio_, volume_);
            PlayMusicStream(audio_);
            UpdateMusicStream(audio_);
        }
        else TraceLog(LOG_WARNING, "INTRO: Could not load soundtrack: %s", audioPath);
    }
    else TraceLog(LOG_WARNING, "INTRO: Soundtrack unavailable; playing without audio");

    startedAt_ = GetTime();
    playing_ = true;
    return true;
}

bool IntroVideoPlayer::Update()
{
    if (!playing_) return false;
    if (audioLoaded_ && IsAudioDeviceReady()) UpdateMusicStream(audio_);

    const double now = GetTime();
    const double elapsed = (std::max)(0.0, now - startedAt_);
    if (!fadingOut_ && elapsed >= duration_)
    {
        // Show the last frame even if a slow frame skipped over it.
        const std::size_t lastFrame = frames_.size() - 1;
        if (currentFrame_ != lastFrame && !LoadFrame(lastFrame))
        {
            TraceLog(LOG_WARNING, "INTRO: Could not decode the last frame");
            Stop();
            return false;
        }
        fadingOut_ = true;
        fadeStartedAt_ = now;
    }
    if (fadingOut_)
    {
        const float progress = static_cast<float>((std::max)(0.0, (std::min)(1.0,
            (now - fadeStartedAt_) / FadeOutSeconds)));
        fadeOutAlpha_ = progress * progress * (3.0f - 2.0f * progress);
        if (audioLoaded_ && IsAudioDeviceReady()) SetMusicVolume(audio_, volume_ * (1.0f - fadeOutAlpha_));
        if (progress >= 1.0f)
        {
            Stop();
            return false;
        }
        return true;
    }
    const std::size_t frame = (std::min)(
        static_cast<std::size_t>(elapsed * fps_), frames_.size() - 1);
    if (frame != currentFrame_ && !LoadFrame(frame))
    {
        TraceLog(LOG_WARNING, "INTRO: Could not decode a frame; continuing to the game");
        Stop();
        return false;
    }
    return true;
}

void IntroVideoPlayer::Draw() const
{
    ClearBackground(BLACK);
    if (!playing_ || texture_.id == 0) return;
    const float scale = (std::min)(
        static_cast<float>(GetScreenWidth()) / texture_.width,
        static_cast<float>(GetScreenHeight()) / texture_.height);
    const float width = texture_.width * scale;
    const float height = texture_.height * scale;
    const Rectangle source{ 0, 0, static_cast<float>(texture_.width), static_cast<float>(texture_.height) };
    const Rectangle destination{ (GetScreenWidth() - width) * 0.5f,
                                 (GetScreenHeight() - height) * 0.5f, width, height };
    DrawTexturePro(texture_, source, destination, { 0, 0 }, 0.0f, WHITE);
    if (fadeOutAlpha_ > 0.0f)
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                      Color{0, 0, 0, static_cast<unsigned char>(fadeOutAlpha_ * 255.0f + 0.5f)});
}

void IntroVideoPlayer::Stop()
{
    if (audioLoaded_ && IsAudioDeviceReady())
    {
        StopMusicStream(audio_);
        UnloadMusicStream(audio_);
    }
    audio_ = {};
    audioLoaded_ = false;
    if (texture_.id != 0) UnloadTexture(texture_);
    texture_ = {};
    if (bytes_) UnloadFileData(bytes_);
    bytes_ = nullptr;
    byteCount_ = 0;
    frames_.clear();
    fps_ = 0;
    duration_ = 0.0;
    startedAt_ = 0.0;
    fadeStartedAt_ = 0.0;
    fadeOutAlpha_ = 0.0f;
    volume_ = 0.0f;
    currentFrame_ = 0;
    fadingOut_ = false;
    playing_ = false;
}

std::uint32_t IntroVideoPlayer::ReadU32(std::size_t pos) const
{
    return static_cast<std::uint32_t>(bytes_[pos]) |
           (static_cast<std::uint32_t>(bytes_[pos + 1]) << 8) |
           (static_cast<std::uint32_t>(bytes_[pos + 2]) << 16) |
           (static_cast<std::uint32_t>(bytes_[pos + 3]) << 24);
}

bool IntroVideoPlayer::LoadFrame(std::size_t index)
{
    const Frame& frame = frames_[index];
    Image image = LoadImageFromMemory(".jpg", bytes_ + frame.offset, frame.length);
    if (!image.data || image.width <= 0 || image.height <= 0)
    {
        if (image.data) UnloadImage(image);
        return false;
    }
    ImageFormat(&image, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    if (image.format != PIXELFORMAT_UNCOMPRESSED_R8G8B8A8)
    {
        UnloadImage(image);
        return false;
    }
    if (texture_.id == 0)
    {
        texture_ = LoadTextureFromImage(image);
        if (texture_.id != 0) SetTextureFilter(texture_, TEXTURE_FILTER_BILINEAR);
    }
    else if (texture_.width == image.width && texture_.height == image.height)
        UpdateTexture(texture_, image.data);
    else
    {
        UnloadImage(image);
        return false;
    }
    UnloadImage(image);
    if (texture_.id == 0) return false;
    currentFrame_ = index;
    return true;
}
