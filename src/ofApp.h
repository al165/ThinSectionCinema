#pragma once

#include <fstream>
#include <deque>
#include <unordered_map>
#include <format>

#include "ofMain.h"
#include "SmoothValue.h"
#include "TileCacheLRU.hpp"
#include "AsyncTextureLoader.hpp"
#include "TilesetProperties.h"
#include "TilesetManager.hpp"

#include "ofxCsv.h"
#include "ofxJSON.h"
#include "ofxFFmpegRecorder.h"
#include "ofxAnimatableFloat.h"

#include <toml.hpp>

#include "ofxImGui.h"

#include <filesystem>
namespace fs = std::filesystem;

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
    bool waitForFrames = false;
    int numFramesQueued;
    int frameCount = 0;
    ofPixels framePixels;
    ofImage frame;
    float recordingFps = 30.f;

    bool recordPath = false;
    float recordPathDt = 1.f;
    float lastPathT = -recordPathDt * 2.f;

    bool showDebug = true;
    bool drawCached = false;
    float lastFrameTime;

    AsyncTextureLoader loader;
    View currentView;

    ofMatrix4x4 viewMatrix;
    ofMatrix4x4 viewMatrixInverted;
    ofRectangle screenRectangle;
    ofVec2f screenCenter;

    TileSet *currentTileSet;

    float zoomSpeed = 4.f;
    const float maxZoom = 1.f;
    const float minZoom = 8.f;
    const int maxZoomLevel = 1;
    const int minZoomLevel = 5;
    SmoothValueLinear currentZoomSmooth = {4.f, 5.3f, 0.f, 8.f};
    ofxAnimatableFloat drillZoomAnim;
    int currentZoomLevel = 5;
    Zoom currentZoom;
    int lastZoomLevel = 5;
    SmoothValueLinear currentTheta = {2.f, 0.f, -360.f, 720.f};
    bool cycleTheta = true;
    float thetaSpeed = 0.1f;

    bool centerZoom = true;
    ofVec2f zoomCenterWorld = {0.f, 0.f};
    ofVec2f rotationCenterWorld = {0.f, 0.f};
    ofVec2f lastMouse;
    ofVec2f offsetDelta = {0.f, 0.f};

    float minMovingTime, maxMovingTime;
    ofxAnimatableFloat viewTargetAnim;
    ofVec2f viewTargetWorld = {0.f, 0.f};
    ofVec2f viewStartWorld = {0.f, 0.f};
    bool focusViewTarget = false;
    float time;
    bool drill = false;
    float drillTime = 8.f;
    bool targetOrientation = false;
    float drillDepth = 0.f;
    float flyHeight = 4.f;
    float drillSpeed = 0.4f;
    float spinSpeed = 0.06f;
    ofxAnimatableFloat spinSmooth;

    SmoothValueLinear rotationAngle = {2.f, 0.f, -360.f, 720.f};

    TilesetManager tilesetManager;

    std::unordered_map<TileKey, ofTexture> cacheMain;
    TileCacheLRU cacheSecondary{600};
    int cacheMisses = 0;

    int numberVisibleTiles = 0;

    std::vector<shared_ptr<SequenceEvent>> sequence;
    std::vector<shared_ptr<POI>> sequencePoi;
    int sequenceStep;
    bool sequencePlaying = false;

    bool isVisible(const ofRectangle &rect, ofVec2f offset = {0.f, 0.f});
    bool isVisible(const TileKey &key, ofVec2f offset = {0.f, 0.f});
    bool updateCaches();
    // void addTileSet(const std::string &set, const std::string &position, const std::string &alignment, const std::string &relativeTo);
    // void loadTileList(const std::string &set);
    void preloadZoom(int level);
    void drawTiles(const TileSet &tileset);
    void setViewTarget(ofVec2f worldCoords, float delayS = 0.f);
    void playSequence(int step = 0);
    void nextStep();
    void animationFinished(ofxAnimatableFloat::AnimationEvent &ev);
    void valueReached(SmoothValueLinear::SmoothValueEvent &ev);

    ofVec2f screenToWorld(const ofVec2f &coords);
    ofVec2f worldToScreen(const ofVec2f &coords);

    ofVec2f globalToWorld(const ofVec2f &coords, const TileSet *tileset) const;
    ofVec2f worldToGlobal(const ofVec2f &coords, const TileSet *tileset) const;

    void jumpTo(const ofVec2f &worldCoords);
    void jumpTo(const POI &poi);
    void jumpZoom(float zoomLevel);

    bool saveSequence(const std::string &name);
    bool loadSequence(const std::string &name);

    void calculateViewMatrix();

    // GUI stuff
    bool hideGui = false;
    ofxImGui::Gui gui;
    void drawGUI();
    bool disableMouse = false;
    bool disableKeyboard = false;
};
