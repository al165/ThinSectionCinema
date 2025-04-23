#pragma once

#include <unordered_map>
#include <unordered_set>
#include <list>

#include "ofMain.h"
#include "SmoothValue.h"

struct TileKey
{
	int zoom;
	int x;
	int y;
	int width;
	int height;
	std::string filepath;

	TileKey(int z, int xx, int yy, int w, int h, std::string path) : zoom(z), x(xx), y(yy), width(w), height(h), filepath(std::move(path)) {}

	bool operator==(const TileKey &other) const
	{
		return zoom == other.zoom && x == other.x && y == other.y && width == other.width && height == other.height;
	}
};

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
			size_t h4 = std::hash<int>()(k.width);
			size_t h5 = std::hash<int>()(k.height);
			return (((((h1 ^ (h2 << 1)) >> 1) ^ (h3 << 1)) >> 1) ^ (h4 << 1)) ^ (h5 << 1);
		}
	};
}

class TileCacheLRU
{
public:
	TileCacheLRU(size_t maxSize) : maxSize(maxSize) {}

	bool contains(const TileKey &key)
	{
		return cache.find(key) != cache.end();
	}

	bool get(const TileKey &key, ofImage &outImg)
	{
		auto it = cache.find(key);
		if (it == cache.end())
			return false;
		// Move to front
		usage.splice(usage.begin(), usage, it->second.second);
		outImg = it->second.first;
		return true;
	}

	void put(const TileKey &key, const ofImage &img)
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

private:
	size_t maxSize;
	std::list<TileKey> usage;
	std::unordered_map<TileKey, std::pair<ofImage, std::list<TileKey>::iterator>> cache;
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

	ofFpsCounter fpsCounter = ofFpsCounter();

	ofDirectory tiles;

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

	TileCacheLRU tileCache{200};
	std::unordered_set<TileKey> tileKeys;

	std::unordered_map<int, std::vector<TileKey>> avaliableTiles;
	int numberVisibleTiles = 0;

	void loadTileList();
	void loadVisibleTiles();
	void drawTiles();
};
