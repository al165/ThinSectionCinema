#pragma once

#include <fstream>
#include <deque>
#include <unordered_map>
#include <format>

#include "ofMain.h"
#include "SmoothValue.h"
#include "TileCacheLRU.hpp"
#include "AsyncTextureLoader.hpp"

#include "ofxCsv.h"
#include "ofxFFmpegRecorder.h"
#include "ofxAnimatableFloat.h"

#include <toml.hpp>

using Theta = int;
using Zoom = int;

struct View
{
    ofVec2f offsetWorld;
    float width;
    float height;
    float zoomLevel;
    float scale;
    float rotation; // view rotation
    float theta;
    ofRectangle viewWorld;
    // std::string tileset;
};

struct TileSet
{
    std::string name;
    ofFbo fboA, fboB, fboMain;
    ofVec2f offset;
    Theta t1, t2;
    float blendAlpha = 0.f;
    std::vector<Theta> thetaLevels;
    std::vector<ofVec2f> viewTargets;
    std::unordered_map<Zoom, std::unordered_map<Theta, std::vector<TileKey>>> avaliableTiles;
    std::unordered_map<Zoom, ofVec2f> zoomWorldSizes;
    TileSet()
    {
        int fboW = ofGetWidth();
        int fboH = ofGetHeight();
        fboA.allocate(fboW, fboH, GL_RGBA);
        fboB.allocate(fboW, fboH, GL_RGBA);
        fboMain.allocate(fboW, fboH, GL_RGBA);

        t1 = 0;
        t2 = 1;
    }
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

    std::string scanRoot;
    ofxCsv csv;

    ofFbo fboFinal;
    ofShader blendShader;
    ofPlanePrimitive plane;

    ofxFFmpegRecorder ffmpegRecorder;
    std::optional<std::string> recordingFolder;
    std::optional<std::string> recordingFileName;
    bool frameReady = false;
    bool recording = false;
    int frameCount = 0;
    ofPixels framePixels;
    ofImage frame;
    float recordingFps = 30.f;

    bool recordPath = false;
    float recordPathDt = 1.f;
    float lastPathT = -recordPathDt * 2.f;

    bool showDebug = true;
    bool drawCached = false;
    ofFpsCounter fpsCounter = ofFpsCounter();
    float lastFrameTime;

    ofDirectory tiles;
    AsyncTextureLoader loader;
    View currentView;

    ofMatrix4x4 viewMatrix;
    ofMatrix4x4 viewMatrixInverted;
    ofRectangle screenRectangle;
    ofVec2f screenCenter;

    TileSet *currentTileSet;

    const float maxZoom = 1.f;
    const float minZoom = 8.f;
    const int maxZoomLevel = 1;
    const int minZoomLevel = 5;
    SmoothValueLinear currentZoom = {2.f, 5.3f, 1.f, 8.f};
    int currentZoomLevel = 5;
    int lastZoomLevel = 5;
    SmoothValueLinear currentTheta = {2.f, 0.f, -360.f, 720.f};
    bool cycleTheta = true;

    ofVec2f zoomCenterWorld = {0.f, 0.f};
    ofVec2f rotationCenterWorld = {0.f, 0.f};
    ofVec2f lastMouse;
    ofVec2f offsetDelta = {0.f, 0.f};

    float minMovingTime, maxMovingTime;
    ofxAnimatableFloat viewTargetAnim;
    ofxAnimatableFloat zoomAnim;
    ofVec2f viewTargetWorld = {0.f, 0.f};
    ofVec2f viewStartWorld = {0.f, 0.f};
    bool focusViewTarget = false;
    float time;

    SmoothValueLinear rotationAngle = {2.f, 0.f, -360.f, 720.f};

    std::unordered_map<std::string, TileSet> tilesets;
    std::vector<TileSet *> tilesetList;
    size_t tileset_index = 0;

    std::unordered_map<TileKey, ofTexture> cacheMain;
    TileCacheLRU cacheSecondary{600};
    int cacheMisses = 0;

    int numberVisibleTiles = 0;

    bool isVisible(const ofRectangle &rect, ofVec2f offset = {0.f, 0.f});
    bool isVisible(const TileKey &key, ofVec2f offset = {0.f, 0.f});
    bool updateCaches();
    void loadTileList(const std::string &set);
    void preloadZoom(int level);
    void drawTiles(const TileSet &tileset);
    void setViewTarget(ofVec2f worldCoords, float delayS = 0.f);
    void animationFinished(ofxAnimatableFloat::AnimationEvent &ev);

    ofVec2f screenToWorld(const ofVec2f &coords);
    ofVec2f worldToScreen(const ofVec2f &coords);

    ofVec2f globalToWorld(const ofVec2f &coords, const TileSet *tileset) const;
    ofVec2f worldToGlobal(const ofVec2f &coords, const TileSet *tileset) const;

    void calculateViewMatrix();
};
