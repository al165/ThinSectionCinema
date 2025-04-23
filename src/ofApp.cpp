#include "ofApp.h"

//--------------------------------------------------------------
void ofApp::setup()
{
    ofSetWindowTitle("Tile Viewer");
    ofSetVerticalSync(true);

    tiles = ofDirectory("/home/arran/Desktop/04_47/32.0/0.0/");
    tiles.allowExt("jpg");
    tiles.listDir();

    loadTileList();

    ofResetElapsedTimeCounter();
}

//--------------------------------------------------------------
void ofApp::update()
{
    bool zoomUpdated = currentZoom.process(fpsCounter.getLastFrameFilteredSecs());

    if (zoomUpdated)
    {
        currentZoomLevel = std::floor(currentZoom.getValue());
        currentZoomLevel = std::clamp(currentZoomLevel, maxZoomLevel, minZoomLevel);

        scale = std::powf(2.f, static_cast<float>(currentZoomLevel) - currentZoom.getValue());

        if (currentZoomLevel != lastZoomLevel)
        {
            float multiplier = std::powf(2.f, (lastZoomLevel - currentZoomLevel));
            offset *= multiplier;
            lastZoomLevel = currentZoomLevel;
        }

        loadVisibleTiles();
    }

    fpsCounter.update();
}

//--------------------------------------------------------------
void ofApp::draw()
{
    float elapsedTime = ofGetElapsedTimef();
    float delta = 1000.f * (elapsedTime - lastFrameTime);
    lastFrameTime = elapsedTime;

    ofBackground(0);
    ofPushMatrix();
    ofScale(scale);
    ofTranslate(offset);
    drawTiles();
    ofPopMatrix();

    fpsHistory.push_back(delta);
    while (fpsHistory.size() > historyLength)
        fpsHistory.pop_front();

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

    std::string status = "Zoom: " + ofToString(currentZoom.getValue()) + ", Zoom level: " + ofToString(currentZoomLevel) + ", Scale: " + ofToString(scale) + ", Cache: " + ofToString(tileCache.size()) + ", Visible tiles: " + ofToString(numberVisibleTiles);
    status += "\nOffset: " + ofToString(offset);
    ofDrawBitmapStringHighlight(status, 0, ofGetHeight() - 20);

    fpsCounter.newFrame();
}

//--------------------------------------------------------------
void ofApp::exit()
{
}

//--------------------------------------------------------------
void ofApp::keyPressed(int key)
{
    // if (key == OF_KEY_UP && currentZoom < maxZoom)
    //     currentZoom += 0.1f;
    // if (key == OF_KEY_DOWN && currentZoom > minZoom)
    //     currentZoom -= 0.1f;
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
    loadVisibleTiles();
}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button)
{
    ofLogNotice("ofApp::mousePressed");
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
    // targetZoom -= scrollY * 0.01;
    currentZoom.setTarget(currentZoom.getTargetValue() - scrollY * 0.01);
}

//--------------------------------------------------------------
void ofApp::windowResized(int w, int h)
{
    loadVisibleTiles();
}

//--------------------------------------------------------------
void ofApp::dragEvent(ofDragInfo dragInfo)
{
}

//--------------------------------------------------------------
void ofApp::loadVisibleTiles()
{
    const int windowWidth = ofGetWidth() / scale;
    const int windowHeight = ofGetHeight() / scale;

    static int lastZoomLoaded = -1;

    int zoom = static_cast<int>(std::floor(std::powf(2, currentZoomLevel)));

    if (lastZoomLoaded != zoom)
        tileKeys.clear();

    const int left = -offset.x;
    const int right = -offset.x + windowWidth;
    const int top = -offset.y;
    const int bottom = -offset.y + windowHeight;

    for (const TileKey &key : avaliableTiles[zoom])
    {
        if (key.x >= right || key.x + key.width <= left ||
            key.y >= bottom || key.y + key.height <= top)
        {
            // if (tileKeys.count(key))
            // tileKeys.erase(key);
            continue;
        }

        if (!tileCache.contains(key))
        {
            ofImage tile;
            if (!tile.load(key.filepath))
            {
                ofLogNotice("failed to load " + key.filepath);
                continue;
            }
            tileCache.put(key, tile.getTexture());
        }

        if (!tileKeys.count(key))
            tileKeys.insert(key);
    }

    lastZoomLoaded = zoom;
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

    int zoom = std::floor(std::powf(2.f, currentZoomLevel));

    ofTexture tile;
    for (const TileKey &key : tileKeys)
    {
        if (key.zoom != zoom)
            continue;

        if (key.x >= right || key.x + key.width <= left ||
            key.y >= bottom || key.y + key.height <= top)
            continue;

        if (tileCache.get(key, tile))
        {
            ofSetColor(255);
            tile.draw(key.x, key.y);
            numberVisibleTiles++;
        }
        ofSetColor(255, 0, 0);
        ofDrawRectangle(key.x, key.y, key.width, key.height);
    }
}

void ofApp::loadTileList()
{
    ofLogNotice("ofApp::loadTileList()");
    ofDirectory tileDir{"/home/arran/Desktop/04_47/"};
    tileDir.listDir();
    auto zoomLevelsDirs = tileDir.getFiles();

    for (const ofFile &zoomDir : zoomLevelsDirs)
    {
        if (!zoomDir.isDirectory())
            continue;

        int zoom = ofToInt(zoomDir.getFileName());

        ofLogNotice("- Zoom level: " + ofToString(zoom));

        tiles = ofDirectory("/home/arran/Desktop/04_47/" + ofToString(zoom) + ".0/0.0/");
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

            tiles.emplace_back(zoom, x, y, width, height, tileFiles[i].getAbsolutePath());
        }

        avaliableTiles[zoom] = tiles;
    }
}
