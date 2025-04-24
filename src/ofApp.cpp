#include "ofApp.h"

//--------------------------------------------------------------
void ofApp::setup()
{
    ofSetWindowTitle("Tile Viewer");
    ofSetVerticalSync(true);

    // Setup shader stuff
    ofEnableAlphaBlending();
    int fboW = ofGetWidth();
    int fboH = ofGetHeight();
    fboA.allocate(fboW, fboH, GL_RGBA);
    fboB.allocate(fboW, fboH, GL_RGBA);
    fboFinal.allocate(fboW, fboH, GL_RGBA);

    if (!blendShader.load("blend"))
        ofLogError() << "Shader not loaded!";
    else
        ofLogNotice() << "Shader loaded successfully";

    plane.set(ofGetWidth(), ofGetHeight());
    plane.setScale(1, -1, 1);
    plane.setPosition(0, ofGetHeight(), 0);
    plane.mapTexCoords(0, 0, ofGetWidth(), ofGetHeight());

    ofLogNotice() << "plane size: " << plane.getWidth() << ", " << plane.getHeight();

    tiles = ofDirectory("/home/arran/Desktop/04_47/32.0/0.0/");
    tiles.allowExt("jpg");
    tiles.listDir();

    currentView.x = offset.x;
    currentView.y = offset.y;
    currentView.zoomLevel = currentZoom.getValue();
    currentView.width = ofGetWidth();
    currentView.height = ofGetHeight();

    loader.start();
    loadTileList();
    loadVisibleTiles(currentView);
    preloadZoomIn();

    ofResetElapsedTimeCounter();
}

//--------------------------------------------------------------
void ofApp::update()
{
    loader.dispatchMainCallbacks(12);

    float prevZoom = currentZoom.getValue();
    bool zoomUpdated = currentZoom.process(fpsCounter.getLastFrameSecs());

    if (zoomUpdated)
    {
        currentZoomLevel = std::floor(currentZoom.getValue());
        currentZoomLevel = std::clamp(currentZoomLevel, maxZoomLevel, minZoomLevel);

        scale = std::powf(2.f, static_cast<float>(currentZoomLevel) - currentZoom.getValue());
        currentView.width = ofGetWidth() / scale;
        currentView.height = ofGetWidth() / scale;

        if (currentZoomLevel != lastZoomLevel)
        {
            float multiplier = std::powf(2.f, (lastZoomLevel - currentZoomLevel));
            offset *= multiplier;
            lastZoomLevel = currentZoomLevel;
        }

        loadVisibleTiles(currentView);

        bool zoomingIn = (prevZoom > currentZoom.getValue());
        if (scale > 0.8f && zoomingIn)
            preloadZoomIn();
        else if (scale < 0.7f && !zoomingIn)
            preloadZoomOut();
    }

    fpsCounter.update();
}

//--------------------------------------------------------------
void ofApp::draw()
{
    float elapsedTime = ofGetElapsedTimef();
    float delta = 1000.f * (elapsedTime - lastFrameTime);
    lastFrameTime = elapsedTime;

    ofBackground(0, 0, 255);
    drawTiles();

    fboFinal.begin();
    ofClear(0);
    blendShader.begin();
    // blendShader.bindDefaults();
    blendShader.setUniformTexture("texA", fboA.getTexture(), 1);
    blendShader.setUniformTexture("texB", fboB.getTexture(), 2);
    blendShader.setUniform1f("alpha", blendAlpha);
    plane.draw();
    blendShader.end();
    fboFinal.end();

    fboFinal.draw(0, 0);

    fpsHistory.push_back(delta);
    while (fpsHistory.size() > historyLength)
        fpsHistory.pop_front();

    if (showDebug)
    {
        ofDrawBitmapStringHighlight("Frame delta (ms): " + ofToString(delta), 0, 20);
        const int graphHeight = 40;
        const int graphWidth = 120;
        ofSetColor(0, 200);
        ofFill();
        ofDrawRectangle(0, 20, graphWidth, graphHeight);
        ofSetColor(255);
        ofNoFill();
        ofDrawRectangle(0, 20, graphWidth, graphHeight);

        ofBeginShape();
        for (size_t i = 0; i < fpsHistory.size(); i++)
        {
            ofVertex(
                ofMap(i, 0, historyLength, 0, graphWidth),
                ofMap(fpsHistory[i], 0.f, 1000.f, graphHeight + 20, 20, true));
        }
        ofEndShape();

        std::string status = "Zoom: " + ofToString(currentZoom.getValue()) + ", Zoom level: " + ofToString(currentZoomLevel) + ", Scale: " + ofToString(scale);
        status += ", theta: " + ofToString(theta) + ", thetaIndex: " + ofToString(thetaIndex) + ", blendAlpha: " + ofToString(blendAlpha);
        status += ", Cache: " + ofToString(tileCache.size()) + ", Visible tiles: " + ofToString(numberVisibleTiles);
        status += "\nOffset: " + ofToString(offset);
        ofDrawBitmapStringHighlight(status, 0, ofGetHeight() - 20);
    }

    fpsCounter.newFrame();
}

