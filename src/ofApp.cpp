#include "ofApp.h"

//--------------------------------------------------------------
void ofApp::setup()
{
    ofSetWindowTitle("As Gems in Metal");
    ofSetVerticalSync(true);

    // Setup shader stuff
    ofEnableAlphaBlending();

    if (!blendShader.load("blend"))
        ofLogError() << "Shader not loaded!";
    else
        ofLogNotice() << "Shader loaded successfully";

    // Load config
    toml::table tbl;
    try
    {
        tbl = toml::parse_file(ofToDataPath("config.toml", true));
        // std::cout << tbl << "\n";
    }
    catch (const toml::parse_error &err)
    {
        std::cerr << "Parsing failed:\n"
                  << err << "\n";
        return;
    }

    std::optional<std::string> rootFolder = tbl["scans_root"].value<std::string>();
    if (!rootFolder.has_value())
    {
        ofLogError() << "config `scans_root` is empty!";
        return;
    }
    scanRoot.assign(rootFolder.value());

    std::optional<std::string> layout = tbl["scan_layout"].value<std::string>();

    recordingFolder = tbl["recording_folder"].value<std::string>();
    recordingFileName = tbl["recording_filename"].value<std::string>();
    std::optional<float> fps = tbl["recording_fps"].value<float>();
    std::optional<float> minMove = tbl["min_moving_time"].value<float>();
    std::optional<float> maxMove = tbl["max_moving_time"].value<float>();
    std::optional<float> drillSpeedConfig = tbl["drill_speed"].value<float>();

    ofLogNotice() << "Loading config.toml:";
    ofLogNotice() << " - scans_root: " << rootFolder.value_or("<empty>");
    ofLogNotice() << " - scans_layout: " << layout.value_or("<empty>");
    ofLogNotice() << " - recording_folder: " << recordingFolder.value_or("<empty>");
    ofLogNotice() << " - recording_filename: " << recordingFileName.value_or("<empty>");
    ofLogNotice() << " - recording_fps: " << fps.value();

    tilesetManager.setRoot(scanRoot);

    recordingFps = fps.value_or(30.f);
    minMovingTime = minMove.value_or(8.f);
    maxMovingTime = maxMove.value_or(8.f);
    drillSpeed = drillSpeedConfig.value_or(0.4f);

    plane.set(ofGetWidth(), ofGetHeight());
    plane.setScale(1, -1, 1);
    plane.setPosition(0, ofGetHeight(), 0);
    plane.mapTexCoords(0, 0, ofGetWidth(), ofGetHeight());

    rotationAngle.maxStep = 0.5f;
    rotationAngle.warmUp = 0.f;
    rotationAngle.epsilon = 0.001f;
    currentZoomSmooth.maxStep = 0.5f;
    currentZoomSmooth.warmUp = 3.f;
    currentZoomSmooth.epsilon = 0.0001f;
    currentTheta.warmUp = 0.0f;

    ofVec2f centerWorld(0, 0);

    zoomCenterWorld = {0.f, 0.f};
    currentZoomLevel = std::floor(currentZoomSmooth.getValue());
    currentZoom = static_cast<int>(std::floor(std::powf(2, currentZoomLevel)));
    currentView.scale = std::powf(2.f, static_cast<float>(currentZoomLevel) - currentZoomSmooth.getValue());
    screenRectangle = ofRectangle(0.f, 0.f, static_cast<float>(ofGetWidth()), static_cast<float>(ofGetHeight()));
    screenCenter = screenRectangle.getBottomRight() / 2.f;
    calculateViewMatrix();

    std::ofstream outfile;
    outfile.open("tween.csv", std::ofstream::out | std::ofstream::trunc);
    outfile << "frameCount,t,currentViewX,currentViewY,deltaX,deltaY" << std::endl;

    std::ofstream pathfile;
    pathfile.open("path.csv");
    pathfile << "t,leftX,leftY,rightX,rightY" << std::endl;

    ofResetElapsedTimeCounter();

    gui.setup(nullptr, true);

    ofAddListener(viewTargetAnim.animFinished, this, &ofApp::animationFinished);
    ofAddListener(drillZoomAnim.animFinished, this, &ofApp::animationFinished);

    if (layout.has_value())
    {
        tilesetManager.loadLayout(layout.value());
        tilesetManager.computeLayout(currentZoom);
    }

    if (tilesetManager.tilesetList.size())
    {
        currentTileSet = tilesetManager.tilesetList[0];
        currentView.offsetWorld = worldToGlobal({0.5f, 0.5f}, currentTileSet);
        centerWorld = globalToWorld({0.5f, 0.5f}, currentTileSet);
    }

    loadSequence("sequence.json");
    currentView.offsetWorld.set(centerWorld);
    calculateViewMatrix();
}

