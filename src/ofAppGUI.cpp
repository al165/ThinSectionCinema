#include "ofApp.h"

void ofApp::drawGUI()
{
    gui.begin();
    ImGui::SetNextWindowPos(ImGui::GetWindowViewport()->Pos + ImVec2(ofGetWidth() - 340, 10));
    ImGui::SetNextWindowSizeConstraints(ImVec2(320, 500), ImVec2(320, FLT_MAX));
    ImGui::Begin("Thin Section Cinema", NULL, ImGuiWindowFlags_AlwaysAutoResize);

    ImGuiIO &io = ImGui::GetIO();
    disableMouse = io.WantCaptureMouse;
    disableKeyboard = io.WantCaptureKeyboard;

    if (ImGui::TreeNode("Layout"))
    {
        ImGui::SeparatorText("Save / Load");
        ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(4.f / 7.f, 0.6f, 0.6f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(4.f / 7.f, 0.7f, 0.7f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor::HSV(4.f / 7.f, 0.8f, 0.8f));
        if (ImGui::Button("Load layout"))
            tilesetManager.loadLayout("layout.json");
        ImGui::PopStyleColor(3);

        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(0.0f, 0.6f, 0.6f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(0.0f, 0.7f, 0.7f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor::HSV(0.0f, 0.8f, 0.8f));
        if (ImGui::Button("Save layout"))
            tilesetManager.saveLayout("layout.json");
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

        if (ImGui::BeginTable("##table-poi", 4))
        {
            if (tilesetManager.tilesetList.size() > 0 && selectedTilesetName.size() > 0)
            {
                for (size_t n = 0; n < tilesetManager.tilesets[selectedTilesetName].viewTargets.size(); n++)
                {
                    const bool is_selected = (scan_poi_idx == n);
                    ImGui::TableNextColumn();
                    if (ImGui::Selectable(ofToString(n).c_str(), is_selected))
                    {
                        scan_poi_idx = n;

                        ofVec2f coords = globalToWorld(tilesetManager.tilesets[selectedTilesetName].viewTargets[n], &tilesetManager.tilesets[selectedTilesetName]);
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
            {
                POI ev(selectedTilesetName, scan_poi_idx);
                sequence.emplace_back(new POI(selectedTilesetName, scan_poi_idx));
            }
        }

        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Parameters"))
    {
        ImGui::SeparatorText("Values");
        ImGui::SliderFloat("zoomSpeed", &zoomSpeed, 0.5f, 8.f);
        ImGui::SameLine();
        if (ImGui::SmallButton("+##zoomSpeed"))
            sequence.push_back(std::make_unique<ParameterChange>("zoomSpeed", zoomSpeed));

        ImGui::SliderFloat("drillSpeed", &drillSpeed, 0.f, 1.f);
        ImGui::SameLine();
        if (ImGui::SmallButton("+##drillSpeed"))
            sequence.push_back(std::make_unique<ParameterChange>("drillSpeed", drillSpeed));

        ImGui::SliderFloat("drillTime", &drillTime, 1.f, 20.f);
        ImGui::SameLine();
        if (ImGui::SmallButton("+##drillTime"))
            sequence.push_back(std::make_unique<ParameterChange>("drillTime", drillTime));

        ImGui::SliderFloat("drillDepth", &drillDepth, 0.f, 3.f);
        ImGui::SameLine();
        if (ImGui::SmallButton("+##drillDepth"))
            sequence.push_back(std::make_unique<ParameterChange>("drillDepth", drillDepth));

        ImGui::SliderFloat("spinSpeed", &spinSpeed, -0.5f, 0.5f);
        ImGui::SameLine();
        if (ImGui::SmallButton("+##spinSpeed"))
            sequence.push_back(std::make_unique<ParameterChange>("spinSpeed", spinSpeed));

        ImGui::SliderFloat("flyHeight", &flyHeight, 2.f, 5.f);
        ImGui::SameLine();
        if (ImGui::SmallButton("+##flyHeight"))
            sequence.push_back(std::make_unique<ParameterChange>("flyHeight", flyHeight));

        ImGui::SliderFloat("thetaSpeed", &thetaSpeed, 0.f, 1.f);
        ImGui::SameLine();
        if (ImGui::SmallButton("+##thetaSpeed"))
            sequence.push_back(std::make_unique<ParameterChange>("thetaSpeed", thetaSpeed));

        ImGui::SliderFloat("minMovingTime", &minMovingTime, 1.f, 30.f);
        ImGui::SameLine();
        if (ImGui::SmallButton("+##minMovingTime"))
            sequence.push_back(std::make_unique<ParameterChange>("minMovingTime", minMovingTime));

        ImGui::SliderFloat("maxMovingTime", &maxMovingTime, 1.f, 60.f);
        ImGui::SameLine();
        if (ImGui::SmallButton("+##maxMovingTime"))
            sequence.push_back(std::make_unique<ParameterChange>("maxMovingTime", maxMovingTime));

        ImGui::Checkbox("orientation", &targetOrientation);
        ImGui::SameLine();
        if (ImGui::SmallButton("+##orientation"))
            sequence.push_back(std::make_unique<ParameterChange>("orientation", (float)targetOrientation));

        ImGui::Dummy({10, 10});
        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Sequence"))
    {
        ImGui::SeparatorText("Load / Save");
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

        ImGui::SeparatorText("Add event");
        static float waitTime = 1.0f;
        ImGui::SliderFloat("waitTime", &waitTime, 0.f, 60.f);
        ImGui::SameLine();
        if (ImGui::SmallButton("+##waitTime"))
            sequence.push_back(std::make_unique<Wait>(waitTime));

        ImGui::SeparatorText("Edit sequence");

        static bool focusSequence = true;
        static size_t selected_event = 0;
        if (ImGui::BeginListBox("##Sequence", ImVec2(-FLT_MIN, 16 * ImGui::GetTextLineHeightWithSpacing())))
        {
            ImGui::PushItemFlag(ImGuiItemFlags_AllowDuplicateId, true);
            for (size_t i = 0; i < sequence.size(); i++)
            {
                SequenceEvent *ev = sequence[i].get();
                const bool is_selected = (selected_event == i);
                if ((int)i < sequenceStep)
                    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(200, 200, 200, 255));
                else if ((int)i == sequenceStep)
                    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 0, 0, 255));

                if (ImGui::Selectable(ev->toString().c_str(), is_selected))
                {
                    selected_event = i;
                    auto *a = dynamic_cast<POI *>(ev);
                    if (focusSequence && a)
                    {
                        jumpTo(*a);
                        jumpZoom(1.5);
                    }
                }
                if ((int)i <= sequenceStep)
                    ImGui::PopStyleColor();

                if (ImGui::IsItemActive() && !ImGui::IsItemHovered())
                {
                    size_t n_next = i + (ImGui::GetMouseDragDelta(0).y < 0.f ? -1 : 1);
                    if (n_next >= 0 && n_next < sequence.size())
                    {
                        sequence[i].swap(sequence[n_next]);
                        // sequence[n_next].;
                        ImGui::ResetMouseDragDelta();
                    }
                }
            }
            ImGui::PopItemFlag();
            ImGui::EndListBox();
        }

        if (ImGui::Button("Delete event"))
        {
            sequence.erase(sequence.begin() + selected_event);
            if (selected_event < sequenceStep)
                sequenceStep--;
        }

        ImGui::SameLine();
        ImGui::Checkbox("Focus on select", &focusSequence);

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

        ImGui::Dummy({10, 10});
        ImGui::TreePop();
    }

    ImGui::End();
    gui.end();
}