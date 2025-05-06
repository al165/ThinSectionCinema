#pragma once

#include <deque>
#include <unordered_map>
#include <format>

#include "ofMain.h"
#include "SmoothValue.h"
#include "TileCacheLRU.hpp"
#include "AsyncTextureLoader.hpp"

struct View
{
	ofVec2f offset;
	float width;
	float height;
	float zoomLevel;
	float scale;
	float rotation; // view rotation
	int thetaIndex; // polarisation
	float theta;
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

	ofFbo fboA, fboB, fboFinal;
	ofShader blendShader;
	float blendAlpha;
	ofPlanePrimitive plane;

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

	ofVec2f zoomCenter = {0, 0};
	ofVec2f zoomOffset = {0, 0};
	ofVec2f lastOffset;
	ofVec2f mouseStart;
	ofVec2f tileSize = {520, 384};
	std::vector<int> thetaLevels = {0, 18, 36, 54, 72, 90, 108, 126, 144, 162};
	bool hasPanned = false;

	// cache +- 1.5x view area at zoom level
	// cache +- 1 zoom level of tiles
	// cache +- 2 theta levels for current zoom level
	std::unordered_map<TileKey, ofTexture> cacheMain;
	TileCacheLRU cacheSecondary{400};
	int cacheMisses = 0;

	std::unordered_map<int, std::vector<TileKey>> avaliableTiles;
	int numberVisibleTiles = 0;

	void updateCaches();
	void loadTileList();
	void loadVisibleTiles(const View &view);
	void preloadZoom(int level);
	void drawTiles();
};
