#pragma once

#include "ofMain.h"
#include <queue>
#include <unordered_set>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <functional>

class AsyncTextureLoader : public ofThread
{
public:
    using LoadCallback = std::function<void(const std::string &, ofImage)>;

    void requestLoad(const std::string &path, LoadCallback callback)
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (pendingSet.count(path))
            return; // deduplicate

        loadQueue.push({path, callback});
        pendingSet.insert(path);
        condition.notify_one();
    }

    void start()
    {
        startThread();
    }

    void stop()
    {
        stopThread();
        condition.notify_all();
    }

    void dispatchMainCallbacks()
    {
        std::lock_guard<std::mutex> lock(callbackMutex);
        while (!mainThreadCallbacks.empty())
        {
            mainThreadCallbacks.front()();
            mainThreadCallbacks.pop();
        }
    }

protected:
    void threadedFunction() override
    {
        while (isThreadRunning())
        {
            std::unique_lock<std::mutex> lock(mutex);
            condition.wait(lock, [&]()
                           { return !loadQueue.empty() || !isThreadRunning(); });

            if (!isThreadRunning())
                break;

            auto [path, callback] = loadQueue.front();
            loadQueue.pop();
            pendingSet.erase(path);
            lock.unlock();

            // ofPixels pixels;
            ofImage tile;
            if (!tile.load(path))
            {
                ofLogError() << "AsyncTextureLoader failed to load: " << path;
                continue;
            }

            std::lock_guard<std::mutex> cbLock(callbackMutex);
            mainThreadCallbacks.push([=]()
                                     {
                                        // ofTexture tex = tile.getTexture();
                                        callback(path, tile); });
        }
    }

private:
    struct LoadRequest
    {
        std::string path;
        LoadCallback callback;
    };

    std::queue<LoadRequest> loadQueue;
    std::unordered_set<std::string> pendingSet;
    std::mutex mutex;
    std::condition_variable condition;

    std::queue<std::function<void()>> mainThreadCallbacks;
    std::mutex callbackMutex;
};