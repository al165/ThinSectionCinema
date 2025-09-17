#include "TilesetManager.hpp"

void TilesetManager::setRoot(const std::string &root)
{
    tilesetsRoot.assign(root);
    ofDirectory scanDir{root};
    scanDir.listDir();
    auto scanDirFiles = scanDir.getFiles();

    for (const ofFile &scanDirFile : scanDirFiles)
    {
        if (scanDirFile.isDirectory())
            scanListOptions.push_back(scanDirFile.getFileName());
    }
}

void TilesetManager::loadTileList(const std::string &set)
{
    ofLogNotice() << "ofApp::loadTileList()";

    fs::path tileSetPath{tilesetsRoot};
    tileSetPath /= set;
    ofLogNotice() << "Loading " << tileSetPath;

    TileSet tileset;
    tileset.name = set;

    // Look for saved tilelist...
    ofxJSON j;

    if (j.open(fs::path(tileSetPath) /= "tilelist.json"))
    {
        ofLog() << "Loading cached tilelist";
        for (auto &tl : j["thetaLevels"])
            tileset.thetaLevels.push_back(tl.asInt());

        std::sort(tileset.thetaLevels.begin(), tileset.thetaLevels.end());

        ofLog() << " - Loaded thetaLevels";

        for (auto &zl : j["zoomWorldSizes"].getMemberNames())
        {
            ofVec2f size{j["zoomWorldSizes"][zl]["x"].asFloat(), j["zoomWorldSizes"][zl]["y"].asFloat()};
            tileset.zoomWorldSizes[ofToInt(zl)] = size;
        }
        ofLog() << " - Loaded zoomWorldSizes";

        for (auto &zl : j["avaliableTiles"].getMemberNames())
        {
            Zoom zoom = ofToInt(zl);
            for (auto &t : j["avaliableTiles"][zl].getMemberNames())
            {
                int theta = ofToInt(t);
                for (auto &tk : j["avaliableTiles"][zl][t])
                {
                    std::string fp = (tileSetPath / tk["filepath"].asString());
                    TileKey tile{zoom, tk["x"].asInt(), tk["y"].asInt(), tk["width"].asInt(), tk["height"].asInt(), theta, fp, set};

                    tileset.avaliableTiles[zoom][theta].push_back(tile);
                }
            }
        }
        ofLog() << " - Loaded avaliable tiles";
    }
    else
    {
        // Build JSON
        ofDirectory tileDir{tileSetPath};
        tileDir.listDir();
        auto zoomLevelsDirs = tileDir.getFiles();

        if (zoomLevelsDirs.size() == 0)
        {
            ofLogWarning() << tileSetPath << " is empty";
            return;
        }

        j["name"] = set;

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
            j["thetaLevels"].append(theta);
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

            ofDirectory tiles(tileDir.getAbsolutePath() + "/" + ofToString(zoom) + ".0/0.0/");
            tiles.allowExt("jpg");
            tiles.listDir();

            auto tileFiles = tiles.getFiles();
            ofLogNotice() << "  - Found " << ofToString(tileFiles.size()) + " tiles";

            ofVec2f zoomSize(0.f, 0.f);

            for (const Theta t : tileset.thetaLevels)
                tileset.avaliableTiles[zoom][t].reserve(tileFiles.size());

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
                    ofxJSONElement obj;
                    obj["x"] = x;
                    obj["y"] = y;
                    obj["width"] = width;
                    obj["height"] = height;
                    obj["t"] = t;
                    obj["filepath"] = fs::relative(filepath, tileSetPath).c_str();
                    j["avaliableTiles"][ofToString(zoom)][ofToString(t)].append(obj);
                }

                zoomSize.x = std::max(zoomSize.x, static_cast<float>(x + width));
                zoomSize.y = std::max(zoomSize.y, static_cast<float>(y + height));
            }

            tileset.zoomWorldSizes[zoom] = zoomSize;
            j["zoomWorldSizes"][ofToString(zoom)]["x"] = zoomSize.x;
            j["zoomWorldSizes"][ofToString(zoom)]["y"] = zoomSize.y;
        }

        ofLogNotice() << "Saving tilelist JSON";
        j.save(fs::path(tileSetPath) / "tilelist.json", true);
    }

    // load POI list
    if (csv.load(fs::path(tileSetPath) / "poi.csv"))
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

void TilesetManager::addTileSet(const std::string &name, const std::string &position = "", const std::string &alignment = "", const std::string &relativeTo = "")
{
    ofLog() << "ofApp::addTileSet()";
    if (name.size() == 0)
        return;

    loadTileList(name);

    LayoutPosition positionStruct{
        .name = name,
        .relativeTo = relativeTo};

    if (alignment == "center")
        positionStruct.alignment = Alignment::CENTER;
    else if (alignment == "end")
        positionStruct.alignment = Alignment::END;

    if (position == "left")
        positionStruct.position = Position::RIGHT;
    else if (position == "below")
        positionStruct.position = Position::BELOW;
    else if (position == "above")
        positionStruct.position = Position::ABOVE;

    tilesetList.push_back(&tilesets[name]);
    layout.push_back(positionStruct);
}

