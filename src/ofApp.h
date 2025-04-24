#pragma once

#include <unordered_map>
#include <unordered_set>
#include <list>
#include <deque>

#include "ofMain.h"
#include "SmoothValue.h"
#include "AsyncTextureLoader.hpp"

struct TileKey
{
	int zoom;
	int x;
	int y;
	int width;
	int height;
	int theta;
	std::string filepath;

	TileKey(int z, int xx, int yy, int w, int h, int t, std::string path) : zoom(z),
																			x(xx), y(yy),
																			width(w), height(h),
																			theta(t),
																			filepath(std::move(path)) {}

	bool operator==(const TileKey &other) const
	{
		return zoom == other.zoom && x == other.x && y == other.y && width == other.width && height == other.height;
	}
};

struct View
{
	float x;
	float y;
	float width;
	float height;
	float zoomLevel;
	float scale;
	float rotation; // view rotation
	float theta;	// polarisation
};

template <class T>
inline void hash_combine(std::size_t &seed, const T &v)
{
	std::hash<T> hasher;
	seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

namespace std
{
	template <>
	struct hash<TileKey>
	{
		size_t operator()(const TileKey &k) const
		{
			size_t h1 = std::hash<int>()(k.zoom);
			size_t h2 = std::hash<int>()(k.x);
			size_t h3 = std::hash<int>()(k.y);
			size_t h4 = std::hash<int>()(k.theta);
			size_t hash = h1;
			hash_combine(hash, h2);
			hash_combine(hash, h3);
			hash_combine(hash, h4);
			return hash;
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

	bool contains(const TileKey &key)
	{
		return cache.find(key) != cache.end();
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
		usage.splice(usage.begin(), usage, it->second.second);
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
			usage.pop_back();
			cache.erase(oldKey);
		}
		usage.push_front(key);
		cache[key] = {img, usage.begin()};
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

class ofApp : public ofBaseApp
{

public:
	void setup();
	void update();
	void draw();
	void exit();

	void keyPressed(int key);
	void keyReleased(int key);
	void mouseMoved(int x, int y);
	void mouseDragged(int x, int y, int button);
	void mousePressed(int x, int y, int button);
	void mouseReleased(int x, int y, int button);
	void mouseScrolled(int x, int y, float scrollX, float scrollY);
	void windowResized(int w, int h);
	void dragEvent(ofDragInfo dragInfo);

	bool showDebug = false;
	ofFpsCounter fpsCounter = ofFpsCounter();
	std::deque<float> fpsHistory;
	size_t historyLength = 128;
	float lastFrameTime;

	ofDirectory tiles;
	AsyncTextureLoader loader;
	View currentView;

	const float maxZoom = 1.f;
	const float minZoom = 8.f;
	const int maxZoomLevel = 1;
	const int minZoomLevel = 5;
	SmoothValueLinear currentZoom = {2.f, 5.f, 1.f, 8.f};
	int currentZoomLevel = 5;
	int lastZoomLevel = 5;
	float scale = 1.f;

	ofVec2f offset = {0, 0};
	ofVec2f lastOffset;
	ofVec2f mouseStart;
	ofVec2f tileSize = {520, 384};
	float theta;
	std::vector<int> thetaLevels = {0, 18, 36, 54, 72, 90, 108, 126, 144, 162};
	size_t thetaIndex = 0;

	TileCacheLRU tileCache{300};
	// std::unordered_set<TileKey> tileKeys;

	std::unordered_map<int, std::vector<TileKey>> avaliableTiles;
	int numberVisibleTiles = 0;

	void loadTileList();
	void loadVisibleTiles(const View &view);
	void preloadZoomIn();
	void preloadZoomOut();
	void drawTiles();
};
