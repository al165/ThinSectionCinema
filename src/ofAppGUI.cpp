#include "ofApp.h"

std::string formatTime(float seconds)
{
    if (seconds < 0)
        seconds = 0; // clamp negative

    int totalMilliseconds = static_cast<int>(std::round(seconds * 1000.0f));
    int minutes = totalMilliseconds / 60000;
    int msRemaining = totalMilliseconds % 60000;

    int secs = msRemaining / 1000;
    int millis = msRemaining % 1000;

    std::ostringstream oss;
    oss << std::setw(2) << std::setfill('0') << minutes
        << ":"
        << std::setw(2) << std::setfill('0') << secs
        << "."
        << std::setw(3) << std::setfill('0') << millis;

    return oss.str();
}

void ofApp::drawGUI()
{
    std::string title = "Thin Section Cinema - " + projectName;
    gui.begin();
    ImGui::SetNextWindowPos(ImGui::GetWindowViewport()->Pos + ImVec2(ofGetWidth() - 340, 10));
    ImGui::SetNextWindowSizeConstraints(ImVec2(320, 500), ImVec2(320, FLT_MAX));
    ImGui::Begin(title.c_str(), NULL, ImGuiWindowFlags_AlwaysAutoResize);

    ImGuiIO &io = ImGui::GetIO();
    disableMouse = io.WantCaptureMouse;
    disableKeyboard = io.WantCaptureKeyboard;

    static size_t selected_event = 0;

    if (ImGui::TreeNode("Layout"))
    {
        ImGui::SeparatorText("Save");

        ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(0.0f, 0.6f, 0.6f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(0.0f, 0.7f, 0.7f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor::HSV(0.0f, 0.8f, 0.8f));
        if (ImGui::Button("Save layout"))
            tilesetManager.saveLayout(layoutPath);
        ImGui::PopStyleColor(3);

        ImGui::SeparatorText("Add");
        static size_t scan_selected_idx = 0;
        static bool cannot_add = true;

        const char *combo_preview_value = tilesetManager.scanListOptions[scan_selected_idx].c_str();
        if (ImGui::BeginCombo("##scan-list", combo_preview_value))
        {
            static ImGuiTextFilter filter;
            if (ImGui::IsWindowAppearing())
            {
                ImGui::SetKeyboardFocusHere();
                filter.Clear();
            }
            filter.Draw("##Filter", -FLT_MIN);

            for (size_t n = 0; n < tilesetManager.scanListOptions.size(); n++)
            {
                const bool is_selected = (scan_selected_idx == n);
                if (filter.PassFilter(tilesetManager.scanListOptions[n].c_str()))
                    if (ImGui::Selectable(tilesetManager.scanListOptions[n].c_str(), is_selected))
                    {
                        scan_selected_idx = n;
                        cannot_add = tilesetManager.contains(tilesetManager.scanListOptions[n]);
                    }
            }
            ImGui::EndCombo();
        }
        static size_t relative_idx = 0;
        static int position_selection_idx = 1;
        static int align_selection_idx = 1;
        const char *positions[] = {"right", "below", "left", "above"};
        const char *alignments[] = {"start", "center", "end"};

        ImGui::SameLine();
        if (cannot_add)
            ImGui::BeginDisabled();

        if (ImGui::Button("Add"))
        {
            ofLog() << "Adding scan " << tilesetManager.scanListOptions[scan_selected_idx];
            tilesetManager.addTileSet(tilesetManager.scanListOptions[scan_selected_idx], "", "", "");
            tilesetManager.computeLayout(currentZoom);
        }

        if (cannot_add)
            ImGui::EndDisabled();

        static size_t selected_layout = 0;
        ImGui::SeparatorText("Edit");
        if (ImGui::BeginListBox("##layout-list", ImVec2(-FLT_MIN, 5 * ImGui::GetTextLineHeightWithSpacing())))
        {
            for (size_t i = 0; i < tilesetManager.layout.size(); i++)
            {
                const bool is_selected = (selected_layout == i);
                LayoutPosition layoutPos = tilesetManager.layout[i];
                if (ImGui::Selectable(layoutPos.name.c_str(), is_selected))
                {
                    selected_layout = i;

                    switch (layoutPos.position)
                    {
                    case Position::RIGHT:
                        position_selection_idx = 0;
                        break;
                    case Position::BELOW:
                        position_selection_idx = 1;
                        break;
                    case Position::LEFT:
                        position_selection_idx = 2;
                        break;
                    case Position::ABOVE:
                        position_selection_idx = 3;
                        break;
                    default:
                        break;
                    }

                    relative_idx = 0;
                    if (layoutPos.relativeTo.size() > 0)
                    {
                        for (size_t n = 0; n < tilesetManager.tilesetList.size(); n++)
                        {
                            if (layoutPos.relativeTo == tilesetManager.tilesetList[n]->name)
                            {
                                relative_idx = n + 1;
                                break;
                            }
                        }
                    }

                    switch (layoutPos.alignment)
                    {
                    case Alignment::START:
                        align_selection_idx = 0;
                        break;
                    case Alignment::CENTER:
                        align_selection_idx = 1;
                        break;
                    case Alignment::END:
                        align_selection_idx = 2;
                        break;
                    default:
                        break;
                    }
                }

                if (ImGui::IsItemActive() && !ImGui::IsItemHovered())
                {
                    size_t n_next = i + (ImGui::GetMouseDragDelta(0).y < 0.f ? -1 : 1);
                    if (n_next >= 0 && n_next < tilesetManager.layout.size())
                    {
                        tilesetManager.layout[i] = tilesetManager.layout[n_next];
                        tilesetManager.layout[n_next] = layoutPos;
                        tilesetManager.computeLayout(currentZoom);
                        ImGui::ResetMouseDragDelta();
                    }
                }
            }
            ImGui::EndListBox();
        }

        if (selected_layout > 0)
        {
            LayoutPosition layout = tilesetManager.layout[selected_layout];
            if (ImGui::Combo("##position", &position_selection_idx, positions, IM_ARRAYSIZE(positions)))
            {
                std::string newPosition = positions[position_selection_idx];
                if (newPosition == "right")
                    tilesetManager.layout[selected_layout].position = Position::RIGHT;
                else if (newPosition == "left")
                    tilesetManager.layout[selected_layout].position = Position::LEFT;
                else if (newPosition == "above")
                    tilesetManager.layout[selected_layout].position = Position::ABOVE;
                else if (newPosition == "below")
                    tilesetManager.layout[selected_layout].position = Position::BELOW;

                tilesetManager.computeLayout(currentZoom);
            }

            std::string relative_to_preview = "<previous>";
            if (relative_idx > 0)
                relative_to_preview = tilesetManager.tilesetList[relative_idx - 1]->name.c_str();

            if (ImGui::BeginCombo("## Relative to", relative_to_preview.c_str()))
            {
                static ImGuiTextFilter filter;
                if (ImGui::IsWindowAppearing())
                {
                    ImGui::SetKeyboardFocusHere();
                    filter.Clear();
                }
                ImGui::SetNextItemShortcut(ImGuiMod_Ctrl | ImGuiKey_F);
                filter.Draw("##FilterRelative", -FLT_MIN);

                for (size_t n = 0; n < tilesetManager.tilesetList.size() + 1; n++)
                {
                    const bool is_selected = (relative_idx == n);
                    if (n == 0)
                    {
                        if (ImGui::Selectable("<previous>", is_selected))
                        {
                            relative_idx = 0;
                            tilesetManager.layout[selected_layout].relativeTo = "";
                            tilesetManager.computeLayout(currentZoom);
                        }
                    }
                    else
                    {
                        std::string relative_name = tilesetManager.tilesetList[n - 1]->name;
                        if (filter.PassFilter(relative_name.c_str()) && relative_name != layout.name)
                            if (ImGui::Selectable(relative_name.c_str(), is_selected))
                            {
                                relative_idx = n;
                                tilesetManager.layout[selected_layout].relativeTo = relative_name;
                                tilesetManager.computeLayout(currentZoom);
                            }
                    }
                }
                ImGui::EndCombo();
            }

            if (ImGui::Combo("##alignment", &align_selection_idx, alignments, IM_ARRAYSIZE(alignments)))
            {
                std::string newAlignment = alignments[align_selection_idx];
                if (newAlignment == "start")
                    tilesetManager.layout[selected_layout].alignment = Alignment::START;
                else if (newAlignment == "center")
                    tilesetManager.layout[selected_layout].alignment = Alignment::CENTER;
                else if (newAlignment == "end")
                    tilesetManager.layout[selected_layout].alignment = Alignment::END;

                tilesetManager.computeLayout(currentZoom);
            }
        }

        ImGui::Dummy({10, 10});
        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Explore"))
    {
        static size_t scan_explore_idx = 0;
        static size_t scan_poi_idx = 0;
        static std::string selectedTilesetName = "";

        static bool focusOnSelect = true;
        static bool zoomOnSelect = true;

        ImGui::SeparatorText("Scan");
        if (ImGui::BeginListBox("##expore-list-box", ImVec2(-FLT_MIN, 5 * ImGui::GetTextLineHeightWithSpacing())))
        {
            for (size_t n = 0; n < tilesetManager.tilesetList.size(); n++)
            {
                const bool is_selected = (scan_explore_idx == n);
                if (ImGui::Selectable(tilesetManager.tilesetList[n]->name.c_str(), is_selected))
                {
                    scan_explore_idx = n;
                    scan_poi_idx = 0;
                    selectedTilesetName = tilesetManager.tilesetList[n]->name;
                }
            }
            ImGui::EndListBox();
        }

        ImGui::Checkbox("Focus on select", &focusOnSelect);
        if (focusOnSelect)
        {
            ImGui::SameLine();
            ImGui::Checkbox("Zoom", &zoomOnSelect);
        }

        if (tilesetManager.tilesetList.size() > 0 && selectedTilesetName.size() > 0)
        {
            ImGui::SeparatorText("POI");
            ImGuiStyle &style = ImGui::GetStyle();
            ImVec2 button_sz(40, 40);
            size_t poiCount = tilesetManager.tilesets[selectedTilesetName]->viewTargets.size();
            float window_visible_x2 = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
            for (size_t n = 0; n < poiCount; n++)
            {
                const bool is_selected = (scan_poi_idx == n);
                if (is_selected)
                    ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(0, 0.6f, 0.6f));

                if (ImGui::Button(ofToString(n).c_str()))
                {
                    scan_poi_idx = n;

                    ofVec2f coords = globalToWorld(tilesetManager.tilesets[selectedTilesetName]->viewTargets[n], tilesetManager.tilesets[selectedTilesetName]);
                    if (focusOnSelect)
                    {
                        jumpTo(coords);
                        if (zoomOnSelect)
                            jumpZoom(2);
                    }
                }

                if (is_selected)
                    ImGui::PopStyleColor();

                float next_item_x2 = ImGui::GetItemRectMax().x + style.ItemSpacing.x + ImGui::GetItemRectSize().x;

                if (n + 1 < poiCount && next_item_x2 < window_visible_x2)
                    ImGui::SameLine();
            }
        }

        if (selectedTilesetName.size() > 0)
        {
            if (ImGui::Button("Add to sequence"))
                selected_event = addSequenceEvent(std::make_shared<POI>(selectedTilesetName, scan_poi_idx), selected_event + 1);
        }

        ImGui::Dummy({10, 10});
        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Parameters"))
    {
        ImGui::SeparatorText("Camera dampening");
        ImGui::SliderFloat("k", &k, 0.f, 10.f);
        ImGui::SameLine();
        if (ImGui::SmallButton("+##cameraK"))
            selected_event = addSequenceEvent(std::make_shared<ParameterChange>("k", k), selected_event + 1);

        ImGui::SliderFloat("d", &d, 0.5f, 1.5f);
        ImGui::SameLine();
        if (ImGui::SmallButton("+##cameraD"))
            selected_event = addSequenceEvent(std::make_shared<ParameterChange>("d", d), selected_event + 1);

        ImGui::SeparatorText("Values");
        ImGui::SliderFloat("zoomSpeed", &zoomSpeed, 0.5f, 8.f);
        ImGui::SameLine();
        if (ImGui::SmallButton("+##zoomSpeed"))
            selected_event = addSequenceEvent(std::make_shared<ParameterChange>("zoomSpeed", zoomSpeed), selected_event + 1);

        ImGui::SliderFloat("drillSpeed", &drillSpeed, 0.f, 1.f);
        ImGui::SameLine();
        if (ImGui::SmallButton("+##drillSpeed"))
            selected_event = addSequenceEvent(std::make_shared<ParameterChange>("drillSpeed", drillSpeed), selected_event + 1);

        ImGui::SliderFloat("drillTime", &drillTime, 1.f, 20.f);
        ImGui::SameLine();
        if (ImGui::SmallButton("+##drillTime"))
            selected_event = addSequenceEvent(std::make_shared<ParameterChange>("drillTime", drillTime), selected_event + 1);

        ImGui::SliderFloat("spinSpeed", &spinSpeed, -0.5f, 0.5f);
        ImGui::SameLine();
        if (ImGui::SmallButton("+##spinSpeed"))
            selected_event = addSequenceEvent(std::make_shared<ParameterChange>("spinSpeed", spinSpeed), selected_event + 1);

        ImGui::SliderFloat("flyHeight", &flyHeight, 2.f, 5.f);
        ImGui::SameLine();
        if (ImGui::SmallButton("+##flyHeight"))
            selected_event = addSequenceEvent(std::make_shared<ParameterChange>("flyHeight", flyHeight), selected_event + 1);

        ImGui::SliderFloat("thetaSpeed", &thetaSpeed, 0.f, 1.f);
        ImGui::SameLine();
        if (ImGui::SmallButton("+##thetaSpeed"))
            selected_event = addSequenceEvent(std::make_shared<ParameterChange>("thetaSpeed", thetaSpeed), selected_event + 1);

        ImGui::SliderFloat("minMoving", &minMovingTime, 1.f, 30.f);
        ImGui::SameLine();
        if (ImGui::SmallButton("+##minMovingTime"))
            selected_event = addSequenceEvent(std::make_shared<ParameterChange>("minMovingTime", minMovingTime), selected_event + 1);

        ImGui::SliderFloat("maxMoving", &maxMovingTime, 1.f, 60.f);
        ImGui::SameLine();
        if (ImGui::SmallButton("+##maxMovingTime"))
            selected_event = addSequenceEvent(std::make_shared<ParameterChange>("maxMovingTime", maxMovingTime), selected_event + 1);

        if (ImGui::Button("Add All"))
        {
            selected_event = addSequenceEvent(std::make_shared<ParameterChange>("k", k), selected_event + 1);
            selected_event = addSequenceEvent(std::make_shared<ParameterChange>("d", d), selected_event + 1);
            selected_event = addSequenceEvent(std::make_shared<ParameterChange>("zoomSpeed", zoomSpeed), selected_event + 1);
            selected_event = addSequenceEvent(std::make_shared<ParameterChange>("drillSpeed", drillSpeed), selected_event + 1);
            selected_event = addSequenceEvent(std::make_shared<ParameterChange>("drillTime", drillTime), selected_event + 1);
            selected_event = addSequenceEvent(std::make_shared<ParameterChange>("spinSpeed", spinSpeed), selected_event + 1);
            selected_event = addSequenceEvent(std::make_shared<ParameterChange>("flyHeight", flyHeight), selected_event + 1);
            selected_event = addSequenceEvent(std::make_shared<ParameterChange>("thetaSpeed", thetaSpeed), selected_event + 1);
            selected_event = addSequenceEvent(std::make_shared<ParameterChange>("minMovingTime", minMovingTime), selected_event + 1);
            selected_event = addSequenceEvent(std::make_shared<ParameterChange>("maxMovingTime", maxMovingTime), selected_event + 1);
            selected_event = addSequenceEvent(std::make_shared<ParameterChange>("orientation", (float)targetOrientation), selected_event + 1);
        }

        ImGui::Dummy({10, 10});
        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Sequence"))
    {
        if (ImGui::TreeNode("Jump to"))
        {
            static float jumpStateTheta = 0.f;
            ImGui::SliderFloat("theta", &jumpStateTheta, 0.f, 180.f);
            ImGui::SameLine();
            if (ImGui::SmallButton("+##jumpTheta"))
                selected_event = addSequenceEvent(std::make_shared<Jump>("theta", jumpStateTheta), selected_event + 1);

            static float jumpStateRotation = 0.f;
            ImGui::SliderFloat("rotation", &jumpStateRotation, 0.f, 360.f);
            ImGui::SameLine();
            if (ImGui::SmallButton("+##jumpRotation"))
                selected_event = addSequenceEvent(std::make_shared<Jump>("rotation", jumpStateRotation), selected_event + 1);

            static float jumpStateZoom = 2.f;
            ImGui::SliderFloat("zoom", &jumpStateZoom, 0.f, 5.f);
            ImGui::SameLine();
            if (ImGui::SmallButton("+##jumpZoom"))
                selected_event = addSequenceEvent(std::make_shared<Jump>("zoom", jumpStateZoom), selected_event + 1);

            ImGui::TreePop();
        }

        static float waitTime = 1.0f;
        static float waitTheta = 0.f;
        static float overviewHeight = 4.f;
        if (ImGui::TreeNode("Action"))
        {
            ImGui::SliderFloat("waitTime", &waitTime, 0.f, 10.f);
            ImGui::SameLine();
            if (ImGui::SmallButton("+##waitTime"))
                selected_event = addSequenceEvent(std::make_shared<WaitSeconds>(waitTime), selected_event + 1);

            ImGui::SliderFloat("waitTheta", &waitTheta, 0.f, 180.f);
            ImGui::SameLine();
            if (ImGui::SmallButton("+##waitTheta"))
                selected_event = addSequenceEvent(std::make_shared<WaitTheta>(waitTheta), selected_event + 1);

            ImGui::SliderFloat("drill", &drillDepth, -1.f, 4.f);
            ImGui::SameLine();
            if (ImGui::SmallButton("+##drillDepth"))
                selected_event = addSequenceEvent(std::make_shared<Drill>(drillDepth), selected_event + 1);

            ImGui::SliderFloat("overview", &overviewHeight, 0.f, 4.f);
            ImGui::SameLine();
            if (ImGui::SmallButton("+##overviewHeight"))
            {
                for (size_t i = selected_event; i >= 0; i--)
                {
                    if (sequence[i]->type != "poi")
                        continue;

                    POI *poi = dynamic_cast<POI *>(sequence[i].get());

                    selected_event = addSequenceEvent(std::make_shared<Overview>(poi->tileset, overviewHeight), selected_event + 1);
                    break;
                }
            }

            if (ImGui::Button("+ Load state file"))
            {
                ofFileDialogResult result = ofSystemLoadDialog("Select a state file JSON to load", false, projectDir);
                if (result.bSuccess)
                    selected_event = addSequenceEvent(std::make_shared<Load>(result.getPath()), selected_event + 1);
            }
            if (ImGui::Button("+ End"))
                selected_event = addSequenceEvent(std::make_shared<End>(), selected_event + 1);

            ImGui::TreePop();
        }

        ImGui::SeparatorText("Edit sequence");

        static bool focusSequence = true;
        static bool focusZoom = false;

        ImVec2 outer_size = ImVec2(0.0f, 16 * ImGui::GetTextLineHeightWithSpacing());
        static ImGuiTableFlags flags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV;

        // if (ImGui::BeginListBox("##Sequence", ImVec2(-FLT_MIN, 16 * ImGui::GetTextLineHeightWithSpacing())))
        if (ImGui::BeginTable("Sequence view", 2, flags, outer_size))
        {
            ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 10.0f);
            ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed);

            ImGuiListClipper clipper;
            clipper.Begin(sequence.size());
            // for (size_t i = 0; i < sequence.size(); i++)
            while (clipper.Step())
            {
                for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++)
                {

                    ImGui::TableNextRow();

                    auto ev = sequence[i];
                    const bool is_selected = ((int)selected_event == i);

                    if (i == sequenceStep)
                        ImGui::TableSetBgColor(1, IM_COL32(255, 0, 0, 120));

                    ImGui::TableSetColumnIndex(0);

                    if (i == sequenceStep)
                        ImGui::Text(">");

                    ImGui::TableSetColumnIndex(1);

                    if (ev->type == "poi")
                        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 255, 255));
                    else if (ev->type == "parameter")
                        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 150));
                    else if (ev->type == "load")
                        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 255, 0, 255));
                    else if (ev->type == "end")
                        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255));
                    else
                        ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 200));

                    if (ImGui::Selectable((ev->toString() + "##event" + ofToString(i)).c_str(), is_selected))
                    {
                        selected_event = i;
                        if (focusSequence)
                            jumpToSequenceStep(i, focusZoom);
                    }

                    ImGui::PopStyleColor();

                    if (ImGui::BeginPopupContextItem())
                    {
                        static float newValue = ev->value;
                        selected_event = i;
                        if (ImGui::IsWindowAppearing())
                        {
                            newValue = ev->value;
                            ImGui::SetKeyboardFocusHere();
                        }

                        if (ev->type == "load")
                        {
                            if (ImGui::Button("Change state file"))
                            {
                                ofFileDialogResult result = ofSystemLoadDialog("Select a state file JSON to load", false, projectDir);
                                if (result.bSuccess)
                                {
                                    auto *loadEv = dynamic_cast<Load *>(ev.get());
                                    if (loadEv)
                                        loadEv->statePath = result.getPath();

                                    ImGui::CloseCurrentPopup();
                                }
                            }
                        }
                        else if (ev->type != "poi")
                        {
                            ImGui::SetNextItemWidth(-FLT_MIN);
                            if (ImGui::InputScalar("##Value", ImGuiDataType_Float, &newValue, NULL, NULL, NULL, ImGuiInputTextFlags_EnterReturnsTrue))
                            {
                                ofLog() << "Update to " << newValue;
                                ev->value = newValue;
                                ImGui::CloseCurrentPopup();
                            }
                        }

                        if (ImGui::Button("Delete"))
                            sequence.erase(sequence.begin() + selected_event);

                        if (ImGui::BeginMenu("Add"))
                        {
                            if (ImGui::MenuItem("waitTime"))
                            {
                                selected_event = addSequenceEvent(std::make_shared<WaitSeconds>(waitTime), selected_event + 1);
                            }
                            if (ImGui::MenuItem("drill"))
                            {
                                selected_event = addSequenceEvent(std::make_shared<Drill>(drillDepth), selected_event + 1);
                            }
                            ImGui::EndMenu();
                        }

                        ImGui::EndPopup();
                    }

                    if (ImGui::IsItemActive() && !ImGui::IsItemHovered())
                    {
                        float dragY = ImGui::GetMouseDragDelta(0).y;
                        int n_next = static_cast<int>(i) + (dragY < 0.f ? -1 : 1);
                        if (n_next >= 0 && n_next < (int)sequence.size())
                        {
                            std::swap(sequence[i], sequence[n_next]);
                            ImGui::ResetMouseDragDelta();
                        }
                    }
                }
            }
            ImGui::EndTable();
            // ImGui::EndListBox();
        }

        ImGui::Checkbox("Focus on select", &focusSequence);
        if (focusSequence)
        {
            ImGui::SameLine();
            ImGui::Checkbox("Zoom", &focusZoom);
        }

        ImGui::SeparatorText("Save");

        ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(0.0f, 0.6f, 0.6f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(0.0f, 0.7f, 0.7f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor::HSV(0.0f, 0.8f, 0.8f));
        if (ImGui::Button("Save sequence"))
            saveSequence(sequencePath);
        ImGui::PopStyleColor(3);

        ImGui::SeparatorText("Playback");
        ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(4.f / 7.f, 0.6f, 0.6f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(4.f / 7.f, 0.7f, 0.7f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor::HSV(4.f / 7.f, 0.8f, 0.8f));
        if (ImGui::Button("Play sequence"))
            playSequence();
        ImGui::PopStyleColor(3);

        ImGui::SameLine();
        if (ImGui::Button("Play from selection"))
            playSequence(selected_event);

        ImGui::Dummy({10, 10});
        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Debug"))
    {
        ImGui::SeparatorText("Progress");
        float progress = 0.f;
        if (sequence.size() > 0)
            progress = (float)sequenceStep / ((float)sequence.size() - 1);
        ImGui::ProgressBar(progress);

        ImGui::SeparatorText("Animated Floats");

        ImGui::Text("viewTargetAnim");
        ImGui::ProgressBar(viewTargetAnim.getPercentDone());

        ImGui::Text("spinSmooth");
        ImGui::ProgressBar(spinSmooth.getPercentDone());

        ImGui::Text("drillZoomAnim");
        ImGui::ProgressBar(drillZoomAnim.getPercentDone());

        ImGui::SeparatorText("Smooth Values");

        if (ImGui::BeginTable("smoothValues", 3))
        {
            ImGui::TableSetupColumn("name");
            ImGui::TableSetupColumn("currentValue");
            ImGui::TableSetupColumn("targetValue");
            ImGui::TableHeadersRow();

            ImGui::TableNextColumn();
            ImGui::Text("currentZoomSmooth");
            ImGui::TableNextColumn();
            ImGui::Text(ofToString(currentZoomSmooth.getValue()).c_str());
            ImGui::TableNextColumn();
            ImGui::Text(ofToString(currentZoomSmooth.getTargetValue()).c_str());

            ImGui::TableNextColumn();
            ImGui::Text("currentTheta");
            ImGui::TableNextColumn();
            ImGui::Text(ofToString(currentTheta.getValue()).c_str());
            ImGui::TableNextColumn();
            ImGui::Text(ofToString(currentTheta.getTargetValue()).c_str());

            ImGui::TableNextColumn();
            ImGui::Text("rotationAngle");
            ImGui::TableNextColumn();
            ImGui::Text(ofToString(rotationAngle.getValue()).c_str());
            ImGui::TableNextColumn();
            ImGui::Text(ofToString(rotationAngle.getTargetValue()).c_str());

            ImGui::EndTable();
        }

        ImGui::SeparatorText("Timings");
        if (ImGui::BeginTable("smoothValues", 2))
        {
            ImGui::TableNextColumn();
            ImGui::Text("time");
            ImGui::TableNextColumn();
            ImGui::Text(formatTime(time).c_str());

            float realTime = ofGetElapsedTimef() - recordStartTime;

            ImGui::TableNextColumn();
            ImGui::Text("real");
            ImGui::TableNextColumn();
            ImGui::Text(formatTime(realTime).c_str());

            float ratio = 1.f;
            if (recording && time > 1.f)
                ratio = time / realTime;

            ImGui::TableNextColumn();
            ImGui::Text("ratio");
            ImGui::TableNextColumn();
            ImGui::Text(ofToString(ratio, 4).c_str());

            ImGui::EndTable();
        }
        // ofRectangle bounds = getLayoutBounds();
        // ImGui::Text("Layout bounds: %.2f, %.2f, %.2f, %.2f", bounds.getX(), bounds.getY(), bounds.getWidth(), bounds.getHeight());

        ImGui::Dummy({10, 10});
        ImGui::TreePop();
    }

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (quitting)
        ImGui::OpenPopup("Quit");

    if (ImGui::BeginPopupModal("Quit", NULL, ImGuiWindowFlags_AlwaysAutoResize))
    {
        if (ImGui::Button("Quit no saving"))
            ofExit();
        ImGui::SameLine();
        ImGui::SetItemDefaultFocus();
        if (ImGui::Button("Save and exit"))
        {
            tilesetManager.saveLayout(layoutPath);
            saveSequence(sequencePath);
            ofExit();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0)))
        {
            quitting = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::End();
    gui.end();
}

void ofApp::drawWelcome()
{
    gui.begin();

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::Begin("Thin Section Cinema", NULL, ImGuiWindowFlags_AlwaysAutoResize);

    static size_t selected_project_idx = 0;
    static std::string openName = "";
    if (ImGui::BeginListBox("##recent-projects", ImVec2(-FLT_MIN, 10 * ImGui::GetTextLineHeightWithSpacing())))
    {
        for (size_t i = 0; i < projectsList.size(); i++)
        {
            bool is_selected = selected_project_idx == i;
            if (ImGui::Selectable(projectsList[i].c_str(), is_selected, ImGuiSelectableFlags_AllowDoubleClick))
            {
                selected_project_idx = i;
                openName = projectsList[i];

                if (ImGui::IsMouseDoubleClicked(0))
                    loadProject(openName);
            }
        }

        ImGui::EndListBox();
    }

    if (!openName.size())
        ImGui::BeginDisabled();

    if (ImGui::Button("Open"))
        loadProject(openName);

    if (!openName.size())
        ImGui::EndDisabled();

    ImGui::SeparatorText("Create new");

    static char newProject[128] = "";
    ImGui::InputText("##project-name", newProject, IM_ARRAYSIZE(newProject));
    ImGui::SameLine();
    if (ImGui::Button("New..."))
        createProject(newProject);

    ImGui::End();
    gui.end();
}