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
#include "Sequencer.hpp"

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

class ofApp : public ofBaseApp, public Visitor
{

public:
    void setup();
    void update();
    void draw();
    void exit();

    void keyPressed(ofKeyEventArgs &key);
    void mouseMoved(int x, int y);
    void mouseDragged(int x, int y, int button);
    void mousePressed(int x, int y, int button);
    void mouseReleased(int x, int y, int button);
    void mouseScrolled(int x, int y, float scrollX, float scrollY);
    void windowResized(int w, int h);

    fs::path scanRoot;
    fs::path projectsDir;
    std::vector<std::string> projectsList;
    fs::path projectDir, layoutPath, sequencePath;
    std::string projectName;
    ofxCsv csv;

    ofFbo fboFinal;
    ofShader blendShader;
    ofPlanePrimitive plane;

    ofxFFmpegRecorder ffmpegRecorder;
    // std::optional<std::string> recordingFolder;
    // std::optional<std::string> recordingFileName;
    std::string recordingFileName;
    fs::path recordingDir;

    bool frameReady = false;
    bool recording = false;
    bool waitForFrames = false;
    int numFramesQueued;
    int frameCount = 0;
    ofPixels framePixels;
    ofImage frame;
    float recordingFps = 30.f;

    bool recordPath = false;
    float recordPathDt = 0.1;
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

    std::shared_ptr<TileSet> currentTileSet;
    int currentPOI = -1;

    float zoomAdjust = 0.f;
    float zoomSpeed = 4.f;
    const float maxZoom = 1.f;
    const float minZoom = 8.f;
    const int maxZoomLevel = 1;
    const int minZoomLevel = 5;
    bool manualZooming = true;
    SmoothValueLinear currentZoomSmooth = {4.f, 5.3f, -10.f, 8.f};
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
    ofVec2f cameraTargetWorld;
    float k = 3.f;
    float d = 0.8;
    ofVec2f vel;
    ofVec2f viewTargetWorld = {0.f, 0.f};
    ofVec2f viewStartWorld = {0.f, 0.f};
    bool focusViewTarget = false;
    float recordStartTime = 0.f;
    float time;
    float waitEndTime, waitTheta;
    bool waiting = false;
    bool doneWaiting = false;
    bool waitingForTheta = false;
    bool drill = false;
    float drillTime = 8.f;
    bool targetOrientation = false;
    float orientationStartAngle = 0.f;
    float orientationEndAngle = 0.f;
    float drillDepth = 0.5f;
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

    void createProject(const std::string &name);
    void loadProject(const std::string &name);
    bool isVisible(const ofRectangle &rect, ofVec2f offset = {0.f, 0.f});
    bool isVisible(const TileKey &key, ofVec2f offset = {0.f, 0.f});
    ofRectangle getLayoutBounds();
    bool updateCaches();
    void preloadZoom(int level);
    void drawTiles(std::shared_ptr<TileSet> tileset);
    void setViewTarget(ofVec2f worldCoords, float delayS = 0.f);
    void startRecording();
    void stopRecording();
    void playSequence(int step = 0);
    void nextStep();
    void animationFinished(ofxAnimatableFloat::AnimationEvent &ev);
    void valueReached(SmoothValueLinear::SmoothValueEvent &ev);
    void dumpState(const std::string &path);
    void loadState(const std::string &path);
    void visit(POI &ev) override;
    void visit(ParameterChange &ev) override;
    void visit(WaitSeconds &ev) override;
    void visit(WaitTheta &ev) override;
    void visit(Drill &ev) override;
    void visit(Overview &ev) override;
    void visit(Jump &ev) override;
    void visit(Load &ev) override;
    void visit(End &ev) override;

    ofVec2f screenToWorld(const ofVec2f &coords);
    ofVec2f worldToScreen(const ofVec2f &coords);

    ofVec2f globalToWorld(const ofVec2f &coords, std::shared_ptr<TileSet> tileset) const;
    ofVec2f worldToGlobal(const ofVec2f &coords, std::shared_ptr<TileSet> tileset) const;

    void jumpTo(const ofVec2f &worldCoords);
    void jumpTo(const POI &poi);
    void jumpZoom(float zoomLevel);
    void jumpToSequenceStep(size_t step, bool focusZoom = true);

    size_t addSequenceEvent(std::shared_ptr<SequenceEvent> ev, int position = -1);
    bool saveSequence(const std::string &name);
    bool loadSequence(const std::string &name);

    void renderScreenShot();
    bool rendering = false;
    ofVec2f screenSizeWorld;
    ofRectangle layoutBounds;
    float topLayoutWorld;
    ofPixels screenshot;

    void calculateViewMatrix();
    float updateScale();

    // GUI stuff
    bool hideGui = false;
    ofxImGui::Gui gui;
    void drawGUI();
    void drawWelcome();
    bool disableMouse = false;
    bool disableKeyboard = false;
    bool quitting = false;
    bool newProject = true;

    std::unordered_map<std::string, float *> parameters;
};

void pasteInto(ofPixels &dstPixels, const ofPixels &src, int x, int y);

std::string formatTime(float timeSeconds);