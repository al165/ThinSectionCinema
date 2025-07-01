#include "ofApp.h"

//--------------------------------------------------------------
void ofApp::setup()
{
    ofSetWindowTitle("As Gems in Metal");
    ofSetVerticalSync(true);

    // Setup shader stuff
    ofEnableAlphaBlending();
    int fboW = ofGetWidth();
    int fboH = ofGetHeight();
    fboA.allocate(fboW, fboH, GL_RGBA);
    fboB.allocate(fboW, fboH, GL_RGBA);
    fboFinal.allocate(fboW, fboH, GL_RGB);

    if (!blendShader.load("blend"))
        ofLogError() << "Shader not loaded!";
    else
        ofLogNotice() << "Shader loaded successfully";

    // Load config
    toml::table tbl;
    try
    {
        tbl = toml::parse_file(ofToDataPath("config.toml", true));
        std::cout << tbl << "\n";
    }
    catch (const toml::parse_error &err)
    {
        std::cerr << "Parsing failed:\n"
                  << err << "\n";
        return;
    }

    std::optional<std::string> rootFolder = tbl["scans_root"].value<std::string>();
    std::optional<std::string> scanName = tbl["scan_name"].value<std::string>();
    std::optional<std::string> scanName2 = tbl["secondary_name"].value<std::string>();

    ofLog() << "Loading config:";
    ofLog() << " - scans_root: " << rootFolder.value_or("<empty>");
    ofLog() << " - scan_name: " << scanName.value_or("<empty>");
    ofLog() << " - secondary_name: " << scanName2.value_or("<empty>");

    if (!rootFolder.has_value())
    {
        ofLogError() << "config `scans_root` is empty!";
        return;
    }

    if (!scanName.has_value())
    {
        ofLogError() << "config `scan_name` is empty!";
        return;
    }

    if (scanName2.has_value())
        nextTileSet.assign(scanName2.value());

    scanRoot.assign(rootFolder.value());

    plane.set(ofGetWidth(), ofGetHeight());
    plane.setScale(1, -1, 1);
    plane.setPosition(0, ofGetHeight(), 0);
    plane.mapTexCoords(0, 0, ofGetWidth(), ofGetHeight());

    zoomCenterWorld = {0.f, 0.f};
    currentZoomLevel = std::floor(currentZoom.getValue());
    currentView.scale = std::powf(2.f, static_cast<float>(currentZoomLevel) - currentZoom.getValue());
    screenRectangle = ofRectangle(0.f, 0.f, static_cast<float>(ofGetWidth()), static_cast<float>(ofGetHeight()));
    screenCenter = screenRectangle.getBottomRight() / 2.f;
    calculateViewMatrix();

    loader.start();
    currentView.tileset = scanName.value();

    loadTileList(currentView.tileset);
    int zoom = static_cast<int>(std::floor(std::powf(2, currentZoomLevel)));
    ofVec2f currentWoldSize = zoomWorldSizes[currentView.tileset][zoom];
    nextTileSetOffset = ofVec2f(currentWoldSize.x, 0.f);
    if (nextTileSet.length() > 0)
        loadTileList(nextTileSet);
    loadVisibleTiles(currentView);
    preloadZoom(currentZoomLevel - 1);
    preloadZoom(currentZoomLevel - 2);
    updateCaches();

    ofVec2f centerWorld = globalToWorld({0.5f, 0.5f});
    ofLogNotice() << centerWorld;
    currentView.offsetWorld.set(centerWorld);
    calculateViewMatrix();

    std::ofstream outfile;
    outfile.open("tween.csv", std::ofstream::out | std::ofstream::trunc);
    outfile << "frameCount,t,currentViewX,currentViewY,deltaX,deltaY" << std::endl;

    ffmpegRecorder.setup(true, false, {fboFinal.getWidth(), fboFinal.getHeight()}, recordingFps);
    ffmpegRecorder.setInputPixelFormat(OF_IMAGE_COLOR);

    ofResetElapsedTimeCounter();

    ofAddListener(viewTargetAnim.animFinished, this, &ofApp::animationFinished);

    viewTargets.emplace_back(0.264165, 0.148148);
    viewTargets.emplace_back(0.048030, 0.918519);
    viewTargets.emplace_back(0.360225, 0.681481);
    viewTargets.emplace_back(0.504315, 0.118519);
    viewTargets.emplace_back(0.936585, 0.562963);
    viewTargets.emplace_back(0.048030, 0.948148);
    viewTargets.emplace_back(0.240150, 0.177778);
    viewTargets.emplace_back(0.288180, 0.148148);
    viewTargets.emplace_back(0.288180, 0.118519);
    viewTargets.emplace_back(0.264165, 0.177778);
    viewTargets.emplace_back(0.936585, 0.474074);
    viewTargets.emplace_back(0.960600, 0.562963);
}

