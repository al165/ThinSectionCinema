#include "ofMain.h"
#include <unordered_set>

class AsyncTextureLoader : public ofThread
{
public:
    using LoadCallback = std::function<void(const std::string &, ofImage)>;

    AsyncTextureLoader()
    {
        startThread();
    }

    ~AsyncTextureLoader()
    {
        stop();
    }

    void requestLoad(const std::string &path, LoadCallback callback)
    {
        if (pendingSet.count(path))
            return;

        pendingSet.insert(path);
        loadRequests.send({path, callback});
    }

    void stop()
    {
        loadRequests.close();
        waitForThread();
    }

    void dispatchMainCallbacks(int maxCount)
    {
        int n = 0;
        LoadResult result;
        while (n < maxCount && loadResults.tryReceive(result))
        {
            auto &[path, pixels, callback] = result;

            // Allocate and upload to GPU on main thread
            ofImage tile;
            tile.setFromPixels(pixels);

            callback(path, tile);
            n++;
        }
    }

protected:
    void threadedFunction() override
    {
        LoadRequest request;
        while (loadRequests.receive(request))
        {
            auto &[path, callback] = request;

            ofPixels loadedPixels;
            if (!ofLoadImage(loadedPixels, path))
            {
                ofLogError() << "AsyncTextureLoader failed to load: " << path;
                pendingSet.erase(path);
                continue;
            }

            loadResults.send({path, loadedPixels, callback});
            pendingSet.erase(path);
        }

        loadResults.close();
    }

private:
    struct LoadRequest
    {
        std::string path;
        LoadCallback callback;
    };

    struct LoadResult
    {
        std::string path;
        ofPixels pixels;
        LoadCallback callback;
    };

    ofThreadChannel<LoadRequest> loadRequests;
    ofThreadChannel<LoadResult> loadResults;

    std::unordered_set<std::string> pendingSet;
};
