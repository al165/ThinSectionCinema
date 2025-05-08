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

    float prevZoom = currentZoom.getValue();
    bool zoomUpdated = currentZoom.process(fpsCounter.getLastFrameSecs());
    bool zoomingIn = prevZoom > currentZoom.getValue();

    bool shouldLoadZoomIn = false;
    bool shouldLoadZoomOut = false;

    if (zoomUpdated)
    {
        ofVec2f worldBefore = screenToWorld(zoomCenter);
        currentView.scale = std::powf(2.f, static_cast<float>(currentZoomLevel) - currentZoom.getValue());
        ofVec2f worldAfter = screenToWorld(zoomCenter);
        currentView.offset += (worldBefore - worldAfter);

        currentZoomLevel = std::clamp(
            static_cast<int>(std::floor(currentZoom.getValue())),
            maxZoomLevel, minZoomLevel);

        if (currentZoomLevel != lastZoomLevel)
        {
            // changed zoom level, rescale everything
            float multiplier = std::powf(2.f, (lastZoomLevel - currentZoomLevel));
            currentView.scale = std::powf(2.f, static_cast<float>(currentZoomLevel) - currentZoom.getValue());

            currentView.offset *= multiplier;
            zoomCenter *= multiplier;
            lastZoomLevel = currentZoomLevel;
        }

        if (currentView.scale > 0.8 && zoomingIn)
            shouldLoadZoomIn = true;
        else if (currentView.scale < 0.6 && !zoomingIn)
            shouldLoadZoomOut = true;
    }

    loadVisibleTiles(currentView);

    if (shouldLoadZoomOut)
        preloadZoom(currentZoomLevel + 1);

    if (shouldLoadZoomIn)
        preloadZoom(currentZoomLevel - 1);

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

    ofRectangle viewWorld(
        screenToWorld({0.f, 0.f}),
        screenToWorld({static_cast<float>(ofGetWidth()), static_cast<float>(ofGetHeight())}));
    float right = viewWorld.getRight();
    float left = viewWorld.getLeft();
    float top = viewWorld.getTop();
    float bottom = viewWorld.getBottom();

    ofVec2f cursorWorld = screenToWorld({static_cast<float>(ofGetMouseX()), static_cast<float>(ofGetMouseY())});
    ofVec2f cursorGlobal = worldToGlobal(cursorWorld);

    if (showDebug)
    {
        ofPushMatrix();
        ofScale(currentView.scale);
        ofTranslate(-currentView.offset);

        if (drawCached)
        {
            ofPushStyle();
            ofSetColor(0, 0, 255);
            ofNoFill();
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

                if (x > right || key.x + key.width < left ||
                    y > bottom || key.y + key.height < top)
                    continue;

                ofDrawRectangle(x + 2, y + 2, w - 4, h - 4);
            }
            ofPopStyle();
        }

        ofPushStyle();
        ofNoFill();
        ofSetColor(0, 255, 0);
        ofSetLineWidth(1.f);
        ofDrawLine(cursorWorld.x, top, cursorWorld.x, bottom);
        ofDrawLine(left, cursorWorld.y, right, cursorWorld.y);
        ofSetColor(255, 0, 0);
        ofSetLineWidth(6.f);
        ofDrawRectangle(viewWorld);
        ofPopStyle();

        ofPopMatrix();
    }

    fboFinal.end();

    fboFinal.draw(0, 0);

    fpsHistory.push_back(delta);
    while (fpsHistory.size() > historyLength)
        fpsHistory.pop_front();

    if (showDebug)
    {
        // ofDrawBitmapStringHighlight("Frame delta (ms): " + ofToString(delta), 0, 20);
        // const int graphHeight = 40;
        // const int graphWidth = 120;
        // ofSetColor(0, 200);
        // ofFill();
        // ofDrawRectangle(0, 20, graphWidth, graphHeight);
        // ofSetColor(255);
        // ofNoFill();
        // ofDrawRectangle(0, 20, graphWidth, graphHeight);

        // ofBeginShape();
        // for (size_t i = 0; i < fpsHistory.size(); i++)
        // {
        //     ofVertex(
        //         ofMap(i, 0, historyLength, 0, graphWidth),
        //         ofMap(fpsHistory[i], 0.f, 1000.f, graphHeight + 20, 20, true));
        // }
        // ofEndShape();

        std::string coordinates = std::format(
            "Offset: {:.2f}, {:.2f}, Mouse: world {:.2f}, {:.2f} global {:.4f}, {:.4f}",
            currentView.offset.x, currentView.offset.y, cursorWorld.x, cursorWorld.y, cursorGlobal.x, cursorGlobal.y);

        ofDrawBitmapStringHighlight(coordinates, 0, ofGetHeight() - 40);

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
void ofApp::mouseMoved(int x, int y)
{
}

//--------------------------------------------------------------
void ofApp::mouseDragged(int x, int y, int button)
{
    ofVec2f mouse(x, y);
    ofVec2f delta = (mouse - mouseStart) / currentView.scale;
    currentView.offset -= delta;
    mouseStart.set(x, y);
}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button)
{
    mouseStart.set(x, y);
}