//--------------------------------------------------------------
void ofApp::update()
{
    loader.dispatchMainCallbacks(32);

    float dt;
    if (recording)
    {
        // update dt to target recording rate
        if (frameReady)
            dt = 1.f / recordingFps;
        else
            dt = 0.f;
    }
    else
        dt = fpsCounter.getLastFrameSecs();

    if (!recording || frameReady)
    {
        currentView.offsetWorld += offsetDelta;
        offsetDelta.set(0.f, 0.f);
        if (cycleTheta)
        {
            float nextTheta = currentTheta.getTargetValue() + 0.1f;
            currentTheta.setTarget(nextTheta);
        }
        viewTargetAnim.update(dt);
    }

    calculateViewMatrix();

    float prevZoom = currentZoom.getValue();
    bool zoomUpdated = currentZoom.process(dt);
    bool zoomingIn = prevZoom > currentZoom.getValue();

    bool shouldLoadZoomIn = false;
    bool shouldLoadZoomOut = false;

    if (zoomUpdated)
    {
        ofVec2f screenBeforeZoom = worldToScreen(zoomCenterWorld);
        currentView.scale = std::powf(2.f, static_cast<float>(currentZoomLevel) - currentZoom.getValue());
        calculateViewMatrix();
        ofVec2f screenAfterZoom = worldToScreen(zoomCenterWorld);

        ofVec2f screenDeltaWorld = screenToWorld(screenAfterZoom) - screenToWorld(screenBeforeZoom);
        currentView.offsetWorld += screenDeltaWorld;
        calculateViewMatrix();

        currentZoomLevel = std::clamp(
            static_cast<int>(std::floor(currentZoom.getValue())),
            maxZoomLevel, minZoomLevel);

        if (currentZoomLevel != lastZoomLevel)
        {
            float multiplier = std::powf(2.f, (lastZoomLevel - currentZoomLevel));
            currentView.scale = std::powf(2.f, static_cast<float>(currentZoomLevel) - currentZoom.getValue());

            zoomCenterWorld *= multiplier;
            rotationCenterWorld *= multiplier;
            viewTargetWorld *= multiplier;
            viewStartWorld *= multiplier;
            currentView.offsetWorld *= multiplier;
            nextTileSetOffset *= multiplier;
            lastZoomLevel = currentZoomLevel;
            calculateViewMatrix();
        }

        if (currentView.scale > 0.8 && zoomingIn)
            shouldLoadZoomIn = true;
        else if (currentView.scale < 0.6 && !zoomingIn)
            shouldLoadZoomOut = true;
    }

    rotationAngle.process(dt);
    calculateViewMatrix();

    bool thetaUpdated = currentTheta.process(dt);
    if (thetaUpdated)
    {
        currentView.theta = std::fmodf(currentTheta.getValue() + 180.f, 180.f);

        if (currentTheta.getTargetValue() < 0.0f)
        {
            currentTheta.setTarget(currentTheta.getTargetValue() + 180.f);
            currentTheta.setValue(currentTheta.getValue() + 180.f);
        }
        else if (currentTheta.getTargetValue() >= 180.0f)
        {
            currentTheta.setTarget(currentTheta.getTargetValue() - 180.f);
            currentTheta.setValue(currentTheta.getValue() - 180.f);
        }

        currentView.thetaIndex = 0;
        for (size_t i = 0; i < thetaLevels.size(); i++)
        {
            if (thetaLevels[i] > currentView.theta)
                break;
            currentView.thetaIndex = i;
        }

        // compute alpha blend
        int t1 = thetaLevels[currentView.thetaIndex];
        int t2;
        if (currentView.thetaIndex == static_cast<int>(thetaLevels.size()) - 1)
            t2 = thetaLevels[0] + 180;
        else
            t2 = thetaLevels[currentView.thetaIndex + 1];

        blendAlpha = ofMap(currentView.theta, (float)t1, (float)(t2), 0.f, 1.f);
    }

    currentView.viewWorld.set(
        screenToWorld({0.f + 6.f, 0.f + 6.f}),
        screenToWorld({static_cast<float>(ofGetWidth() - 12), static_cast<float>(ofGetHeight() - 12)}));

    if (viewTargetAnim.isAnimating() && !viewTargetAnim.getPaused())
    {
        t = viewTargetAnim.val();
        ofVec2f tween = viewTargetWorld * t + viewStartWorld * (1.f - t);
        offsetDelta.set(tween - currentView.offsetWorld);

        if (!recording)
        {
            // load tiles further down animation path
            View futureView(currentView);
            float t2 = std::sqrtf(t);
            ofVec2f tween2 = viewTargetWorld * t2 + viewStartWorld * (1.f - t2);

            futureView.offsetWorld = screenToWorld(tween2);
            loadVisibleTiles(futureView);
        }
    }
    else
    {
        loadVisibleTiles(currentView);
    }

    if (shouldLoadZoomOut && !recording)
        preloadZoom(currentZoomLevel + 1);

    if (shouldLoadZoomIn && !recording)
        preloadZoom(currentZoomLevel - 1);

    frameReady = updateCaches();

    fpsCounter.update();
}