//--------------------------------------------------------------
void ofApp::update()
{
    loader.dispatchMainCallbacks(32);

    if (!frameReady)
    {
        frameReady = updateCaches();
        return;
    }

    numFramesQueued = ffmpegRecorder.m_Frames.size();

    if (numFramesQueued > 50)
    {
        waitForFrames = true;
        return;
    }
    else if (waitForFrames && numFramesQueued == 0)
        waitForFrames = false;

    if (waitForFrames)
        return;

    float dt = frameReady ? (1.f / recordingFps) : 0.f;

    if (centerZoom)
        zoomCenterWorld = screenToWorld(screenCenter);

    viewTargetAnim.update(dt);
    if (viewTargetAnim.isAnimating() && !viewTargetAnim.getPaused())
    {
        float t = viewTargetAnim.val();
        ofVec2f tween = viewTargetWorld * t + viewStartWorld * (1.f - t);
        offsetDelta.set(tween - currentView.offsetWorld);
    }

    if (cycleTheta)
        currentTheta.setTarget(currentTheta.getTargetValue() + thetaSpeed);

    if (drill)
    {
        drillZoomAnim.update(dt);
        float nextAngle = rotationAngle.getTargetValue() - (drillSpeed * drillZoomAnim.getCurrentSpeed());
        rotationAngle.setTarget(nextAngle);
    }

    calculateViewMatrix();

    currentZoomSmooth.speed = zoomSpeed;

    if (drill && drillZoomAnim.isAnimating() && !drillZoomAnim.getPaused())
        currentZoomSmooth.jumpTo(drillZoomAnim.getCurrentValue());

    bool shouldPreloadZoomIn = false;
    float prevZoom = currentZoomSmooth.getValue();
    bool zoomUpdated = currentZoomSmooth.process(dt);

    if (zoomUpdated)
    {
        ofVec2f screenBeforeZoom = worldToScreen(zoomCenterWorld);
        currentView.scale = std::powf(2.f, static_cast<float>(currentZoomLevel) - currentZoomSmooth.getValue());
        calculateViewMatrix();
        ofVec2f screenAfterZoom = worldToScreen(zoomCenterWorld);

        ofVec2f screenDeltaWorld = screenToWorld(screenAfterZoom) - screenToWorld(screenBeforeZoom);
        currentView.offsetWorld += screenDeltaWorld;
        calculateViewMatrix();

        currentZoomLevel = std::clamp(
            static_cast<int>(std::floor(currentZoomSmooth.getValue())),
            maxZoomLevel, minZoomLevel);

        if (prevZoom > currentZoomSmooth.getValue() && std::fmodf(currentZoomSmooth.getValue(), 1.0f) < 0.5f)
        {
            preloadZoom(currentZoomLevel - 1);
        }

        if (currentZoomLevel != lastZoomLevel)
        {
            float multiplier = std::powf(2.f, (lastZoomLevel - currentZoomLevel));
            currentView.scale = std::powf(2.f, static_cast<float>(currentZoomLevel) - currentZoomSmooth.getValue());

            zoomCenterWorld *= multiplier;
            rotationCenterWorld *= multiplier;
            viewTargetWorld *= multiplier;
            viewStartWorld *= multiplier;
            currentView.offsetWorld *= multiplier;
            offsetDelta *= multiplier;
            tilesetManager.updateScale(multiplier);

            lastZoomLevel = currentZoomLevel;
            calculateViewMatrix();
        }

        currentZoom = static_cast<int>(std::floor(std::powf(2, currentZoomLevel)));
    }

    currentView.offsetWorld += offsetDelta;
    offsetDelta.set(0.f, 0.f);

    if (!drill && sequencePlaying)
    {
        spinSmooth.update(dt);
        spinSmooth.getCurrentValue(); // Needed for getCurrentSpeed to work?...
        if (targetOrientation)
            rotationAngle.setTarget(rotationAngle.getTargetValue() * (1.f - spinSmooth.getCurrentValue()));
        else
            rotationAngle.setTarget(rotationAngle.getTargetValue() + spinSmooth.getCurrentSpeed() * spinSpeed);
    }

    rotationAngle.process(dt);

    if (rotationAngle.getValue() < -180.f)
    {
        rotationAngle.setTarget(rotationAngle.getTargetValue() + 360.f);
        rotationAngle.setValue(rotationAngle.getValue() + 360.f);
    }
    else if (rotationAngle.getValue() > 540.f)
    {
        rotationAngle.setTarget(rotationAngle.getTargetValue() - 360.f);
        rotationAngle.setValue(rotationAngle.getValue() - 360.f);
    }

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

        tilesetManager.updateTheta(currentView.theta);
    }

    currentView.viewWorld.set(
        screenToWorld({0.f + 6.f, 0.f + 6.f}),
        screenToWorld({static_cast<float>(ofGetWidth() - 12), static_cast<float>(ofGetHeight() - 12)}));

    if (frameReady)
    {
        time += dt;

        if (waiting && sequencePlaying)
        {
            if (time >= waitEndTime)
            {
                waiting = false;
                nextStep();
            }
        }
    }

    frameReady = updateCaches();
}