//--------------------------------------------------------------
void ofApp::exit()
{
    loader.stop();
}

//--------------------------------------------------------------
void ofApp::keyPressed(int key)
{
    if (key == OF_KEY_LEFT)
        theta = std::fmodf(theta + 180.f - 1.f, 180.f);
    else if (key == OF_KEY_RIGHT)
        theta = std::fmodf(theta + 1.f, 180.f);
    if (key == 'd')
        showDebug = !showDebug;

    // compute theta index
    int lastThetaIndex = thetaIndex;
    thetaIndex = 0;
    for (size_t i = 0; i < thetaLevels.size(); i++)
    {
        if (thetaLevels[i] > theta)
            break;
        thetaIndex = i;
    }

    if (thetaIndex != lastThetaIndex)
        loadVisibleTiles(currentView);

    // compute alpha blend
    int t1 = thetaLevels[thetaIndex];
    int t2;
    if (thetaIndex == thetaLevels.size() - 1)
        t2 = thetaLevels[0] + 180;
    else
        t2 = thetaLevels[thetaIndex + 1];

    blendAlpha = ofMap(theta, (float)t1, (float)(t2), 0.f, 1.f);
}

//--------------------------------------------------------------
void ofApp::keyReleased(int key)
{
}

//--------------------------------------------------------------
void ofApp::mouseMoved(int x, int y)
{
}

//--------------------------------------------------------------
void ofApp::mouseDragged(int x, int y, int button)
{
    // ofLogNotice("ofApp::mouseDragged " + ofToString(x) + ", " + ofToString(y));
    ofVec2f mouse(x, y);
    offset = lastOffset - (mouseStart - mouse);
    loadVisibleTiles(currentView);
}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button)
{
    mouseStart = ofVec2f(x, y);
    lastOffset = offset;
}

//--------------------------------------------------------------
void ofApp::mouseReleased(int x, int y, int button)
{
}

//--------------------------------------------------------------
void ofApp::mouseScrolled(int x, int y, float scrollX, float scrollY)
{
    currentZoom.setTarget(currentZoom.getTargetValue() - scrollY * 0.015);
}

//--------------------------------------------------------------
void ofApp::windowResized(int w, int h)
{
    currentView.width = w / scale;
    currentView.height = h / scale;

    fboA.allocate(w, h, GL_RGBA);
    fboB.allocate(w, h, GL_RGBA);
    fboFinal.allocate(w, h, GL_RGBA);

    plane.set(ofGetWidth(), ofGetHeight());
    plane.setPosition(ofGetWidth() / 2, ofGetHeight() / 2, 0);
    plane.mapTexCoordsFromTexture(fboFinal.getTexture());

    loadVisibleTiles(currentView);
}

//--------------------------------------------------------------
void ofApp::dragEvent(ofDragInfo dragInfo)
{
}