//--------------------------------------------------------------
void ofApp::draw()
{
    float elapsedTime = ofGetElapsedTimef();
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

    ofVec2f cursor(static_cast<float>(ofGetMouseX()), static_cast<float>(ofGetMouseY()));
    ofVec2f cursorWorld = screenToWorld(cursor);
    ofVec2f cursorGlobal = worldToGlobal(cursorWorld);
    const float margin = 6.f;
    std::vector<ofVec2f> screenCorners = {
        {margin, margin},
        {ofGetWidth() - margin, margin},
        {ofGetWidth() - margin, ofGetHeight() - margin},
        {margin, ofGetHeight() - margin},
        {margin, margin},
    };

    if (showDebug && !recording)
    {
        ofPushMatrix();
        ofMultMatrix(viewMatrix);

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

                if (!isVisible(key))
                    continue;

                ofDrawRectangle(x + 2, y + 2, w - 4, h - 4);
            }
            ofPopStyle();
        }

        ofPushStyle();
        ofSetLineWidth(10.f);
        ofNoFill();
        ofSetColor(0, 255, 255);
        int zoom = static_cast<int>(std::floor(std::powf(2, currentZoomLevel)));
        ofRectangle nextTileSetBounds(nextTileSetOffset, nextTileSetOffset + zoomWorldSizes[nextTileSet][zoom]);
        ofDrawRectangle(nextTileSetBounds);
        ofPopStyle();

        ofPushStyle();
        ofNoFill();

        ofSetColor(255, 255, 0);
        ofDrawCircle(zoomCenterWorld, 10 / currentView.scale);

        ofSetColor(0, 255, 0);
        ofSetLineWidth(1.f);

        ofVec2f vLineStart = screenToWorld({cursor.x, 0.f});
        ofVec2f vLineEnd = screenToWorld({cursor.x, static_cast<float>(ofGetHeight())});

        ofVec2f hLineStart = screenToWorld({0.f, cursor.y});
        ofVec2f hLineEnd = screenToWorld({static_cast<float>(ofGetWidth()), cursor.y});

        ofDrawLine(vLineStart, vLineEnd);
        ofDrawLine(hLineStart, hLineEnd);
        ofSetColor(255, 255, 0);
        ofSetLineWidth(6.f);
        ofBeginShape();
        for (const auto &pt : screenCorners)
            ofVertex(screenToWorld(pt));
        ofEndShape();
        ofPopStyle();

        ofPopMatrix();
    }

    // ofDrawBitmapStringHighlight(ofToString(frameCount), 0, ofGetHeight());

    fboFinal.end();

    if (recording && frameReady)
    {
        // save fboFinal to video buffer
        fboFinal.readToPixels(framePixels);
        if (framePixels.getWidth() > 0 && framePixels.getHeight() > 0)
            ffmpegRecorder.addSingleFrame(framePixels);

        std::ofstream outfile;
        outfile.open("tween.csv", std::ofstream::out | std::ios_base::app);
        outfile << frameCount << "," << ofToString(t) << ","
                << ofToString(currentView.offsetWorld.x) << "," << ofToString(currentView.offsetWorld.y) << ","
                << ofToString(offsetDelta.x) << "," << ofToString(offsetDelta.y) << std::endl;

        frameCount++;
    }

    fboFinal.draw(0, 0);

    if (recording)
    {
        ofNoFill();
        ofSetColor(255, 0, 0);
        ofSetLineWidth(6);
        ofDrawRectangle(6, 6, ofGetWidth() - 12, ofGetHeight() - 12);
    }

    if (showDebug)
    {
        std::string coordinates = std::format(
            "Offset: {:.2f}, {:.2f}, Mouse: world {:.2f}, {:.2f} global {:.4f}, {:.4f}, ZoomCenter {:.2f}, {:.2f}, rotationAngle {:.2f}",
            currentView.offsetWorld.x, currentView.offsetWorld.y, cursorWorld.x, cursorWorld.y, cursorGlobal.x, cursorGlobal.y, zoomCenterWorld.x, zoomCenterWorld.y, rotationAngle.getValue());

        ofDrawBitmapStringHighlight(coordinates, 0, ofGetHeight() - 40);

        std::string status = std::format(
            "Zoom: {:.2f} (ZoomLevel {}, Scale: {:.2f}), Theta: {:.2f} (Theta Index: {}, blend: {:.2f})\nCache: MAIN {}, SECONDARY {} (cache misses: {}), frameReady {}",
            currentZoom.getValue(), currentZoomLevel, currentView.scale, currentView.theta, currentView.thetaIndex, blendAlpha, cacheMain.size(), cacheSecondary.size(), cacheMisses, frameReady);

        ofDrawBitmapStringHighlight(status, 0, ofGetHeight() - 20);

        std::string viewMatrixStr = std::format(
            "┏{:9.3f} {:9.3f} {:9.3f} {:9.3f}┓\n│{:9.3f} {:9.3f} {:9.3f} {:9.3f}│\n│{:9.3f} {:9.3f} {:9.3f} {:9.3f}│\n┗{:9.3f} {:9.3f} {:9.3f} {:9.3f}┛",
            viewMatrix._mat[0][0], viewMatrix._mat[1][0], viewMatrix._mat[2][0], viewMatrix._mat[3][0],
            viewMatrix._mat[0][1], viewMatrix._mat[1][1], viewMatrix._mat[2][1], viewMatrix._mat[3][1],
            viewMatrix._mat[0][2], viewMatrix._mat[1][2], viewMatrix._mat[2][2], viewMatrix._mat[3][2],
            viewMatrix._mat[0][3], viewMatrix._mat[1][3], viewMatrix._mat[2][3], viewMatrix._mat[3][3]);

        ofDrawBitmapStringHighlight(viewMatrixStr, ofGetWidth() - 360, 20);
    }

    fpsCounter.newFrame();
}

