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

    ofDirectory scanDir{scanRoot};
    scanDir.listDir();
    auto scanDirFiles = scanDir.getFiles();

    for (const ofFile &scanDirFile : scanDirFiles)
    {
        if (scanDirFile.isDirectory())
            scanListOptions.push_back(scanDirFile.getFileName());
    }

    recordingFps = fps.value_or(30.f);
    minMovingTime = minMove.value_or(8.f);
    maxMovingTime = maxMove.value_or(8.f);
    drillSpeed = drillSpeedConfig.value_or(0.1f);

    plane.set(ofGetWidth(), ofGetHeight());
    plane.setScale(1, -1, 1);
    plane.setPosition(0, ofGetHeight(), 0);
    plane.mapTexCoords(0, 0, ofGetWidth(), ofGetHeight());

    rotationAngle.maxStep = 0.5f;
    rotationAngle.warmUp = 0.f;
    rotationAngle.epsilon = 0.5f;
    currentZoomSmooth.maxStep = 0.5f;
    currentZoomSmooth.warmUp = 3.f;
    currentZoomSmooth.epsilon = 0.01f;
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
    ofAddListener(currentZoomSmooth.valueReached, this, &ofApp::valueReached);

    if (layout.has_value())
        loadLayout(layout.value());

    if (tilesetList.size())
    {
        currentTileSet = tilesetList[0];
        currentView.offsetWorld = worldToGlobal({0.5f, 0.5f}, currentTileSet);
        centerWorld = globalToWorld({0.5f, 0.5f}, currentTileSet);
    }

    loadSequence("sequence.json");
    currentView.offsetWorld.set(centerWorld);
    calculateViewMatrix();
}

