#pragma once

#include "raylib.h"
#include <cstddef>
#include <cstdint>
#include <vector>

// イントロはJPEGの連番画像を再生し、音声は別に流す
class IntroVideoPlayer
{
public:
    IntroVideoPlayer() = default;
    ~IntroVideoPlayer() = default;

    bool Start(const char* framesPath, const char* audioPath, float volume);
    bool Update();
    void Draw() const;
    void Stop();

    bool IsPlaying() const { return playing_; }
    float GetFadeOutAlpha() const { return fadeOutAlpha_; }

private:
    static constexpr double FadeOutSeconds = 0.65;
    struct Frame { std::size_t offset; int length; };

    std::uint32_t ReadU32(std::size_t pos) const;
    bool LoadFrame(std::size_t index);

    unsigned char* bytes_ = nullptr;
    std::size_t byteCount_ = 0;
    std::vector<Frame> frames_;
    Texture2D texture_{};
    Music audio_{};
    bool audioLoaded_ = false;
    bool playing_ = false;
    bool fadingOut_ = false;
    float volume_ = 0.0f;
    float fadeOutAlpha_ = 0.0f;
    std::uint32_t fps_ = 0;
    double duration_ = 0.0;
    double startedAt_ = 0.0;
    double fadeStartedAt_ = 0.0;
    std::size_t currentFrame_ = 0;
};