//--------------------------------------------------------------
void ofApp::loadVisibleTiles(const View &view)
{
    static int lastZoomLoaded = -1;

    int zoom = static_cast<int>(std::floor(std::powf(2, currentZoomLevel)));

    // preload thetas CW and CCW from current
    int t = thetaLevels[thetaIndex];
    int tCW = thetaLevels[(thetaIndex + 1) % thetaLevels.size()];
    int tCCW = thetaLevels[(thetaIndex + thetaLevels.size() - 1) % thetaLevels.size()];

    // add an additional margin to preload tiles offscreen
    const int left = -offset.x - 520;
    const int right = -offset.x + view.width + 520;
    const int top = -offset.y - 384;
    const int bottom = -offset.y + view.height + 384;

    for (const TileKey &key : avaliableTiles[zoom])
    {
        if (key.x >= right || key.x + key.width <= left ||
            key.y >= bottom || key.y + key.height <= top ||
            key.zoom != zoom)
        {
            continue;
        }

        if (key.theta != t && key.theta != tCW && key.theta != tCCW)
            continue;

        if (!tileCache.contains(key))
        {
            loader.requestLoad(key.filepath, [this, key](const std::string &, ofImage tile)
                               { tileCache.put(key, tile.getTexture()); });
        }
    }

    lastZoomLoaded = zoom;
}

void ofApp::preloadZoomIn()
{
    int zoom = static_cast<int>(std::floor(std::powf(2, currentZoomLevel)));
    int t = thetaLevels[thetaIndex];

    static int lastNextZoom = -1;
    if (zoom <= 1)
        return;

    int zoomNext = zoom / 2;
    if (zoomNext == lastNextZoom)
        return;

    lastNextZoom = zoomNext;
    ofLogNotice("ofApp::preloadZoomIn()");

    const int windowWidth = ofGetWidth() / scale;
    const int windowHeight = ofGetHeight() / scale;

    // add an additional margin to preload tiles offscreen
    const int left = -offset.x - 520;
    const int right = -offset.x + windowWidth + 520;
    const int top = -offset.y - 384;
    const int bottom = -offset.y + windowHeight + 384;

    for (const TileKey &key : avaliableTiles[zoomNext])
    {
        if (key.x / 2 >= right || (key.x + key.width) / 2 <= left ||
            key.y / 2 >= bottom || (key.y + key.height) / 2 <= top ||
            key.theta != t)
        {
            continue;
        }

        if (!tileCache.contains(key))
        {
            loader.requestLoad(key.filepath, [this, key](const std::string &, ofImage tile)
                               { tileCache.put(key, tile.getTexture()); });
        }
    }
}

void ofApp::preloadZoomOut()
{
    int zoom = static_cast<int>(std::floor(std::powf(2, currentZoomLevel)));
    int t = thetaLevels[thetaIndex];

    static int lastNextZoom = -1;
    if (zoom >= 32)
        return;

    int zoomNext = zoom * 2;
    if (zoomNext == lastNextZoom)
        return;

    lastNextZoom = zoomNext;
    ofLogNotice("ofApp::preloadZoomOut()");

    const int windowWidth = ofGetWidth() / scale;
    const int windowHeight = ofGetHeight() / scale;

    // add an additional margin to preload tiles offscreen
    const int left = -offset.x - 520;
    const int right = -offset.x + windowWidth + 520;
    const int top = -offset.y - 384;
    const int bottom = -offset.y + windowHeight + 384;

    for (const TileKey &key : avaliableTiles[zoomNext])
    {
        if (key.x * 2 >= right || (key.x + key.width) * 2 <= left ||
            key.y * 2 >= bottom || (key.y + key.height) * 2 <= top ||
            key.theta != t)
        {
            continue;
        }

        if (!tileCache.contains(key))
        {
            loader.requestLoad(key.filepath, [this, key](const std::string &, ofImage tile)
                               { tileCache.put(key, tile.getTexture()); });
        }
    }
}