void ofApp::drawGUI()
{
    gui.begin();
    // ImGui::SetNextWindowPos(ofVec2f(ofGetWindowPositionX(), ofGetWindowPositionY()), ImGuiCond_Once);
    // ImGui::SetNextWindowSize(ofVec2f(ofGetWidth(), ofGetHeight()), ImGuiCond_Once);
    // ImGui::ShowDemoWindow();
    ImGui::SetNextWindowPos(ImGui::GetWindowViewport()->Pos + ImVec2(ofGetWidth() - 400, 10), ImGuiCond_Once);
    ImGui::SetNextWindowSizeConstraints(ImVec2(320, 500), ImVec2(320, FLT_MAX));
    // ImGui::SetNextWindowSize(ImVec2(320, ofGetHeight() * 0.5), ImGuiCond_Once);
    ImGui::Begin("Scan manager", NULL, ImGuiWindowFlags_AlwaysAutoResize);

    if (ImGui::TreeNode("Layout"))
    {
        static size_t scan_selected_idx = 0;
        const char *combo_preview_value = scanListOptions[scan_selected_idx].c_str();
        if (ImGui::BeginCombo("Scan list", combo_preview_value))
        {
            static ImGuiTextFilter filter;
            if (ImGui::IsWindowAppearing())
            {
                ImGui::SetKeyboardFocusHere();
                filter.Clear();
            }
            ImGui::SetNextItemShortcut(ImGuiMod_Ctrl | ImGuiKey_F);
            filter.Draw("##Filter", -FLT_MIN);

            for (size_t n = 0; n < scanListOptions.size(); n++)
            {
                const bool is_selected = (scan_selected_idx == n);
                if (filter.PassFilter(scanListOptions[n].c_str()))
                    if (ImGui::Selectable(scanListOptions[n].c_str(), is_selected))
                        scan_selected_idx = n;
            }
            ImGui::EndCombo();
        }
        static size_t relative_idx = 0;
        static int position_selection_idx = 0;
        const char *positions[] = {"right", "below", "left", "above"};

        if (tilesetList.size() > 0)
        {
            ImGui::Combo("position", &position_selection_idx, positions, IM_ARRAYSIZE(positions));

            std::string relative_to_preview = "<previous>";
            if (relative_idx > 0)
                relative_to_preview = tilesetList[relative_idx - 1]->name.c_str();

            if (ImGui::BeginCombo("Relative list", relative_to_preview.c_str()))
            {
                static ImGuiTextFilter filter;
                if (ImGui::IsWindowAppearing())
                {
                    ImGui::SetKeyboardFocusHere();
                    filter.Clear();
                }
                ImGui::SetNextItemShortcut(ImGuiMod_Ctrl | ImGuiKey_F);
                filter.Draw("##FilterRelative", -FLT_MIN);

                for (size_t n = 0; n < tilesetList.size() + 1; n++)
                {
                    const bool is_selected = (relative_idx == n);
                    if (n == 0)
                    {
                        if (ImGui::Selectable("<previous>", is_selected))
                            relative_idx = n;
                    }
                    else
                    {
                        if (filter.PassFilter(tilesetList[n - 1]->name.c_str()))
                            if (ImGui::Selectable(tilesetList[n - 1]->name.c_str(), is_selected))
                                relative_idx = n;
                    }
                }
                ImGui::EndCombo();
            }
        }

        if (ImGui::Button("Add"))
        {
            if (tilesetList.size() > 0)
            {
                if (relative_idx > 0)
                {
                    ofLog() << "Adding scan " << scanListOptions[scan_selected_idx] << " " << positions[position_selection_idx] << " of " << tilesetList.at(relative_idx - 1)->name;
                    addTileSet(scanListOptions[scan_selected_idx], positions[position_selection_idx], tilesetList.at(relative_idx - 1)->name);
                }
                else
                {
                    ofLog() << "Adding scan " << scanListOptions[scan_selected_idx] << " " << positions[position_selection_idx] << " of previous";
                    addTileSet(scanListOptions[scan_selected_idx], positions[position_selection_idx], "");
                }
            }
            else
            {
                ofLog() << "Adding scan " << scanListOptions[scan_selected_idx];
                addTileSet(scanListOptions[scan_selected_idx], "", "");
            }
        }

        ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(4.f / 7.f, 0.6f, 0.6f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(4.f / 7.f, 0.7f, 0.7f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor::HSV(4.f / 7.f, 0.8f, 0.8f));
        if (ImGui::Button("Load layout"))
            loadLayout("layout.json");
        ImGui::PopStyleColor(3);

        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(0.0f, 0.6f, 0.6f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(0.0f, 0.7f, 0.7f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor::HSV(0.0f, 0.8f, 0.8f));
        if (ImGui::Button("Save layout"))
            saveLayout("layout.json");
        ImGui::PopStyleColor(3);

        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Explore"))
    {
        static size_t scan_explore_idx = 0;
        static size_t scan_poi_idx = 0;
        static std::string selectedTilesetName = "";

        if (ImGui::BeginListBox("##expore-list-box", ImVec2(-FLT_MIN, 5 * ImGui::GetTextLineHeightWithSpacing())))
        {
            for (size_t n = 0; n < tilesetList.size(); n++)
            {
                const bool is_selected = (scan_explore_idx == n);
                if (ImGui::Selectable(tilesetList[n]->name.c_str(), is_selected))
                {
                    scan_explore_idx = n;
                    scan_poi_idx = 0;
                    selectedTilesetName = tilesetList[n]->name;
                }
            }
            ImGui::EndListBox();
        }

        if (ImGui::BeginTable("##table-poi", 4))
        {
            if (tilesetList.size() > 0 && selectedTilesetName.size() > 0)
            {
                for (size_t n = 0; n < tilesets[selectedTilesetName].viewTargets.size(); n++)
                {
                    const bool is_selected = (scan_poi_idx == n);
                    ImGui::TableNextColumn();
                    if (ImGui::Selectable(ofToString(n).c_str(), is_selected))
                    {
                        scan_poi_idx = n;

                        ofVec2f coords = globalToWorld(tilesets[selectedTilesetName].viewTargets[n], &tilesets[selectedTilesetName]);
                        jumpTo(coords);
                        jumpZoom(1.5);
                    }
                }
            }
            ImGui::EndTable();
        }

        if (selectedTilesetName.size() > 0)
        {
            if (ImGui::Button("Add to sequence"))
                sequence.emplace_back(selectedTilesetName, scan_poi_idx);
        }

        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Sequence"))
    {
        static size_t selected_event = 0;
        if (ImGui::BeginListBox("##Sequence", ImVec2(-FLT_MIN, 7 * ImGui::GetTextLineHeightWithSpacing())))
        {
            ImGui::PushItemFlag(ImGuiItemFlags_AllowDuplicateId, true);
            for (size_t i = 0; i < sequence.size(); i++)
            {
                POI poi = sequence[i];
                std::string eventName = poi.tileset + "::" + ofToString(poi.poi);
                const bool is_selected = (selected_event == i);
                if (ImGui::Selectable(eventName.c_str(), is_selected))
                {
                    selected_event = i;
                    jumpTo(poi);
                    jumpZoom(1.5);
                }

                if (ImGui::IsItemActive() && !ImGui::IsItemHovered())
                {
                    size_t n_next = i + (ImGui::GetMouseDragDelta(0).y < 0.f ? -1 : 1);
                    if (n_next >= 0 && n_next < sequence.size())
                    {
                        sequence[i] = sequence[n_next];
                        sequence[n_next] = poi;
                        ImGui::ResetMouseDragDelta();
                    }
                }
            }
            ImGui::PopItemFlag();
            ImGui::EndListBox();
        }

        if (ImGui::Button("Delete event"))
            sequence.erase(sequence.begin() + selected_event);

        ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(4.f / 7.f, 0.6f, 0.6f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(4.f / 7.f, 0.7f, 0.7f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor::HSV(4.f / 7.f, 0.8f, 0.8f));
        if (ImGui::Button("Load sequence"))
            loadSequence("sequence.json");
        ImGui::PopStyleColor(3);

        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(0.0f, 0.6f, 0.6f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(0.0f, 0.7f, 0.7f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor::HSV(0.0f, 0.8f, 0.8f));
        if (ImGui::Button("Save sequence"))
            saveSequence("sequence.json");
        ImGui::PopStyleColor(3);

        ImGui::TreePop();
    }

    ImGui::End();
    gui.end();
}

//--------------------------------------------------------------
void ofApp::update()
{
    loader.dispatchMainCallbacks(32);

    float dt;
    if (recording)
        dt = frameReady ? (1.f / recordingFps) : 0.f;
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

        if (drill)
        {
            drillZoomAnim.update(dt);
            float nextAngle = rotationAngle.getTargetValue() + (drillSpeed * drillZoomAnim.getCurrentSpeed());
            rotationAngle.setTarget(nextAngle);
        }
    }

    calculateViewMatrix();

    float prevZoom = currentZoomSmooth.getValue();

    if (drill && drillZoomAnim.isAnimating() && !drillZoomAnim.getPaused())
    {
        currentZoomSmooth.jumpTo(drillZoomAnim.getCurrentValue());
    }

    bool zoomUpdated = currentZoomSmooth.process(dt);
    bool zoomingIn = prevZoom > currentZoomSmooth.getValue();

    bool shouldLoadZoomIn = false;
    bool shouldLoadZoomOut = false;

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

        if (currentZoomLevel != lastZoomLevel)
        {
            float multiplier = std::powf(2.f, (lastZoomLevel - currentZoomLevel));
            currentView.scale = std::powf(2.f, static_cast<float>(currentZoomLevel) - currentZoomSmooth.getValue());

            zoomCenterWorld *= multiplier;
            rotationCenterWorld *= multiplier;
            viewTargetWorld *= multiplier;
            viewStartWorld *= multiplier;
            currentView.offsetWorld *= multiplier;
            for (auto &ts : tilesets)
                ts.second.offset *= multiplier;

            lastZoomLevel = currentZoomLevel;
            calculateViewMatrix();
        }

        if (currentView.scale > 0.8 && zoomingIn)
            shouldLoadZoomIn = true;
        else if (currentView.scale < 0.6 && !zoomingIn)
            shouldLoadZoomOut = true;

        currentZoom = static_cast<int>(std::floor(std::powf(2, currentZoomLevel)));
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

        for (auto &[set, tileset] : tilesets)
        {
            int thetaIndex = 0;
            for (size_t i = 0; i < tileset.thetaLevels.size(); i++)
            {
                if (tileset.thetaLevels[i] > currentView.theta)
                    break;
                thetaIndex = i;
            }

            // compute alpha blend
            tileset.t1 = tileset.thetaLevels[thetaIndex];
            tileset.t2 = tileset.thetaLevels[(thetaIndex + 1) % tileset.thetaLevels.size()];
            int t2;
            if (thetaIndex == static_cast<int>(tileset.thetaLevels.size()) - 1)
                t2 = tileset.thetaLevels[0] + 180;
            else
                t2 = tileset.t2;

            tileset.blendAlpha = ofMap(currentView.theta, (float)tileset.t1, (float)(t2), 0.f, 1.f);
        }
    }

    currentView.viewWorld.set(
        screenToWorld({0.f + 6.f, 0.f + 6.f}),
        screenToWorld({static_cast<float>(ofGetWidth() - 12), static_cast<float>(ofGetHeight() - 12)}));

    if (viewTargetAnim.isAnimating() && !viewTargetAnim.getPaused())
    {
        float t = viewTargetAnim.val();
        ofVec2f tween = viewTargetWorld * t + viewStartWorld * (1.f - t);
        offsetDelta.set(tween - currentView.offsetWorld);

        if (!recording)
        {
            // load tiles further down animation path
            View futureView(currentView);
            float t2 = std::sqrtf(t);
            ofVec2f tween2 = viewTargetWorld * t2 + viewStartWorld * (1.f - t2);

            futureView.offsetWorld = screenToWorld(tween2);
        }
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

    for (const auto &[set, tileset] : tilesets)
        drawTiles(tileset);

    fboFinal.begin();
    ofBackground(0, 0, 0);

    for (const auto &[set, tileset] : tilesets)
    {
        tileset.fboMain.draw(0, 0);
    }

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

    fboFinal.end();
    fboFinal.draw(0, 0);

    if (recording && frameReady)
    {
        // save fboFinal to video buffer
        fboFinal.readToPixels(framePixels);

        if (framePixels.getWidth() > 0 && framePixels.getHeight() > 0)
            ffmpegRecorder.addSingleFrame(framePixels);

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
        time += 1.f / recordingFps;
    }

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
            "Offset: {:.2f}, {:.2f}, ZoomCenter {:.2f}, {:.2f}, rotationAngle {:.2f}",
            currentView.offsetWorld.x, currentView.offsetWorld.y, zoomCenterWorld.x, zoomCenterWorld.y, rotationAngle.getValue());

        ofDrawBitmapStringHighlight(coordinates, 0, ofGetHeight() - 40);

        std::string tilesetName = "<null>";
        if (currentTileSet != nullptr)
            tilesetName = currentTileSet->name;

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

    fpsCounter.newFrame();

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
    else if (key == 'n')
    {
        // if (nextTileSet == nullptr)
        //     return;

        // currentTileSet = nextTileSet;
        tileset_index++;
        try
        {
            currentTileSet = tilesetList[tileset_index];
            ofVec2f t(currentTileSet->viewTargets.back());
            currentTileSet->viewTargets.pop_back();
            setViewTarget(globalToWorld(t, currentTileSet));
        }
        catch (const std::exception &e)
        {
            ofLogWarning() << "no next tileset";
            return;
        }
    }
    else if (key == ' ')
    {
        if (currentTileSet == nullptr)
            return;

        drill = false;

        if (currentTileSet->viewTargets.size())
        {
            ofVec2f t(currentTileSet->viewTargets.back());
            currentTileSet->viewTargets.pop_back();
            setViewTarget(globalToWorld(t, currentTileSet));
        }
    }
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
            ffmpegRecorder.stop();
            time = 0.f;
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
    drillZoomAnim.pause();
}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button)
{
    lastMouse.set(x, y);
}

//--------------------------------------------------------------
void ofApp::mouseReleased(int x, int y, int button)
{
}

//--------------------------------------------------------------
void ofApp::mouseScrolled(int x, int y, float scrollX, float scrollY)
{
    drill = false;

    rotationCenterWorld.set(screenToWorld({(float)x, (float)y}));
    rotationAngle.setTarget(rotationAngle.getTargetValue() - scrollX);

    zoomCenterWorld.set(screenToWorld({(float)x, (float)y}));
    currentZoomSmooth.speed = 2.f;
    currentZoomSmooth.warmUp = 0.f;
    currentZoomSmooth.setTarget(currentZoomSmooth.getTargetValue() - scrollY * 0.015f);
    focusViewTarget = false;
    viewTargetAnim.pause();
    drillZoomAnim.pause();
}

//--------------------------------------------------------------
void ofApp::windowResized(int w, int h)
{
    plane.set(ofGetWidth(), ofGetHeight());
    plane.setPosition(ofGetWidth() / 2, ofGetHeight() / 2, 0);

    for (auto &[set, tileset] : tilesets)
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

        Theta t1 = tilesets[key.tileset].t1;
        Theta t2 = tilesets[key.tileset].t2;

        if (
            (key.zoom != currentZoom) ||
            (key.theta != t1 && key.theta != t2) ||
            (!isVisible(key, tilesets[key.tileset].offset)))
        {
            cacheSecondary.put(key, it->second);
            it = cacheMain.erase(it);
        }
        else
            ++it;
    }

    // 2. Check which tiles are needed
    for (const auto &[set, tileset] : tilesets)
    {
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

    for (auto &[set, tileset] : tilesets)
    {
        // check if tileset in frame first
        // ofVec2f tilesetBounds = tileset.zoomWorldSizes[zoom];

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

    if (showDebug)
    {
        // Draw poi
        ofSetColor(255, 255, 0);
        for (size_t i = 0; i < tileset.viewTargets.size(); i++)
        // for (auto &vt : tileset.viewTargets)
        {
            ofVec2f vt = tileset.viewTargets[i];
            ofVec2f poi = worldToScreen(globalToWorld(vt, &tileset));
            ofDrawTriangle(poi, poi + ofVec2f(8, 10), poi + ofVec2f(-8, 10));
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

void ofApp::loadTileList(const std::string &set)
{
    ofLogNotice() << "ofApp::loadTileList()";

    fs::path tileSetPath{scanRoot};
    tileSetPath /= set;
    ofLogNotice() << "Loading " << tileSetPath;

    ofDirectory tileDir{tileSetPath};
    tileDir.listDir();
    auto zoomLevelsDirs = tileDir.getFiles();

    if (zoomLevelsDirs.size() == 0)
    {
        ofLogWarning() << tileSetPath << " is empty";
        return;
    }

    TileSet tileset;
    tileset.name = set;

    // get list of theta levels (as ints)
    auto thetas = ofDirectory(tileDir.getAbsolutePath() + "/2.0/");
    auto thetasDirs = thetas.getFiles();
    tileset.thetaLevels.clear();

    for (const ofFile &thetaDir : thetasDirs)
    {
        if (!thetaDir.isDirectory())
            continue;

        int theta = ofToInt(thetaDir.getFileName());
        tileset.thetaLevels.push_back(theta);
    }

    std::sort(tileset.thetaLevels.begin(), tileset.thetaLevels.end());
    ofLogNotice() << "- Theta levels:";
    std::string levels = "";
    for (const Theta t : tileset.thetaLevels)
        levels += " " + ofToString(t);

    ofLogNotice() << levels;

    // Get all the tiles
    for (const ofFile &zoomDir : zoomLevelsDirs)
    {
        if (!zoomDir.isDirectory())
            continue;

        Zoom zoom = ofToInt(zoomDir.getFileName());

        ofLogNotice() << "- Zoom level " << zoom;

        tiles = ofDirectory(tileDir.getAbsolutePath() + "/" + ofToString(zoom) + ".0/0.0/");
        tiles.allowExt("jpg");
        tiles.listDir();

        auto tileFiles = tiles.getFiles();
        ofLogNotice() << "  - Found " << ofToString(tileFiles.size()) + " tiles";

        ofVec2f zoomSize(0.f, 0.f);

        for (const Theta t : tileset.thetaLevels)
        {
            tileset.avaliableTiles[zoom][t].reserve(tileFiles.size());
        }

        for (size_t i = 0; i < tileFiles.size(); i++)
        {
            std::string filename = tileFiles[i].getFileName();
            auto basename = ofSplitString(filename, ".");
            auto components = ofSplitString(basename[0], "x", true, true);

            int x = ofToInt(components[0]);
            int y = ofToInt(components[1]);
            int width = ofToInt(components[2]);
            int height = ofToInt(components[3]);

            for (const Theta t : tileset.thetaLevels)
            {
                std::string filepath = tileDir.getAbsolutePath() + "/" + ofToString(zoom) + ".0/" + ofToString(t) + ".0/" + filename;
                tileset.avaliableTiles[zoom][t].emplace_back(zoom, x, y, width, height, t, filepath, set);
            }

            zoomSize.x = std::max(zoomSize.x, static_cast<float>(x + width));
            zoomSize.y = std::max(zoomSize.y, static_cast<float>(y + height));
        }

        tileset.zoomWorldSizes[zoom] = zoomSize;
    }

    // load POI list
    if (csv.load(tileSetPath / "poi.csv"))
    {
        ofLogNotice() << "Found poi.csv";
        bool skip_header = true;
        for (auto row : csv)
        {
            if (skip_header)
            {
                skip_header = false;
                continue;
            }

            float x = row.getFloat(1);
            float y = row.getFloat(2);

            tileset.viewTargets.emplace_back(x, y);
        }
    }
    else
        ofLogNotice() << "poi.csv not found.";

    tileset.t1 = tileset.thetaLevels[0];
    tileset.t2 = tileset.thetaLevels[1];
    tilesets[set] = tileset;
}

void ofApp::addTileSet(const std::string &name, const std::string &position = "", const std::string &relativeToo = "")
{
    ofLog() << "ofApp::addTileSet()";
    if (name.size() == 0)
        return;

    loadTileList(name);

    LayoutPosition positionStruct;
    positionStruct.name = name;

    if (position.size() > 0 && tilesetList.size() > 0)
    {
        TileSet *prevTileset = tilesetList[tilesetList.size() - 1];
        if (relativeToo.size() > 0 && tilesets.contains(relativeToo))
            prevTileset = &tilesets.at(relativeToo);

        positionStruct.relativeTo = prevTileset->name;

        ofVec2f prevSize = prevTileset->zoomWorldSizes[currentZoom];
        ofVec2f prevOffset = prevTileset->offset;

        TileSet *thisTileset = &tilesets[name];
        ofVec2f thisSize = thisTileset->zoomWorldSizes[currentZoom];

        if (position == "right")
        {
            thisTileset->offset.x = prevOffset.x + prevSize.x;
            thisTileset->offset.y = prevOffset.y;
            positionStruct.position = Position::RIGHT;
        }
        else if (position == "below")
        {
            thisTileset->offset.x = prevOffset.x;
            thisTileset->offset.y = prevOffset.y + prevSize.y;
            positionStruct.position = Position::BELOW;
        }
        else if (position == "left")
        {
            thisTileset->offset.x = prevOffset.x - thisSize.x;
            thisTileset->offset.y = prevOffset.y;
            positionStruct.position = Position::LEFT;
        }
        else
        {
            thisTileset->offset.x = prevOffset.x;
            thisTileset->offset.y = prevOffset.y - thisSize.y;
            positionStruct.position = Position::ABOVE;
        }
    }

    tilesetList.push_back(&tilesets[name]);
    layout.push_back(positionStruct);

    // update GUI dropdowns with new tileset
}

void ofApp::setViewTarget(ofVec2f worldCoords, float delayS)
{
    if (currentTileSet == nullptr)
        return;

    viewTargetAnim.pause();
    viewTargetAnim.reset(0.f);

    zoomCenterWorld.set(worldCoords);
    viewTargetWorld.set(worldCoords);
    viewStartWorld.set(currentView.offsetWorld);

    // set duration based on distance to target
    float dist = worldToGlobal(viewStartWorld, currentTileSet).distance(worldToGlobal(viewTargetWorld, currentTileSet));
    // normalise where corner-to-corner is 1.0
    dist /= sqrtf(2);
    ofLogNotice() << "Normalised distance to target: " << dist;
    float movementTime = max(minMovingTime, maxMovingTime * dist);
    ofLogNotice() << "Movement time: " << movementTime;

    viewTargetAnim.setDuration(movementTime);
    viewTargetAnim.setRepeatType(AnimRepeat::PLAY_ONCE);
    viewTargetAnim.setCurve(AnimCurve::EASE_IN_EASE_OUT);
    viewTargetAnim.animateToAfterDelay(1.f, delayS);

    drillZoomAnim.pause();
    drill = false;
    currentZoomSmooth.warmUp = 3.f;
    currentZoomSmooth.speed = .1f;
    currentZoomSmooth.setTarget(3.f);
}

void ofApp::playSequence(int step)
{
    sequencePlaying = true;
    sequence_step = step - 1;
    nextStep();
}

void ofApp::nextStep()
{
    sequence_step++;
    if (sequence_step >= sequence.size())
    {
        sequencePlaying = false;
        // stop recording
        return;
    }

    ofLog() << "Sequence step " + ofToString(sequence_step);
    POI poi = sequence[sequence_step];
    if (!tilesets.contains(poi.tileset))
    {
        ofLogWarning() << poi.tileset << " not loaded in Layout";
        sequencePlaying = false;
        return;
    }
    currentTileSet = &tilesets[poi.tileset];
    ofVec2f coords = globalToWorld(tilesets[poi.tileset].viewTargets[poi.poi], &tilesets[poi.tileset]);

    setViewTarget(coords);
}

void ofApp::animationFinished(ofxAnimatableFloat::AnimationEvent &ev)
{
    if (ev.who == &viewTargetAnim)
    {
        // currentZoomSmooth.setTarget(1.3f, false);
        drillZoomAnim.setDuration(8);
        drillZoomAnim.setRepeatType(AnimRepeat::PLAY_ONCE);
        drillZoomAnim.setCurve(AnimCurve::EASE_OUT);
        drillZoomAnim.animateFromTo(currentZoomSmooth.getValue(), 1.3f);
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
    // if (ev.who == &currentZoomSmooth)
    // {
    //     drill = false;
    //     if (sequencePlaying)
    //         nextStep();
    // }
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
    if (!tilesets.contains(poi.tileset))
    {
        ofLogWarning() << poi.tileset << " not loaded in Layout";
        return;
    }
    currentTileSet = &tilesets[poi.tileset];
    ofVec2f coords = globalToWorld(tilesets[poi.tileset].viewTargets[poi.poi], &tilesets[poi.tileset]);
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

bool ofApp::saveLayout(const std::string &name)
{
    ofxJSON root;

    for (const LayoutPosition &layoutStruct : layout)
    {
        Json::Value obj;
        obj["name"] = layoutStruct.name;
        obj["relativeTo"] = layoutStruct.relativeTo;
        switch (layoutStruct.position)
        {
        case Position::RIGHT:
            obj["position"] = "right";
            break;
        case Position::BELOW:
            obj["position"] = "below";
            break;
        case Position::LEFT:
            obj["position"] = "left";
            break;
        case Position::ABOVE:
            obj["position"] = "above";
            break;
        default:
            break;
        }
        root.append(obj);
    }

    return root.save(name, true);
}

bool ofApp::loadLayout(const std::string &name)
{
    ofLogNotice() << "ofApp::loadLayout";
    ofxJSON root;
    bool result = root.open(name);

    if (result)
        layout.clear();

    for (Json::ArrayIndex i = 0; i < root.size(); ++i)
    {
        std::string name = root[i]["name"].asString();
        std::string position = root[i]["position"].asString();
        std::string relativeTo = root[i]["relativeTo"].asString();

        addTileSet(name, position, relativeTo);
    }
    return result;
}

bool ofApp::saveSequence(const std::string &name)
{
    ofLogNotice() << "ofApp::saveSequence";
    ofxJSON root;

    for (const POI &poi : sequence)
    {
        Json::Value obj;
        obj["type"] = "poi";
        obj["tileset"] = poi.tileset;
        obj["poi"] = Json::Int(poi.poi);

        root.append(obj);
    }

    return root.save(name, true);
}

bool ofApp::loadSequence(const std::string &name)
{
    ofLogNotice() << "ofApp::loadSequence";
    ofxJSON root;
    bool result = root.open(name);

    if (result)
        sequence.clear();

    for (Json::ArrayIndex i = 0; i < root.size(); ++i)
    {
        std::string type = root[i]["type"].asString();

        if (type == "poi")
        {
            std::string tileset = root[i]["tileset"].asString();
            int poi = root[i]["poi"].asInt();

            sequence.emplace_back(tileset, poi);
        }
    }
    return result;
}