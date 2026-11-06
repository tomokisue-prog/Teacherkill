#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include "EventBase.h"

class EventManager
{
public:
    static EventManager& GetInstance()
    {
        static EventManager instance;
        return instance;
    }

    // 毎フレームの更新と描画
    void Update(float deltaTime);
    void Draw() const;

    // イベントの登録
    void RegisterEvent(const std::string& name, std::shared_ptr<EventBase> ev);

    // 指定したイベントを開始する
    void StartEvent(const std::string& name);

    // 現在のイベントを強制終了する
    void ClearCurrentEvent();

    // 何らかのイベントが実行中か
    bool IsEventActive() const { return currentEvent_ != nullptr; }

private:
    EventManager() = default;
    ~EventManager() = default;

    EventManager(const EventManager&) = delete;
    EventManager& operator=(const EventManager&) = delete;

    std::shared_ptr<EventBase> currentEvent_{ nullptr };
    std::unordered_map<std::string, std::shared_ptr<EventBase>> eventPool_;
};

// ショートカット関数
inline EventManager& EventMgr() { return EventManager::GetInstance(); }