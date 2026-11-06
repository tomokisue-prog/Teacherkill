#include "EventManager.h"

void EventManager::Update(float deltaTime)
{
    if (currentEvent_ != nullptr)
    {
        currentEvent_->Update(deltaTime);

        // イベントが終了フラグを立てたら自動で終了処理
        if (currentEvent_->IsFinished())
        {
            currentEvent_ = nullptr;
        }
    }
}

void EventManager::Draw() const
{
    if (currentEvent_ != nullptr)
    {
        currentEvent_->Draw();
    }
}

void EventManager::RegisterEvent(const std::string& name, std::shared_ptr<EventBase> ev)
{
    eventPool_[name] = ev;
}

void EventManager::StartEvent(const std::string& name)
{
    auto it = eventPool_.find(name);
    if (it != eventPool_.end())
    {
        currentEvent_ = it->second;
        currentEvent_->Init();
    }
}

void EventManager::ClearCurrentEvent()
{
    currentEvent_ = nullptr;
}