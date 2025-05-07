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

    tiles = ofDirectory("/home/arran/Desktop/04_47/32.0/0.0/");
    tiles.allowExt("jpg");
    tiles.listDir();

    currentView.zoomLevel = currentZoom.getValue();
    currentView.width = ofGetWidth();
    currentView.height = ofGetHeight();
    currentZoomLevel = std::floor(currentZoom.getValue());

    zoomCenter = {0.f, 0.f};

    loader.start();
    loadTileList();
    loadVisibleTiles(currentView);
    preloadZoom(currentZoomLevel - 1);
    preloadZoom(currentZoomLevel - 2);
    updateCaches();

    ofResetElapsedTimeCounter();
}

//--------------------------------------------------------------
void ofApp::update()
{
    loader.dispatchMainCallbacks(32);

    float zoomTarget = currentZoom.getTargetValue();
    int targetZoomLevel = std::floor(zoomTarget);

    bool zoomUpdated = currentZoom.process(fpsCounter.getLastFrameSecs());

    bool shouldLoadZoomIn = false;
    bool shouldLoadZoomOut = false;
    bool shouldLoadPan = false;

    if (zoomUpdated)
    {
        currentZoomLevel = std::floor(currentZoom.getValue());
        currentZoomLevel = std::clamp(currentZoomLevel, maxZoomLevel, minZoomLevel);

        ofVec2f zlCoordsBefore = screenToZoomLevelCoords({ofGetMouseX(), ofGetMouseY()});
        currentView.scale = std::powf(2.f, static_cast<float>(currentZoomLevel) - currentZoom.getValue());
        currentView.width = ofGetWidth() / currentView.scale;
        currentView.height = ofGetWidth() / currentView.scale;
        currentView.scale = currentView.scale;
        currentView.zoomLevel = currentZoomLevel;

        ofVec2f zlCoordsAfter = screenToZoomLevelCoords({ofGetMouseX(), ofGetMouseY()});
        currentView.offset += (zlCoordsBefore - zlCoordsAfter);

        if (currentZoomLevel != lastZoomLevel)
        {
            float multiplier = std::powf(2.f, (lastZoomLevel - currentZoomLevel));
            currentView.offset *= multiplier;
            // zoomCenter *= multiplier;
            lastZoomLevel = currentZoomLevel;
        }

        // zoomOffset = currentView.scale * (zoomCenter);
    }

    if (hasPanned)
    {
        hasPanned = false;
        shouldLoadZoomIn = true;
        shouldLoadZoomOut = true;
        shouldLoadPan = true;
    }

    if (targetZoomLevel > currentZoomLevel)
        shouldLoadZoomOut = true;
    else if (targetZoomLevel < currentZoomLevel)
        shouldLoadZoomIn = true;

    if (shouldLoadZoomOut)
        preloadZoom(currentZoomLevel + 1);

    if (shouldLoadZoomIn)
        preloadZoom(currentZoomLevel - 1);

    if (shouldLoadPan)
        loadVisibleTiles(currentView);

    updateCaches();

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
    blendShader.setUniformTexture("texA", fboA.getTexture(), 1);
    blendShader.setUniformTexture("texB", fboB.getTexture(), 2);
    blendShader.setUniform1f("alpha", blendAlpha);
    plane.draw();
    blendShader.end();

    if (showDebug && drawCached)
    {
        ofPushMatrix();
        ofScale(currentView.scale);
        ofTranslate(currentView.offset);
        ofSetColor(0, 0, 255);
        for (auto &it : cacheSecondary)
        {
            TileKey key = it.first;
            if (key.theta != thetaLevels[currentView.thetaIndex])
                continue;

            float multiplier = key.zoom / std::powf(2, currentZoomLevel);
            float x = multiplier * key.x;
            float y = multiplier * key.y;
            float w = multiplier * key.width;
            float h = multiplier * key.height;

            ofDrawRectangle(x + 1, y + 1, w - 2, h - 2);
        }
        ofPopMatrix();
    }

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

        std::string status = std::format(
            "Zoom: {:.2f} (ZoomLevel {}, Scale: {:.2f}), Theta: {:.2f} (Theta Index: {}, blend: {:.2f})\nCache: MAIN {}, SECONDARY {} (cache misses: {})",
            currentZoom.getValue(), currentZoomLevel, currentView.scale, currentView.theta, currentView.thetaIndex, blendAlpha, cacheMain.size(), cacheSecondary.size(), cacheMisses);

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
        currentView.theta = std::fmodf(currentView.theta + 180.f - 0.5f, 180.f);
    else if (key == OF_KEY_RIGHT)
        currentView.theta = std::fmodf(currentView.theta + 0.5f, 180.f);
    else if (key == 'd')
        showDebug = !showDebug;
    else if (key == 'c')
        drawCached = !drawCached;

    // compute theta index
    int lastThetaIndex = currentView.thetaIndex;
    currentView.thetaIndex = 0;
    for (size_t i = 0; i < thetaLevels.size(); i++)
    {
        if (thetaLevels[i] > currentView.theta)
            break;
        currentView.thetaIndex = i;
    }

    if (currentView.thetaIndex != lastThetaIndex)
        loadVisibleTiles(currentView);

    // compute alpha blend
    int t1 = thetaLevels[currentView.thetaIndex];
    int t2;
    if (currentView.thetaIndex == static_cast<int>(thetaLevels.size()) - 1)
        t2 = thetaLevels[0] + 180;
    else
        t2 = thetaLevels[currentView.thetaIndex + 1];

    blendAlpha = ofMap(currentView.theta, (float)t1, (float)(t2), 0.f, 1.f);
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
    ofVec2f mouse(x, y);
    // ofLogNotice() << ofToString(mouse);
    // currentView.offset = lastOffset - (mouseStart - mouse);
    hasPanned = true;

    ofVec2f delta = (mouse - mouseStart) / currentView.scale;

    currentView.offset += delta;

    mouseStart = ofVec2f(mouse);
}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button)
{
    mouseStart = ofVec2f(x, y);
    lastOffset = currentView.offset;
}