//--------------------------------------------------------------
void ofApp::draw()
{
    float elapsedTime = ofGetElapsedTimef();
    lastFrameTime = elapsedTime;

    for (const auto &[set, tileset] : tilesetManager.tilesets)
        drawTiles(tileset);

    fboFinal.begin();
    ofBackground(0, 0, 0);

    for (const auto &[set, tileset] : tilesetManager.tilesets)
        tileset.fboMain.draw(0, 0);

    ofVec2f cursor(static_cast<float>(ofGetMouseX()), static_cast<float>(ofGetMouseY()));
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

        ofPushStyle();
        ofSetLineWidth(10.f);
        ofNoFill();
        ofSetColor(0, 255, 255);
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

    // ofDrawBitmapStringHighlight(ofToString(frameCount) + ", " + ofToString(time), 0, ofGetHeight());
    std::string tilesetName = "<null>";
    if (currentTileSet != nullptr)
        tilesetName = currentTileSet->name;

    if (recording && showDebug)
    {
        std::string eventName = "<null>";
        if (sequence.size() > 0 && sequenceStep >= 0 && sequenceStep < (int)sequence.size())
            eventName = sequence[sequenceStep]->toString();

        std::string renderInfo = std::format(
            "Frame: {} Time: {:.4f}s  currentZoomSmooth {:.4f}  Tileset: {}  Last event: {}", frameCount, time, currentZoomSmooth.getValue(), tilesetName, eventName);

        ofDrawBitmapStringHighlight(renderInfo, 0, ofGetHeight() - 20);

        if (waitForFrames)
        {
            std::string msg = std::format("  PAUSED FOR RENDER TO CATCH UP ({}) ", numFramesQueued);
            ofDrawBitmapStringHighlight(msg, (ofGetWidth() - (msg.size() * 8)) / 2, ofGetHeight() / 2 - 4);
        }
    }

    fboFinal.end();
    fboFinal.draw(0, 0);

    if (recording && frameReady && !waitForFrames)
    {
        // save fboFinal to video buffer
        fboFinal.readToPixels(framePixels);

        if (framePixels.getWidth() > 0 && framePixels.getHeight() > 0)
        {
            ffmpegRecorder.addSingleFrame(framePixels);

            // ofLog() << "Number of frames in queue: " << ffmpegRecorder.m_Frames.size();

            if (time >= lastPathT + recordPathDt)
            {
                ofVec2f leftGlobal = worldToGlobal(screenToWorld({0.f, ofGetHeight() / 2.f}), currentTileSet);
                ofVec2f rightGlobal = worldToGlobal(screenToWorld({static_cast<float>(ofGetWidth()), ofGetHeight() / 2.f}), currentTileSet);

                std::ofstream outfile;
                outfile.open("path.csv", std::ofstream::out | std::ios_base::app);

                outfile << ofToString(time) << ","
                        << ofToString(leftGlobal.x) << "," << ofToString(leftGlobal.y) << ","
                        << ofToString(rightGlobal.x) << "," << ofToString(rightGlobal.y)
                        << std::endl;

                lastPathT += recordPathDt;
            }

            std::ofstream outfile;
            outfile.open("tween.csv", std::ofstream::out | std::ios_base::app);
            outfile << frameCount << "," << ofToString(time) << ","
                    << ofToString(currentView.offsetWorld.x) << "," << ofToString(currentView.offsetWorld.y) << ","
                    << ofToString(offsetDelta.x) << "," << ofToString(offsetDelta.y) << std::endl;

            frameCount++;
        }
    }

    if (recording)
    {
        ofNoFill();
        ofSetColor(255, 0, 0);
        ofSetLineWidth(6);
        ofDrawRectangle(6, 6, ofGetWidth() - 12, ofGetHeight() - 12);
    }

    if (showDebug && !recording)
    {
        ofSetColor(0, 255, 255);
        int indexPrevPOI = -1;
        ofVec2f prevPoiPos;
        for (size_t i = 0; i < sequence.size(); i++)
        {
            auto poi = dynamic_cast<POI *>(sequence[i].get());
            if (poi == nullptr)
                continue;

            ofVec2f pos = tilesetManager[poi->tileset]->viewTargets[poi->poi];
            pos = worldToScreen(globalToWorld(pos, tilesetManager[poi->tileset]));
            // Draw marker
            ofDrawLine(pos.x - 5, pos.y - 5, pos.x + 5, pos.y + 5);
            ofDrawLine(pos.x + 5, pos.y - 5, pos.x - 5, pos.y + 5);

            if (indexPrevPOI >= 0)
                ofDrawLine(prevPoiPos, pos);

            indexPrevPOI = (int)i;
            prevPoiPos.set(pos);
        }

        std::string coordinates = std::format(
            "Offset: {:.2f}, {:.2f}, ZoomCenter {:.2f}, {:.2f}, rotationAngle {:.2f}",
            currentView.offsetWorld.x, currentView.offsetWorld.y, zoomCenterWorld.x, zoomCenterWorld.y, rotationAngle.getValue());

        ofDrawBitmapStringHighlight(coordinates, 0, ofGetHeight() - 40);

        std::string status = std::format(
            "Zoom: {:.2f} (ZoomLevel {}, Scale: {:.2f}), Theta: {:.2f} \nCache: MAIN {}, SECONDARY {} (cache misses: {}), frameReady {:6}, drill {}, t {:.2f}, Tileset: {}",
            currentZoomSmooth.getValue(), currentZoomLevel, currentView.scale, currentView.theta, cacheMain.size(), cacheSecondary.size(), cacheMisses, frameReady, drill, time, tilesetName);

        ofDrawBitmapStringHighlight(status, 0, ofGetHeight() - 20);

        // std::string viewMatrixStr = std::format(
        //     "┏{:9.3f} {:9.3f} {:9.3f} {:9.3f}┓\n│{:9.3f} {:9.3f} {:9.3f} {:9.3f}│\n│{:9.3f} {:9.3f} {:9.3f} {:9.3f}│\n┗{:9.3f} {:9.3f} {:9.3f} {:9.3f}┛",
        //     viewMatrix._mat[0][0], viewMatrix._mat[1][0], viewMatrix._mat[2][0], viewMatrix._mat[3][0],
        //     viewMatrix._mat[0][1], viewMatrix._mat[1][1], viewMatrix._mat[2][1], viewMatrix._mat[3][1],
        //     viewMatrix._mat[0][2], viewMatrix._mat[1][2], viewMatrix._mat[2][2], viewMatrix._mat[3][2],
        //     viewMatrix._mat[0][3], viewMatrix._mat[1][3], viewMatrix._mat[2][3], viewMatrix._mat[3][3]);

        // ofDrawBitmapStringHighlight(viewMatrixStr, ofGetWidth() - 360, 20);
    }

    if (!hideGui)
        drawGUI();
}

