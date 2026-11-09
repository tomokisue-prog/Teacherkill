#pragma once

class ControlsGuide {
public:
    void Open() { open_ = true; }
    void Close() { open_ = false; }
    bool IsOpen() const { return open_; }
    void Update();
    void Draw() const;

private:
    bool open_ = false;
};