void ofApp::drawTiles()
{
    const int windowWidth = ofGetWidth() / scale;
    const int windowHeight = ofGetHeight() / scale;

    const int left = -offset.x;
    const int right = -offset.x + windowWidth;
    const int top = -offset.y;
    const int bottom = -offset.y + windowHeight;

    numberVisibleTiles = 0;

    ofNoFill();
    ofSetColor(255);
    ofSetLineWidth(1.0);

    fboA.begin();
    ofBackground(0);
    fboA.end();

    fboB.begin();
    ofBackground(0);
    fboB.end();

    int zoom = std::floor(std::powf(2.f, currentZoomLevel));
    int t1 = thetaLevels[thetaIndex];
    int t2 = thetaLevels[(thetaIndex + 1) % thetaLevels.size()];

    for (auto it = tileCache.begin(); it != tileCache.end(); ++it)
    {
        const TileKey &key = it->first;
        const ofTexture &tile = it->second.first;

        if (key.zoom != zoom)
            continue;

        if (key.x >= right || key.x + key.width <= left ||
            key.y >= bottom || key.y + key.height <= top)
            continue;

        // draw thetas on different fbos
        if (key.theta == t1)
        {
            fboA.begin();
            ofPushMatrix();
            ofScale(scale);
            ofTranslate(offset);
            ofSetColor(255);
            tile.draw(key.x, key.y);
            if (showDebug)
            {
                ofSetColor(255, 0, 0);
                ofDrawRectangle(key.x, key.y, key.width, key.height);
            }
            ofPopMatrix();
            fboA.end();

            tileCache.touch(it);
        }
        else if (key.theta == t2)
        {
            fboB.begin();
            ofPushMatrix();
            ofScale(scale);
            ofTranslate(offset);
            ofSetColor(255);
            tile.draw(key.x, key.y);
            if (showDebug)
            {
                ofSetColor(255, 0, 0);
                ofDrawRectangle(key.x, key.y, key.width, key.height);
            }
            ofPopMatrix();
            fboB.end();

            tileCache.touch(it);
        }
        else
            continue;

        numberVisibleTiles++;
    }
}

void ofApp::loadTileList()
{
    ofLogNotice("ofApp::loadTileList()");
    ofDirectory tileDir{"/home/arran/Desktop/04_47/"};
    tileDir.listDir();
    auto zoomLevelsDirs = tileDir.getFiles();

    // get list of theta levels (as ints)
    auto rotations = ofDirectory(tileDir.getAbsolutePath() + "/2.0/");
    auto rotationDirs = rotations.getFiles();
    thetaLevels.clear();
    for (const ofFile &thetaDir : rotationDirs)
    {
        if (!thetaDir.isDirectory())
            continue;

        int theta = ofToInt(thetaDir.getFileName());
        thetaLevels.push_back(theta);
    }

    std::sort(thetaLevels.begin(), thetaLevels.end());
    ofLogNotice("- Theta levels:");
    for (const int t : thetaLevels)
        ofLogNotice("   " + ofToString(t));

    // Get all the tiles
    for (const ofFile &zoomDir : zoomLevelsDirs)
    {
        if (!zoomDir.isDirectory())
            continue;

        int zoom = ofToInt(zoomDir.getFileName());

        ofLogNotice("- Zoom level: " + ofToString(zoom));

        tiles = ofDirectory(tileDir.getAbsolutePath() + "/" + ofToString(zoom) + ".0/0.0/");
        tiles.allowExt("jpg");
        tiles.listDir();

        auto tileFiles = tiles.getFiles();
        ofLogNotice(" - Found " + ofToString(tileFiles.size()) + " tiles");

        std::vector<TileKey> tiles;
        tiles.reserve(tileFiles.size());
        for (size_t i = 0; i < tileFiles.size(); i++)
        {
            std::string filename = tileFiles[i].getFileName();
            auto basename = ofSplitString(filename, ".");
            auto components = ofSplitString(basename[0], "x", true, true);

            int x = ofToInt(components[0]);
            int y = ofToInt(components[1]);
            int width = ofToInt(components[2]);
            int height = ofToInt(components[3]);

            for (const int t : thetaLevels)
            {
                std::string filepath = tileDir.getAbsolutePath() + "/" + ofToString(zoom) + ".0/" + ofToString(t) + ".0/" + filename;
                tiles.emplace_back(zoom, x, y, width, height, t, filepath);
            }
        }

        avaliableTiles[zoom] = tiles;
    }
}