//--------------------------------------------------------------
void ofApp::exit()
{
    ffmpegRecorder.stop();
    loader.stop();
}

//--------------------------------------------------------------
void ofApp::keyPressed(int key)
{
    if (disableKeyboard)
        return;

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
        nextStep();
    else if (key == 's')
    {
        // Skip
        viewTargetAnim.pause();
        drillZoomAnim.pause();
        currentView.offsetWorld = viewTargetWorld;

        rotationAngle.skip();
        currentZoomSmooth.skip();
        currentTheta.skip();
        drill = false;
    }
    else if (key == 'p')
    {
        playSequence();
    }
    else if (key == 'r')
    {
        if (!recording)
        {
            // initialise recording
            if (recordingFileName.value_or("").length() == 0)
                recordingFileName = ofGetTimestampString("%Y-%m-%d_%H:%M:%S");

            if (!recordingFolder.has_value())
                recordingFolder = ofToDataPath("./", true);

            ofLogNotice() << "Recording to " << recordingFolder.value() + recordingFileName.value();

            ffmpegRecorder.setup(true, false, {ofGetWidth(), ofGetHeight()}, recordingFps, 10000);
            ffmpegRecorder.setInputPixelFormat(OF_IMAGE_COLOR);
            ffmpegRecorder.setOutputPath(recordingFolder.value() + recordingFileName.value() + ".mp4");
            ffmpegRecorder.setVideoCodec("libx264");
            ffmpegRecorder.addAdditionalInputArgument("-hide_banner");
            ffmpegRecorder.addAdditionalInputArgument("-loglevel error");
            ffmpegRecorder.startCustomRecord();

            time = 0.f;
        }
        else
        {
            // save
            while (ffmpegRecorder.m_Frames.size())
            {
                ofSleepMillis(100);
            }
            waitForFrames = false;
            ffmpegRecorder.stop();
            time = 0.f;
        }
        recording = !recording;
    }
    else if (key == 'g')
    {
        hideGui = !hideGui;
        disableMouse = false;
    }
}