//--------------------------------------------------------------
void ofApp::mouseReleased(int x, int y, int button)
{
}

//--------------------------------------------------------------
void ofApp::mouseScrolled(int x, int y, float scrollX, float scrollY)
{
    currentZoom.setTarget(currentZoom.getTargetValue() - scrollY * 0.015);
    // lastZoomCenter = ofVec2f(lastZoomCenter);
    // zoomCenter.x = ofGetMouseX();
    // zoomCenter.y = ofGetMouseY();
}

//--------------------------------------------------------------
void ofApp::windowResized(int w, int h)
{
    currentView.width = w / currentView.scale;
    currentView.height = h / currentView.scale;

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
void ofApp::updateCaches()
{
    const int windowWidth = ofGetWidth() / currentView.scale;
    const int windowHeight = ofGetHeight() / currentView.scale;

    int zoom = static_cast<int>(std::floor(std::powf(2, currentZoomLevel)));

    const int left = -currentView.offset.x - windowWidth * 0.2f;
    const int right = -currentView.offset.x + windowWidth * 1.2f;
    const int top = -currentView.offset.y - windowHeight * 0.2f;
    const int bottom = -currentView.offset.y + windowHeight * 1.2f;

    int t1 = thetaLevels[currentView.thetaIndex];
    int t2 = thetaLevels[(currentView.thetaIndex + 1) % thetaLevels.size()];

    // 1. Demote from MAIN
    for (auto it = cacheMain.begin(); it != cacheMain.end();)
    {
        TileKey key = it->first;

        if (
            (key.zoom != zoom) ||
            (key.x >= right || key.x + key.width <= left || key.y >= bottom || key.y + key.height <= top) ||
            (key.theta != t1 && key.theta != t2))
        {
            cacheSecondary.put(key, it->second);
            it = cacheMain.erase(it);
        }
        else
            ++it;
    }

    // 2. Check which tiles are needed
    for (const TileKey &key : avaliableTiles[zoom])
    {
        if (key.x >= right || key.x + key.width <= left ||
            key.y >= bottom || key.y + key.height <= top)
            continue;

        if (key.theta != t1 && key.theta != t2)
            continue;

        // tile is needed, make sure it is in cacheMain
        if (!cacheMain.count(key))
        {
            // not in cacheMain, check secondary

            ofTexture tile;
            if (cacheSecondary.get(key, tile))
            {
                // promote to main
                cacheMain[key] = tile;
                cacheSecondary.erase(key);
            }
            else
            {
                // ofLogWarning("Cache miss");
                cacheMisses++;
                loader.requestLoad(key.filepath, [this, key](const std::string &, ofImage tile)
                                   { cacheMain[key] = tile.getTexture(); });
            }
        }
    }
}

//--------------------------------------------------------------
void ofApp::loadVisibleTiles(const View &view)
{
    const int windowWidth = ofGetWidth() / view.scale;
    const int windowHeight = ofGetHeight() / view.scale;

    int zoom = static_cast<int>(std::floor(std::powf(2, currentZoomLevel)));

    // preload thetas CW and CCW from current
    int t = thetaLevels[view.thetaIndex];
    int tCW1 = thetaLevels[(view.thetaIndex + 1) % thetaLevels.size()];
    int tCW2 = thetaLevels[(view.thetaIndex + 2) % thetaLevels.size()];
    int tCCW1 = thetaLevels[(view.thetaIndex + thetaLevels.size() - 1) % thetaLevels.size()];
    int tCCW2 = thetaLevels[(view.thetaIndex + thetaLevels.size() - 2) % thetaLevels.size()];

    // add an additional margin to preload tiles offscreen
    const int left = -view.offset.x - windowWidth * 0.2f;
    const int right = -view.offset.x + windowWidth * 1.2f;
    const int top = -view.offset.y - windowHeight * 0.2f;
    const int bottom = -view.offset.y + windowHeight * 1.2f;

    for (const TileKey &key : avaliableTiles[zoom])
    {
        if (key.x >= right || key.x + key.width <= left ||
            key.y >= bottom || key.y + key.height <= top)
            continue;

        if (key.theta != t && key.theta != tCW1 && key.theta != tCW2 && key.theta != tCCW1 && key.theta != tCCW2)
            continue;

        if (cacheMain.count(key))
            continue;

        if (!cacheSecondary.contains(key, true))
            loader.requestLoad(key.filepath, [this, key](const std::string &, ofImage tile)
                               { cacheSecondary.put(key, tile.getTexture()); });
    }
}

void ofApp::preloadZoom(int level)
{
    if (level < maxZoomLevel || level > minZoomLevel)
        return;

    int t1 = thetaLevels[currentView.thetaIndex];
    int t2 = thetaLevels[(currentView.thetaIndex + 1) % thetaLevels.size()];

    const int windowWidth = ofGetWidth() / currentView.scale;
    const int windowHeight = ofGetHeight() / currentView.scale;

    const int left = -currentView.offset.x;
    const int right = -currentView.offset.x + windowWidth;
    const int top = -currentView.offset.y;
    const int bottom = -currentView.offset.y + windowHeight;

    int preloadCount = 0;
    float multiplier = std::powf(2.f, (level - currentZoomLevel));
    int zoom = static_cast<int>(std::floor(std::powf(2, level)));

    for (const TileKey &key : avaliableTiles[zoom])
    {
        if (key.x * multiplier >= right || (key.x + key.width) * multiplier <= left ||
            key.y * multiplier >= bottom || (key.y + key.height) * multiplier <= top)
            continue;

        if (key.theta != t1 && key.theta != t2)
            continue;

        if (!cacheSecondary.contains(key, false))
        {
            loader.requestLoad(key.filepath, [this, key](const std::string &, ofImage tile)
                               { cacheSecondary.put(key, tile.getTexture()); });
            preloadCount++;
        }
    }
    // ofLogNotice() << "- preloaded " << ofToString(preloadCount) << " tiles for next zoom level";
}

void ofApp::drawTiles()
{
    const int windowWidth = ofGetWidth() / currentView.scale;
    const int windowHeight = ofGetHeight() / currentView.scale;

    const int left = -currentView.offset.x - zoomOffset.x;
    const int right = -currentView.offset.x - zoomOffset.x + windowWidth;
    const int top = -currentView.offset.y - zoomOffset.y;
    const int bottom = -currentView.offset.y - zoomOffset.y + windowHeight;

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
    int t1 = thetaLevels[currentView.thetaIndex];
    int t2 = thetaLevels[(currentView.thetaIndex + 1) % thetaLevels.size()];

    for (auto it = cacheMain.begin(); it != cacheMain.end(); ++it)
    {
        const TileKey &key = it->first;
        const ofTexture &tile = it->second;

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
            ofTranslate(currentView.offset + zoomCenter);
            ofScale(currentView.scale);
            ofSetColor(255);
            tile.draw(key.x, key.y);
            if (showDebug)
            {
                ofSetColor(255, 0, 0);
                ofSetLineWidth(3.f);
                ofDrawRectangle(key.x, key.y, key.width, key.height);
            }
            ofPopMatrix();
            fboA.end();
        }
        else if (key.theta == t2)
        {
            fboB.begin();
            ofPushMatrix();
            ofTranslate(currentView.offset + zoomCenter);
            ofScale(currentView.scale);
            ofSetColor(255);
            tile.draw(key.x, key.y);
            if (showDebug)
            {
                ofSetColor(0, 255, 0);
                ofSetLineWidth(3.f);
                ofDrawRectangle(key.x, key.y, key.width, key.height);
            }
            ofPopMatrix();
            fboB.end();
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

ofVec2f ofApp::screenToZoomLevelCoords(ofVec2f s)
{
    return (s * currentView.scale) + currentView.offset;
}

ofVec2f ofApp::zoomLevelToScreenCoords(ofVec2f z)
{
    return (z - currentView.offset) / currentView.scale;
}