//--------------------------------------------------------------
void ofApp::exit()
{
    loader.stop();
    ffmpegRecorder.stop();
}

//--------------------------------------------------------------
void ofApp::keyPressed(int key)
{
    if (key == OF_KEY_LEFT)
    {
        float nextTheta = currentTheta.getTargetValue() - 0.3f;
        if (!recording || frameReady)
            currentTheta.setTarget(nextTheta);
    }
    else if (key == OF_KEY_RIGHT)
    {
        float nextTheta = currentTheta.getTargetValue() + 0.3f;
        if (!recording || frameReady)
            currentTheta.setTarget(nextTheta);
    }
    else if (key == OF_KEY_UP)
    {
        float nextAngle = rotationAngle.getTargetValue() + 1.f;
        if (!recording || frameReady)
            rotationAngle.setTarget(nextAngle);
    }
    else if (key == OF_KEY_DOWN)
    {
        float nextAngle = rotationAngle.getTargetValue() - 1.f;
        if (!recording || frameReady)
            rotationAngle.setTarget(nextAngle);
    }
    else if (key == 'd')
        showDebug = !showDebug;
    else if (key == 'c')
        drawCached = !drawCached;
    else if (key == 't')
        cycleTheta = !cycleTheta;
    else if (key == ' ')
    {
        ofVec2f t(viewTargets.back());
        viewTargets.pop_back();
        setViewTarget(globalToWorld(t));
    }
    else if (key == 'r')
    {
        if (!recording)
        {
            // initialise recording
            recordingFileName = ofToDataPath(ofGetTimestampString("%Y-%m-%d_%H:%M:%S"), true);
            ofLogNotice() << "Recording to " << recordingFileName;

            ffmpegRecorder.setup(true, false, {fboFinal.getWidth(), fboFinal.getHeight()}, recordingFps, 8500);
            ffmpegRecorder.setOutputPath(recordingFileName + ".mp4");
            ffmpegRecorder.setVideoCodec("libx264");
            ffmpegRecorder.startCustomRecord();
        }
        else
        {
            // save
            ffmpegRecorder.stop();
        }
        recording = !recording;
    }
}