void TilesetManager::computeLayout(Zoom currentZoom)
{
    TileSet *prevTileset = nullptr;

    for (size_t i = 0; i < layout.size(); i++)
    {
        LayoutPosition pos = layout[i];
        TileSet *thisTileset = &tilesets[pos.name];

        if (i == 0)
        {
            thisTileset->offset = {0.f, 0.f};
            prevTileset = thisTileset;
            continue;
        }

        if (pos.relativeTo.size() > 0 && tilesets.contains(pos.relativeTo))
            prevTileset = &tilesets.at(pos.relativeTo);

        ofVec2f prevOffset = prevTileset->offset;
        ofVec2f prevSize = prevTileset->zoomWorldSizes[currentZoom];
        ofVec2f thisSize = thisTileset->zoomWorldSizes[currentZoom];

        float yOffset = 0.f;
        float xOffset = 0.f;

        if (pos.position == Position::RIGHT || pos.position == Position::LEFT)
        {
            if (pos.alignment == Alignment::CENTER)
                yOffset = (prevSize.y - thisSize.y) / 2;
            else if (pos.alignment == Alignment::END)
                yOffset = prevSize.y - thisSize.y;
        }
        else
        {
            if (pos.alignment == Alignment::CENTER)
                xOffset = (prevSize.x - thisSize.x) / 2;
            else if (pos.alignment == Alignment::END)
                xOffset = prevSize.x - thisSize.x;
        }

        if (pos.position == Position::RIGHT)
        {
            thisTileset->offset.x = prevOffset.x + prevSize.x;
            thisTileset->offset.y = prevOffset.y + yOffset;
        }
        else if (pos.position == Position::BELOW)
        {
            thisTileset->offset.x = prevOffset.x + xOffset;
            thisTileset->offset.y = prevOffset.y + prevSize.y;
        }
        else if (pos.position == Position::LEFT)
        {
            thisTileset->offset.x = prevOffset.x - thisSize.x;
            thisTileset->offset.y = prevOffset.y + yOffset;
        }
        else
        {
            thisTileset->offset.x = prevOffset.x + xOffset;
            thisTileset->offset.y = prevOffset.y - thisSize.y;
        }

        prevTileset = thisTileset;
    }
}

bool TilesetManager::saveLayout(const std::string &name)
{
    ofxJSON root;

    for (const LayoutPosition &layoutStruct : layout)
    {
        Json::Value obj;
        obj["name"] = layoutStruct.name;
        obj["relativeTo"] = layoutStruct.relativeTo;
        switch (layoutStruct.position)
        {
        case Position::BELOW:
            obj["position"] = "below";
            break;
        case Position::LEFT:
            obj["position"] = "left";
            break;
        case Position::ABOVE:
            obj["position"] = "above";
            break;
        case Position::RIGHT:
        default:
            obj["position"] = "right";
            break;
        }
        switch (layoutStruct.alignment)
        {
        case Alignment::CENTER:
            obj["alignment"] = "center";
            break;
        case Alignment::END:
            obj["alignment"] = "end";
            break;
        case Alignment::START:
        default:
            obj["alignment"] = "start";
            break;
        }
        root.append(obj);
    }

    fs::path savePath{tilesetsRoot};
    savePath /= name;
    return root.save(savePath, true);
}

bool TilesetManager::loadLayout(const std::string &name)
{
    ofLogNotice() << "ofApp::loadLayout";
    ofxJSON root;
    fs::path loadPath{tilesetsRoot};
    loadPath /= name;
    bool result = root.open(loadPath);

    if (result)
        layout.clear();

    for (Json::ArrayIndex i = 0; i < root.size(); ++i)
    {
        std::string name = root[i]["name"].asString();
        std::string position = root[i]["position"].asString();
        std::string alignment = root[i]["alignment"].asString();
        std::string relativeTo = root[i]["relativeTo"].asString();

        addTileSet(name, position, alignment, relativeTo);
    }

    return result;
}

void TilesetManager::updateTheta(Theta theta)
{
    for (auto &[set, tileset] : tilesets)
    {
        int thetaIndex = 0;
        for (size_t i = 0; i < tileset.thetaLevels.size(); i++)
        {
            if (tileset.thetaLevels[i] > theta)
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

        tileset.blendAlpha = ofMap(theta, (float)tileset.t1, (float)(t2), 0.f, 1.f);
    }
}

void TilesetManager::updateScale(float multiplier)
{
    for (auto &ts : tilesets)
        ts.second.offset *= multiplier;
}

TileSet *TilesetManager::operator[](const std::string &name)
{
    return &(tilesets[name]);
}

bool TilesetManager::contains(const std::string &name) const
{
    return tilesets.contains(name);
}

size_t TilesetManager::size() const
{
    return tilesets.size();
}