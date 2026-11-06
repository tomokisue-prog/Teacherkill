#pragma once

#include "raylib.h"
#include <cstddef>
#include <cstdint>
#include <vector>

// The supplied movie is packaged as JPEG frames and an OGG soundtrack.
// This lets raylib play it without installing an extra video library.
class IntroVideoPlayer
{
public:
    bool Start(const char* framesPath, const char* audioPath, float volume);

    // Returns false once the clip finishes or a frame cannot be decoded.
    bool Update();

    void Draw() const;

    bool IsPlaying() const { return playing_; }

    void Stop();

private:
    static constexpr double FadeOutSeconds = 0.65; // Adjust the video's fade-out duration here.
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