//--------------------------------------------------------------
void ofApp::mouseMoved(int x, int y)
{
}

//--------------------------------------------------------------
void ofApp::mouseDragged(int x, int y, int button)
{
    ofVec2f currentMouse(x, y);

    ofVec2f worldBeforePan = screenToWorld(lastMouse);
    ofVec2f worldAfterPan = screenToWorld(currentMouse);
    ofVec2f worldDelta = worldBeforePan - worldAfterPan;

    currentView.offsetWorld += worldDelta;

    lastMouse.set(currentMouse);
    viewTargetAnim.pause();
}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button)
{
    lastMouse.set(x, y);
}

//--------------------------------------------------------------
void ofApp::mouseReleased(int x, int y, int button)
{
    loadVisibleTiles(currentView);
}

//--------------------------------------------------------------
void ofApp::mouseScrolled(int x, int y, float scrollX, float scrollY)
{
    rotationCenterWorld.set(screenToWorld({(float)x, (float)y}));
    rotationAngle.setTarget(rotationAngle.getTargetValue() - scrollX);

    zoomCenterWorld.set(screenToWorld({(float)x, (float)y}));
    currentZoom.speed = 2.f;
    currentZoom.setTarget(currentZoom.getTargetValue() - scrollY * 0.015f);
    focusViewTarget = false;
    viewTargetAnim.pause();
}

//--------------------------------------------------------------
void ofApp::windowResized(int w, int h)
{
    fboA.allocate(w, h, GL_RGBA);
    fboB.allocate(w, h, GL_RGBA);
    fboFinal.allocate(w, h, GL_RGB);

    screenRectangle = ofRectangle(0.f, 0.f, static_cast<float>(ofGetWidth()), static_cast<float>(ofGetHeight()));
    screenCenter = screenRectangle.getBottomRight() / 2.f;

    plane.set(ofGetWidth(), ofGetHeight());
    plane.setPosition(ofGetWidth() / 2, ofGetHeight() / 2, 0);
    plane.mapTexCoordsFromTexture(fboFinal.getTexture());

    updateCaches();
}

//--------------------------------------------------------------

bool ofApp::isVisible(const ofRectangle &worldRect, ofVec2f offset)
{
    // Get all 4 corners of the world rectangle
    std::vector<ofVec2f> worldCorners = {
        {worldRect.x + offset.x, worldRect.y + offset.y},
        {worldRect.x + worldRect.width + offset.x, worldRect.y + offset.y},
        {worldRect.x + worldRect.width + offset.x, worldRect.y + worldRect.height + offset.y},
        {worldRect.x + offset.x, worldRect.y + worldRect.height + offset.y}};

    // Transform to screen space
    std::vector<ofVec4f> screenCorners;
    for (const auto &pt : worldCorners)
        screenCorners.push_back(ofVec4f(pt.x, pt.y, 0.f, 1.f) * viewMatrix);

    // Compute screen-space bounding box of the transformed rectangle
    float minX = screenCorners[0].x, maxX = screenCorners[0].x;
    float minY = screenCorners[0].y, maxY = screenCorners[0].y;

    for (const auto &pt : screenCorners)
    {
        minX = std::min(minX, pt.x);
        maxX = std::max(maxX, pt.x);
        minY = std::min(minY, pt.y);
        maxY = std::max(maxY, pt.y);
    }

    ofRectangle screenBounds(minX, minY, maxX - minX, maxY - minY);

    // Check for intersection with screen rectangle
    return screenRectangle.intersects(screenBounds);
}

bool ofApp::isVisible(const TileKey &key, ofVec2f offset)
{
    return isVisible({(float)key.x, (float)key.y, (float)key.width, (float)key.height}, offset);
}

