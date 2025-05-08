#pragma once

#include <deque>
#include <unordered_map>
#include <format>

#include "ofMain.h"
#include "SmoothValue.h"
#include "TileCacheLRU.hpp"
#include "AsyncTextureLoader.hpp"

#include "ofxAnimatableFloat.h"

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
	void mouseMoved(int x, int y);
	void mouseDragged(int x, int y, int button);
	void mousePressed(int x, int y, int button);
	void mouseReleased(int x, int y, int button);
	void mouseScrolled(int x, int y, float scrollX, float scrollY);
	void windowResized(int w, int h);

	ofFbo fboA, fboB, fboFinal;
	ofShader blendShader;
	float blendAlpha;
	ofPlanePrimitive plane;

	bool showDebug = true;
	bool drawCached = false;
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
	SmoothValueLinear currentZoom = {2.f, 5.3f, 1.f, 8.f};
	int currentZoomLevel = 5;
	int lastZoomLevel = 5;
	ofRectangle viewWorld;

	ofVec2f zoomCenter = {0.f, 0.f};
	ofVec2f mouseStart;
	// SmoothVec2Linear offsetDelta = {2.f, {0.f, 0.f}};
	ofVec2f offsetDelta = {0.f, 0.f};

	ofxAnimatableFloat viewTargetAnim;
	ofxAnimatableFloat zoomAnim;
	ofVec2f viewTarget = {0.f, 0.f};
	ofVec2f viewStart = {0.f, 0.f};
	bool focusViewTarget = false;

	std::vector<int> thetaLevels = {0, 18, 36, 54, 72, 90, 108, 126, 144, 162};

	// cache +- 1.5x view area at zoom level
	// cache +- 1 zoom level of tiles
	// cache +- 2 theta levels for current zoom level
	std::unordered_map<TileKey, ofTexture> cacheMain;
	TileCacheLRU cacheSecondary{600};
	int cacheMisses = 0;

	std::unordered_map<int, std::vector<TileKey>> avaliableTiles;
	std::unordered_map<int, ofVec2f> zoomWorldSizes;
	int numberVisibleTiles = 0;

	void updateCaches();
	void loadTileList();
	void loadVisibleTiles(const View &view);
	void preloadZoom(int level);
	void drawTiles();
	void setViewTarget(ofVec2f worldCoords, float delayS = 0.f);
	void animationFinished(ofxAnimatableFloat::AnimationEvent &ev);

	ofVec2f screenToWorld(ofVec2f coords) const;
	ofVec2f worldToScreen(ofVec2f coords) const;

	ofVec2f globalToWorld(ofVec2f coords) const;
	ofVec2f worldToGlobal(ofVec2f coords) const;
};