//--------------------------------------------------------------
void ofApp::mouseMoved(int x, int y)
{
}

//--------------------------------------------------------------
void ofApp::mouseDragged(int x, int y, int button)
{
    if (disableMouse)
        return;
    ofVec2f currentMouse(x, y);

    ofVec2f worldBeforePan = screenToWorld(lastMouse);
    ofVec2f worldAfterPan = screenToWorld(currentMouse);
    ofVec2f worldDelta = worldBeforePan - worldAfterPan;

    currentView.offsetWorld += worldDelta;

    lastMouse.set(currentMouse);
    viewTargetAnim.pause();
    drillZoomAnim.pause();
}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button)
{
    if (disableMouse)
        return;

    lastMouse.set(x, y);
}

//--------------------------------------------------------------
void ofApp::mouseReleased(int x, int y, int button)
{
    if (disableMouse)
        return;
}

//--------------------------------------------------------------
void ofApp::mouseScrolled(int x, int y, float scrollX, float scrollY)
{
    if (disableMouse)
        return;

    drill = false;

    rotationCenterWorld.set(screenToWorld({(float)x, (float)y}));
    rotationAngle.setTarget(rotationAngle.getTargetValue() - scrollX);

    centerZoom = false;
    zoomCenterWorld.set(screenToWorld({(float)x, (float)y}));
    currentZoomSmooth.speed = 2.f;
    currentZoomSmooth.warmUp = 0.f;
    currentZoomSmooth.setTarget(currentZoomSmooth.getTargetValue() - scrollY * 0.015f);
    focusViewTarget = false;
    viewTargetAnim.pause();
    drillZoomAnim.pause();
    sequencePlaying = false;
}