bool ofApp::updateCaches()
{
    bool frameReady = true;

    int zoom = static_cast<int>(std::floor(std::powf(2, currentZoomLevel)));

    int t1 = thetaLevels[currentView.thetaIndex];
    int t2 = thetaLevels[(currentView.thetaIndex + 1) % thetaLevels.size()];

    // 1. Demote from MAIN
    for (auto it = cacheMain.begin(); it != cacheMain.end();)
    {
        TileKey key = it->first;

        if (
            (key.zoom != zoom) ||
            (key.theta != t1 && key.theta != t2) ||
            (key.tileset != currentView.tileset && key.tileset != nextTileSet) ||
            (key.tileset == currentView.tileset && !isVisible(key)) ||
            (key.tileset == nextTileSet && !isVisible(key, nextTileSetOffset)))
        {
            cacheSecondary.put(key, it->second);
            it = cacheMain.erase(it);
        }
        else
            ++it;
    }

    // 2. Check which tiles are needed
    for (const TileKey &key : avaliableTiles[currentView.tileset][zoom])
    {
        if (cacheMain.count(key))
            continue;

        if (
            (key.theta != t1 && key.theta != t2) ||
            (key.tileset != currentView.tileset && key.tileset != nextTileSet) ||
            (key.tileset == currentView.tileset && !isVisible(key))
            // (key.tileset == nextTileSet && !isVisible(key, nextTileSetOffset))
        )
            continue;

        ofTexture tile;
        if (cacheSecondary.get(key, tile))
        {
            cacheMain[key] = tile;
            cacheSecondary.erase(key);
        }
        else
        {
            frameReady = false;
            loader.requestLoad(key.filepath, [this, key](const std::string &, ofImage tile)
                               { cacheMisses++;
                                 cacheMain[key] = tile.getTexture(); });
        }
    }

    for (const TileKey &key : avaliableTiles[nextTileSet][zoom])
    {
        if (cacheMain.count(key))
            continue;

        if (
            (key.theta != t1 && key.theta != t2) ||
            (key.tileset != currentView.tileset && key.tileset != nextTileSet) ||
            // (key.tileset == currentView.tileset && !isVisible(key))
            (key.tileset == nextTileSet && !isVisible(key, nextTileSetOffset)))
            continue;

        ofTexture tile;
        if (cacheSecondary.get(key, tile))
        {
            cacheMain[key] = tile;
            cacheSecondary.erase(key);
        }
        else
        {
            frameReady = false;
            loader.requestLoad(key.filepath, [this, key](const std::string &, ofImage tile)
                               { cacheMisses++;
                                 cacheMain[key] = tile.getTexture(); });
        }
    }

    return frameReady;
}

