#pragma once

#include <list>
#include <string>
#include <unordered_map>

#include "ofMain.h"
#include "TilesetProperties.h"

namespace std
{
    template <>
    struct hash<TileKey>
    {
        size_t operator()(const TileKey &k) const
        {
            return k.hash;
        }
    };
}

class TileCacheLRU
{
public:
    TileCacheLRU(size_t maxSize) : maxSize(maxSize) {}

    using CacheMap = std::unordered_map<TileKey, std::pair<ofTexture, std::list<TileKey>::iterator>>;
    using const_iterator = CacheMap::const_iterator;
    using iterator = CacheMap::iterator;

    bool contains(const TileKey &key, bool touch_on_find = false)
    {
        iterator it = cache.find(key);
        if (touch_on_find)
            touch(it);
        return it != cache.end();
    }

    bool get(const TileKey &key, ofTexture &outImg)
    {
        auto it = cache.find(key);
        if (it == cache.end())
            return false;
        // Move to front
        usage.splice(usage.begin(), usage, it->second.second);
        outImg = it->second.first;
        return true;
    }

    void touch(iterator it)
    {
        if (it == cache.end())
            return;
        usage.splice(usage.begin(), usage, it->second.second);
    }

    void touch(const TileKey &key)
    {
        auto it = cache.find(key);
        touch(it);
    }

    void put(const TileKey &key, const ofTexture &img)
    {
        auto it = cache.find(key);
        if (it != cache.end())
        {
            usage.splice(usage.begin(), usage, it->second.second);
            it->second.first = img;
            return;
        }

        if (cache.size() >= maxSize)
        {
            TileKey oldKey = usage.back();
            auto it = cache.find(oldKey);
            if (it != cache.end())
            {
                usage.erase(it->second.second); // erase from list using stored iterator
                cache.erase(it);                // then erase from cache
            }
        }
        usage.push_front(key);
        cache[key] = {img, usage.begin()};
    }

    void erase(const TileKey &key)
    {
        auto it = cache.find(key);
        if (it != cache.end())
        {
            usage.erase(it->second.second);
            cache.erase(it);
        }
    }

    size_t size()
    {
        return cache.size();
    }

    iterator begin() { return cache.begin(); }
    iterator end() { return cache.end(); }
    const_iterator begin() const { return cache.begin(); }
    const_iterator end() const { return cache.end(); }

private:
    size_t maxSize;
    std::list<TileKey> usage;
    std::unordered_map<TileKey, std::pair<ofTexture, std::list<TileKey>::iterator>> cache;
};