//--------------------------------------------------------------
void ofApp::windowResized(int w, int h)
{
    plane.set(ofGetWidth(), ofGetHeight());
    plane.setPosition(ofGetWidth() / 2, ofGetHeight() / 2, 0);

    for (auto &[set, tileset] : tilesetManager.tilesets)
    {
        tileset.fboA.allocate(w, h, GL_RGBA);
        tileset.fboB.allocate(w, h, GL_RGBA);
        tileset.fboMain.allocate(w, h, GL_RGBA);
        plane.mapTexCoordsFromTexture(tileset.fboMain.getTexture());
    }

    screenRectangle = ofRectangle(0.f, 0.f, static_cast<float>(ofGetWidth()), static_cast<float>(ofGetHeight()));
    screenCenter = screenRectangle.getBottomRight() / 2.f;

    fboFinal.allocate(w, h, GL_RGB);

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

    // 1. Demote from MAIN
    for (auto it = cacheMain.begin(); it != cacheMain.end();)
    {
        TileKey key = it->first;

        Theta t1 = tilesetManager[key.tileset]->t1;
        Theta t2 = tilesetManager[key.tileset]->t2;

        if (
            (key.zoom != currentZoom || key.zoom != currentZoom - 1) ||
            (key.theta != t1 && key.theta != t2) ||
            (!isVisible(key, tilesetManager[key.tileset]->offset)))
        {
            cacheSecondary.put(key, it->second);
            it = cacheMain.erase(it);
        }
        else
            ++it;
    }

    // 2. Check which tiles are needed
    for (const auto &[set, tileset] : tilesetManager.tilesets)
    {
        // 2.1 Skip if tileset is not visible
        ofVec2f tilesetSize = tileset.zoomWorldSizes.at(currentZoom);
        ofRectangle tilesetBounds{{0.f, 0.f}, tilesetSize};
        if (!isVisible(tilesetBounds, tileset.offset))
            continue;

        // 2.2 Check tiles for current and next theta level
        const auto &v1 = tileset.avaliableTiles.at(currentZoom).at(tileset.t1);
        const auto &v2 = tileset.avaliableTiles.at(currentZoom).at(tileset.t2);

        for (const auto &vec : {std::cref(v1), std::cref(v2)})
        {
            for (const TileKey &key : vec.get())
            {
                if (cacheMain.count(key))
                    continue;

                if (!isVisible(key, tileset.offset))
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
        }
    }

    return frameReady;
}

void ofApp::preloadZoom(int level)
{
    if (level < maxZoomLevel || level > minZoomLevel)
        return;

    float multiplier = std::powf(2.f, (level - currentZoomLevel));
    Zoom zoom = static_cast<int>(std::floor(std::powf(2, level)));

    float right = currentView.viewWorld.getRight() / multiplier;
    float left = currentView.viewWorld.getLeft() / multiplier;
    float top = currentView.viewWorld.getTop() / multiplier;
    float bottom = currentView.viewWorld.getBottom() / multiplier;

    for (auto &[set, tileset] : tilesetManager.tilesets)
    {
        // check if tileset in frame first
        ofVec2f tilesetSize = tileset.zoomWorldSizes.at(zoom);
        ofRectangle tilesetBounds{{0.f, 0.f}, tilesetSize};
        if (!isVisible(tilesetBounds, tileset.offset))
            continue;

        int preloadCount = 0;

        const auto &v1 = tileset.avaliableTiles.at(zoom).at(tileset.t1);
        const auto &v2 = tileset.avaliableTiles.at(zoom).at(tileset.t2);

        for (const auto &vec : {std::cref(v1), std::cref(v2)})
        {
            for (const TileKey &key : vec.get())
            {
                if (key.x + tileset.offset.x >= right ||
                    (key.x + key.width + tileset.offset.x) <= left ||
                    key.y + tileset.offset.y >= bottom ||
                    (key.y + key.height + tileset.offset.y) <= top)
                    continue;

                if (!cacheSecondary.contains(key, false))
                {
                    loader.requestLoad(key.filepath, [this, key](const std::string &, ofImage tile)
                                       { cacheSecondary.put(key, tile.getTexture()); });
                    preloadCount++;
                }
            }
        }
    }
}

void ofApp::drawTiles(const TileSet &tileset)
{
    numberVisibleTiles = 0;

    ofNoFill();
    ofSetColor(255);
    ofSetLineWidth(1.f);

    tileset.fboA.begin();
    ofClear(0.0f, 0.0f);
    tileset.fboA.end();

    tileset.fboB.begin();
    ofClear(0.0f, 0.0f);
    tileset.fboB.end();

    for (const auto &[key, tile] : cacheMain)
    {
        if (tileset.name != key.tileset)
            continue;

        // draw thetas on different fbos
        if (key.theta == tileset.t1)
            tileset.fboA.begin();
        else if (key.theta == tileset.t2)
            tileset.fboB.begin();
        else
            continue;

        ofPushMatrix();
        ofMultMatrix(viewMatrix);
        ofSetColor(255);
        tile.draw(key.x + tileset.offset.x, key.y + tileset.offset.y);

        if (showDebug && !recording)
        {
            ofSetColor(255, 0, 0);
            ofSetLineWidth(3.f);
            ofDrawRectangle(key.x + tileset.offset.x, key.y + tileset.offset.y, key.width, key.height);
        }
        ofPopMatrix();

        if (key.theta == tileset.t1)
            tileset.fboA.end();
        else
            tileset.fboB.end();

        numberVisibleTiles++;
    }

    tileset.fboMain.begin();
    ofClear(0.f, 0.f);
    blendShader.begin();
    blendShader.setUniformTexture("texA", tileset.fboA.getTexture(), 1);
    blendShader.setUniformTexture("texB", tileset.fboB.getTexture(), 2);
    blendShader.setUniform1f("alpha", tileset.blendAlpha);
    plane.draw();

    blendShader.end();

    if (showDebug && !recording)
    {
        // Draw poi
        ofSetColor(255, 255, 0);
        for (size_t i = 0; i < tileset.viewTargets.size(); i++)
        {
            ofVec2f vt = tileset.viewTargets[i];
            ofVec2f poi = worldToScreen(globalToWorld(vt, &tileset));
            ofDrawTriangle(poi, poi + ofVec2f(8, 10), poi + ofVec2f(-8, 10));
            poi.x += 20 * sin(TWO_PI * i / tileset.viewTargets.size());
            poi.y += 10 + 20 * cos(TWO_PI * i / tileset.viewTargets.size());
            ofDrawBitmapString(ofToString(i), poi);
        }
    }
    tileset.fboMain.end();
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

void ofApp::setViewTarget(ofVec2f worldCoords, float delayS)
{
    if (currentTileSet == nullptr)
        return;

    viewTargetAnim.pause();
    viewTargetAnim.reset(0.f);

    viewTargetWorld.set(worldCoords);
    viewStartWorld.set(currentView.offsetWorld);

    // set duration based on distance to target
    float dist = worldToGlobal(viewStartWorld, currentTileSet).distance(worldToGlobal(viewTargetWorld, currentTileSet));
    // normalise where corner-to-corner is 1.0
    dist /= sqrtf(2);
    float movementTime = max(minMovingTime, maxMovingTime * dist);
    float spinWaitTime = 1.f;
    float spinTime = (movementTime - spinWaitTime) * 0.75f;

    viewTargetAnim.setDuration(movementTime);
    viewTargetAnim.setRepeatType(AnimRepeat::PLAY_ONCE);
    viewTargetAnim.setCurve(AnimCurve::EASE_IN_EASE_OUT);
    viewTargetAnim.animateToAfterDelay(1.f, delayS);

    spinSmooth.pause();
    spinSmooth.reset(0.f);

    spinSmooth.setDuration(spinTime);
    spinSmooth.setRepeatType(AnimRepeat::PLAY_ONCE);
    spinSmooth.setCurve(AnimCurve::EASE_IN_EASE_OUT);
    spinSmooth.animateToAfterDelay(1.f, spinWaitTime);

    drillZoomAnim.pause();
    drill = false;
    currentZoomSmooth.warmUp = 3.f;
    currentZoomSmooth.speed = .2f;
    currentZoomSmooth.setTarget(flyHeight);

    centerZoom = true;
}

void ofApp::playSequence(int step)
{
    sequencePlaying = true;
    sequenceStep = step - 1;
    nextStep();
}

void ofApp::nextStep()
{
    sequenceStep++;
    if (sequenceStep >= (int)sequence.size())
    {
        sequencePlaying = false;
        // stop recording
        return;
    }

    ofLog() << "Sequence step " + ofToString(sequenceStep);
    sequence[sequenceStep]->accept(*this);
}

void ofApp::visit(POI &ev)
{
    std::string tileset = ev.tileset;

    if (!tilesetManager.contains(tileset))
    {
        ofLogWarning() << tileset << " not loaded in Layout";
        sequencePlaying = false;
        return;
    }
    currentTileSet = tilesetManager[tileset];
    ofVec2f coords = globalToWorld(tilesetManager[tileset]->viewTargets[ev.poi], tilesetManager[tileset]);

    setViewTarget(coords, 0.5f);
}

void ofApp::visit(ParameterChange &ev)
{
    ofLog() << "set parameter " << ev.parameter << " to " << ofToString(ev.value);
    if (ev.parameter == "drillSpeed")
        drillSpeed = ev.value;
    else if (ev.parameter == "zoomSpeed")
        zoomSpeed = ev.value;
    else if (ev.parameter == "spinSpeed")
        spinSpeed = ev.value;
    else if (ev.parameter == "drillDepth")
        drillDepth = ev.value;
    else if (ev.parameter == "drillTime")
        drillTime = ev.value;
    else if (ev.parameter == "flyHeight")
        flyHeight = ev.value;
    else if (ev.parameter == "thetaSpeed")
        thetaSpeed = ev.value;
    else if (ev.parameter == "minMovingTime")
        minMovingTime = ev.value;
    else if (ev.parameter == "maxMovingTime")
        maxMovingTime = ev.value;
    else if (ev.parameter == "orientation")
        targetOrientation = ev.value > 0.f;

    nextStep();
}

void ofApp::visit(Wait &ev)
{
    ofLog() << "Wait for " << ev.value << "s";

    waiting = true;
    waitEndTime = time + ev.value;
}

void ofApp::animationFinished(ofxAnimatableFloat::AnimationEvent &ev)
{
    if (ev.who == &viewTargetAnim)
    {
        drillZoomAnim.setDuration(drillTime);
        drillZoomAnim.setRepeatType(AnimRepeat::PLAY_ONCE);
        drillZoomAnim.setCurve(AnimCurve::EASE_OUT);
        drillZoomAnim.animateFromTo(currentZoomSmooth.getValue(), drillDepth);
        drill = true;
    }
    else if (ev.who == &drillZoomAnim)
    {
        drill = false;
        if (sequencePlaying)
            nextStep();
    }
}

void ofApp::valueReached(SmoothValueLinear::SmoothValueEvent &ev)
{
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

ofVec2f ofApp::globalToWorld(const ofVec2f &coords, const TileSet *tileset) const
{
    /*
        Converts normalised relative coordinates in [0, 1]^2 in tileset to world
        coordinates (including tileset's global position).
    */
    if (tileset == nullptr)
    {
        ofLogWarning() << "ofApp::worldToGlobal tileset in null";
        return coords;
    }

    ofVec2f zoomSize(0.f, 0.f);
    try
    {
        zoomSize.set(tileset->zoomWorldSizes.at(currentZoom));
    }
    catch (const std::exception &e)
    {
        // ofLogError("globalToWorld") << e.what() << " (zoom: " << zoom << ", tileset: " << ")";
        return coords;
    }

    return coords * zoomSize + tileset->offset;
}

ofVec2f ofApp::worldToGlobal(const ofVec2f &coords, const TileSet *tileset) const
{
    /*
        Converts current coordinates to normalised coordinates in [0, 1]^2
        relative to `tileset`.
    */
    if (tileset == nullptr)
    {
        ofLogWarning() << "ofApp::worldToGlobal tileset in null";
        return coords;
    }

    ofVec2f zoomSize(0.f, 0.f);
    try
    {
        zoomSize.set(tileset->zoomWorldSizes.at(currentZoom));
    }
    catch (const std::exception &e)
    {
        // ofLogError("worldToGlobal") << e.what() << " (zoom: " << zoom << ")";
        return coords;
    }

    return (coords - tileset->offset) / zoomSize;
}

void ofApp::jumpTo(const ofVec2f &worldCoords)
{
    currentView.offsetWorld = worldCoords;
    zoomCenterWorld.set(worldCoords);

    viewTargetAnim.pause();
}

void ofApp::jumpTo(const POI &poi)
{
    if (!tilesetManager.contains(poi.tileset))
    {
        ofLogWarning() << poi.tileset << " not loaded in Layout";
        return;
    }
    currentTileSet = tilesetManager[poi.tileset];
    ofVec2f coords = globalToWorld(tilesetManager[poi.tileset]->viewTargets[poi.poi], tilesetManager[poi.tileset]);
    jumpTo(coords);
}

void ofApp::jumpZoom(float zoomLevel)
{
    drill = false;

    currentZoomSmooth.speed = 2.f;
    currentZoomSmooth.warmUp = 0.f;
    currentZoomSmooth.jumpTo(zoomLevel);
    focusViewTarget = false;
}

bool ofApp::saveSequence(const std::string &name)
{
    ofLogNotice() << "ofApp::saveSequence";
    ofxJSON root;

    for (auto &ev : sequence)
    {
        Json::Value obj;
        ev->save(obj);
        root.append(obj);
    }

    fs::path savePath{scanRoot};
    savePath /= name;
    return root.save(savePath, true);
}

bool ofApp::loadSequence(const std::string &name)
{
    ofLogNotice() << "ofApp::loadSequence";
    ofxJSON root;

    fs::path loadPath{scanRoot};
    loadPath /= name;
    bool result = root.open(loadPath);

    if (result)
        sequence.clear();

    for (Json::ArrayIndex i = 0; i < root.size(); ++i)
    {
        std::string type = root[i]["type"].asString();

        if (type == "poi")
        {
            std::string tileset = root[i]["tileset"].asString();
            int poi = root[i]["poi"].asInt();
            auto ev = make_shared<POI>(tileset, poi);

            sequence.emplace_back(ev);
            sequencePoi.emplace_back(ev);
        }
        else if (type == "parameter")
        {
            std::string parameter = root[i]["parameter"].asString();
            float value = root[i]["value"].asFloat();

            sequence.emplace_back(new ParameterChange(parameter, value));
        }
    }
    return result;
}