void ofApp::loadVisibleTiles(const View &view)
{
    return;
    int zoom = static_cast<int>(std::floor(std::powf(2, currentZoomLevel)));

    // preload thetas CW and CCW from current
    int t = thetaLevels[view.thetaIndex];
    int tCW1 = thetaLevels[(view.thetaIndex + 1) % thetaLevels.size()];
    int tCW2 = thetaLevels[(view.thetaIndex + 2) % thetaLevels.size()];
    int tCCW1 = thetaLevels[(view.thetaIndex + thetaLevels.size() - 1) % thetaLevels.size()];

    // add an additional margin to preload tiles offscreen
    ofRectangle margin(view.viewWorld);
    margin.scaleFromCenter(1.3f);
    float right = margin.getRight();
    float left = margin.getLeft();
    float top = margin.getTop();
    float bottom = margin.getBottom();

    for (const TileKey &key : avaliableTiles[view.tileset][zoom])
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

    float right = currentView.viewWorld.getRight() / multiplier;
    float left = currentView.viewWorld.getLeft() / multiplier;
    float top = currentView.viewWorld.getTop() / multiplier;
    float bottom = currentView.viewWorld.getBottom() / multiplier;

    int preloadCount = 0;
    int zoom = static_cast<int>(std::floor(std::powf(2, level)));

    for (const TileKey &key : avaliableTiles[currentView.tileset][zoom])
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
            ofMultMatrix(viewMatrix);
            ofSetColor(255);
            if (key.tileset == currentView.tileset)
                tile.draw(key.x, key.y);
            else
                tile.draw(key.x + nextTileSetOffset.x, key.y + nextTileSetOffset.y);

            if (showDebug && !recording)
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
            ofMultMatrix(viewMatrix);
            ofSetColor(255);
            if (key.tileset == currentView.tileset)
                tile.draw(key.x, key.y);
            else
                tile.draw(key.x + nextTileSetOffset.x, key.y + nextTileSetOffset.y);
            if (showDebug && !recording)
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

void ofApp::calculateViewMatrix()
{
    viewMatrix = ofMatrix4x4();
    float angle = std::fmodf(rotationAngle.getValue(), 360.f);

    viewMatrix.translate(-currentView.offsetWorld);
    viewMatrix.scale(currentView.scale, currentView.scale, 1);

    viewMatrix.rotate(angle, 0, 0, 1);
    viewMatrix.translate(screenCenter);

    viewMatrixInverted = viewMatrix.getInverse();
}

void ofApp::loadTileList(const std::string &set)
{
    ofLogNotice() << "ofApp::loadTileList()";

    std::string tileSetPath = scanRoot + set;
    ofLogNotice() << "Loading " << tileSetPath;

    ofDirectory tileDir{tileSetPath};
    tileDir.listDir();
    auto zoomLevelsDirs = tileDir.getFiles();

    if (zoomLevelsDirs.size() == 0)
    {
        ofLogWarning() << tileSetPath << " is empty";
        return;
    }

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
    ofLogNotice() << "- Theta levels:";
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
                tiles.emplace_back(zoom, x, y, width, height, t, filepath, set);
            }

            zoomSize.x = std::max(zoomSize.x, static_cast<float>(x + width));
            zoomSize.y = std::max(zoomSize.y, static_cast<float>(y + height));
        }

        avaliableTiles[set][zoom] = tiles;
        zoomWorldSizes[set][zoom] = zoomSize;
    }
}

void ofApp::setViewTarget(ofVec2f worldCoords, float delayS)
{
    viewTargetAnim.pause();
    viewTargetAnim.reset(0.f);

    zoomCenterWorld.set(worldCoords);
    viewTargetWorld.set(worldCoords);
    viewStartWorld.set(currentView.offsetWorld);

    viewTargetAnim.setDuration(8.f);
    viewTargetAnim.setRepeatType(AnimRepeat::PLAY_ONCE);
    viewTargetAnim.setCurve(AnimCurve::EASE_IN_EASE_OUT);
    viewTargetAnim.animateToAfterDelay(1.f, delayS);

    currentZoom.setTarget(2.f);
    currentZoom.speed = .1f;
}

void ofApp::animationFinished(ofxAnimatableFloat::AnimationEvent &ev)
{
    if (ev.who == &viewTargetAnim)
        currentZoom.setTarget(1.3f);
}

ofVec2f ofApp::screenToWorld(const ofVec2f &coords)
{
    ofVec4f screenCoords(coords.x, coords.y, 0.f, 1.f);
    return screenCoords * viewMatrixInverted;
}

ofVec2f ofApp::worldToScreen(const ofVec2f &coords)
{
    ofVec4f worldCoords(coords.x, coords.y, 0.f, 1.f);
    return worldCoords * viewMatrix;
}

ofVec2f ofApp::globalToWorld(const ofVec2f &coords) const
{
    int zoom = static_cast<int>(std::floor(std::powf(2, currentZoomLevel)));
    ofVec2f zoomSize(0.f, 0.f);
    try
    {
        zoomSize.set(zoomWorldSizes.at(currentView.tileset).at(zoom));
    }
    catch (const std::exception &e)
    {
        // ofLogError("globalToWorld") << e.what() << " (zoom: " << zoom << ", tileset: " << ")";
        return coords;
    }
    return coords * zoomSize;
}

ofVec2f ofApp::worldToGlobal(const ofVec2f &coords) const
{
    int zoom = static_cast<int>(std::floor(std::powf(2, currentZoomLevel)));
    ofVec2f zoomSize(0.f, 0.f);
    try
    {
        zoomSize.set(zoomWorldSizes.at(currentView.tileset).at(zoom));
    }
    catch (const std::exception &e)
    {
        // ofLogError("worldToGlobal") << e.what() << " (zoom: " << zoom << ")";
        return coords;
    }

    return coords / zoomSize;
}