//--------------------------------------------------------------
void ofApp::mouseReleased(int x, int y, int button)
{
    loadVisibleTiles(currentView);
}

//--------------------------------------------------------------
void ofApp::mouseScrolled(int x, int y, float scrollX, float scrollY)
{
    zoomCenter.set(x, y);
    currentZoom.jumpTo(currentZoom.getValue() - scrollY * 0.015f);
}

//--------------------------------------------------------------
void ofApp::windowResized(int w, int h)
{
    fboA.allocate(w, h, GL_RGBA);
    fboB.allocate(w, h, GL_RGBA);
    fboFinal.allocate(w, h, GL_RGBA);

    plane.set(ofGetWidth(), ofGetHeight());
    plane.setPosition(ofGetWidth() / 2, ofGetHeight() / 2, 0);
    plane.mapTexCoordsFromTexture(fboFinal.getTexture());

    updateCaches();
}

//--------------------------------------------------------------
void ofApp::updateCaches()
{
    int zoom = static_cast<int>(std::floor(std::powf(2, currentZoomLevel)));

    ofRectangle viewWorld(
        screenToWorld({0.f, 0.f}),
        screenToWorld({static_cast<float>(ofGetWidth()), static_cast<float>(ofGetHeight())}));

    int t1 = thetaLevels[currentView.thetaIndex];
    int t2 = thetaLevels[(currentView.thetaIndex + 1) % thetaLevels.size()];

    float right = viewWorld.getRight();
    float left = viewWorld.getLeft();
    float top = viewWorld.getTop();
    float bottom = viewWorld.getBottom();

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
        if (cacheMain.count(key))
            continue;

        if (
            (key.x >= right || key.x + key.width <= left || key.y >= bottom || key.y + key.height <= top) ||
            (key.theta != t1 && key.theta != t2))
            continue;

        ofTexture tile;
        if (cacheSecondary.get(key, tile))
        {
            cacheMain[key] = tile;
            cacheSecondary.erase(key);
        }
        else
        {
            loader.requestLoad(key.filepath, [this, key](const std::string &, ofImage tile)
                               { cacheMisses++;
                                 cacheMain[key] = tile.getTexture(); });
        }
    }
}

//--------------------------------------------------------------
void ofApp::loadVisibleTiles(const View &view)
{
    int zoom = static_cast<int>(std::floor(std::powf(2, currentZoomLevel)));

    // preload thetas CW and CCW from current
    int t = thetaLevels[view.thetaIndex];
    int tCW1 = thetaLevels[(view.thetaIndex + 1) % thetaLevels.size()];
    int tCW2 = thetaLevels[(view.thetaIndex + 2) % thetaLevels.size()];
    int tCCW1 = thetaLevels[(view.thetaIndex + thetaLevels.size() - 1) % thetaLevels.size()];

    // add an additional margin to preload tiles offscreen
    ofRectangle viewWorld(
        screenToWorld({0.f, 0.f}),
        screenToWorld({static_cast<float>(ofGetWidth()), static_cast<float>(ofGetHeight())}));

    viewWorld.scaleFromCenter(1.3f);
    float right = viewWorld.getRight();
    float left = viewWorld.getLeft();
    float top = viewWorld.getTop();
    float bottom = viewWorld.getBottom();

    for (const TileKey &key : avaliableTiles[zoom])
    {
        if (key.x >= right || key.x + key.width <= left ||
            key.y >= bottom || key.y + key.height <= top)
            continue;

        if (key.theta != t && key.theta != tCW1 && key.theta != tCW2 && key.theta != tCCW1)
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

    float multiplier = std::powf(2.f, (level - currentZoomLevel));

    int t1 = thetaLevels[currentView.thetaIndex];
    int t2 = thetaLevels[(currentView.thetaIndex + 1) % thetaLevels.size()];

    ofRectangle viewWorld(
        screenToWorld({0.f, 0.f}),
        screenToWorld({static_cast<float>(ofGetWidth()), static_cast<float>(ofGetHeight())}));

    float right = viewWorld.getRight() / multiplier;
    float left = viewWorld.getLeft() / multiplier;
    float top = viewWorld.getTop() / multiplier;
    float bottom = viewWorld.getBottom() / multiplier;

    int preloadCount = 0;
    int zoom = static_cast<int>(std::floor(std::powf(2, level)));

    for (const TileKey &key : avaliableTiles[zoom])
    {
        if (key.theta != t1 && key.theta != t2)
            continue;

        if (key.x >= right || (key.x + key.width) <= left ||
            key.y >= bottom || (key.y + key.height) <= top)
            continue;

        if (!cacheSecondary.contains(key, false))
        {
            loader.requestLoad(key.filepath, [this, key](const std::string &, ofImage tile)
                               { cacheSecondary.put(key, tile.getTexture()); });
            preloadCount++;
        }
    }
}

void ofApp::drawTiles()
{
    numberVisibleTiles = 0;

    ofNoFill();
    ofSetColor(255);
    ofSetLineWidth(1.f);

    fboA.begin();
    ofBackground(0);
    fboA.end();

    fboB.begin();
    ofBackground(0);
    fboB.end();

    int t1 = thetaLevels[currentView.thetaIndex];
    int t2 = thetaLevels[(currentView.thetaIndex + 1) % thetaLevels.size()];

    for (auto it = cacheMain.begin(); it != cacheMain.end(); ++it)
    {
        const TileKey &key = it->first;
        const ofTexture &tile = it->second;

        // draw thetas on different fbos
        if (key.theta == t1)
        {
            fboA.begin();
            ofPushMatrix();
            ofScale(currentView.scale);
            ofTranslate(-currentView.offset);
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
            ofScale(currentView.scale);
            ofTranslate(-currentView.offset);
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
    std::string levels = "";
    for (const int t : thetaLevels)
        levels += " " + t;

    ofLogNotice() << levels;

    // Get all the tiles
    for (const ofFile &zoomDir : zoomLevelsDirs)
    {
        if (!zoomDir.isDirectory())
            continue;

        int zoom = ofToInt(zoomDir.getFileName());

        ofLogNotice() << "- Zoom level " << zoom;

        tiles = ofDirectory(tileDir.getAbsolutePath() + "/" + ofToString(zoom) + ".0/0.0/");
        tiles.allowExt("jpg");
        tiles.listDir();

        auto tileFiles = tiles.getFiles();
        ofLogNotice() << "  - Found " << tileFiles.size() + " tiles";

        ofVec2f zoomSize(0.f, 0.f);

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

            zoomSize.x = std::max(zoomSize.x, static_cast<float>(x + width));
            zoomSize.y = std::max(zoomSize.y, static_cast<float>(y + height));
        }

        avaliableTiles[zoom] = tiles;
        zoomWorldSizes[zoom] = zoomSize;
    }
}

ofVec2f ofApp::screenToWorld(ofVec2f coords) const
{
    return (coords / currentView.scale) + currentView.offset;
}

ofVec2f ofApp::worldToScreen(ofVec2f coords) const
{
    return (coords - currentView.offset) * currentView.scale;
}

ofVec2f ofApp::globalToWorld(ofVec2f coords) const
{
    int zoom = static_cast<int>(std::floor(std::powf(2, currentZoomLevel)));
    ofVec2f zoomSize(0.f, 0.f);
    try
    {
        zoomSize.set(zoomWorldSizes.at(zoom));
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return coords;
    }
    return coords * zoomSize;
}

ofVec2f ofApp::worldToGlobal(ofVec2f coords) const
{
    int zoom = static_cast<int>(std::floor(std::powf(2, currentZoomLevel)));
    ofVec2f zoomSize(0.f, 0.f);
    try
    {
        zoomSize.set(zoomWorldSizes.at(zoom));
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return coords;
    }

    return coords / zoomSize;
}