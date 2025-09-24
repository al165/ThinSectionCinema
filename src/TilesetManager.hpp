#include "ofMain.h"
#include "ofxJSON.h"
#include "ofxCsv.h"

#include <filesystem>
namespace fs = std::filesystem;

#include "TilesetProperties.h"

class TilesetManager
{
public:
    void setRoot(const std::string &root);
    void loadTileList(const std::string &set);
    void addTileSet(
        const std::string &name,
        const std::string &position,
        const std::string &alignment,
        const std::string &relativeTo);
    void computeLayout(Zoom currentZoom);
    bool saveLayout(const std::string &name);
    bool loadLayout(const std::string &name);

    void updateTheta(Theta theta);
    void updateScale(float multiplier);

    std::shared_ptr<TileSet> getTilsetAtWorldCoords(const ofVec2f &coords, Zoom currentZoom) const;

    std::shared_ptr<TileSet> operator[](const std::string &name);
    bool contains(const std::string &name) const;
    size_t size() const;

    ofxCsv csv;
    fs::path tilesetsRoot;
    std::vector<std::string> scanListOptions;

    std::unordered_map<std::string, std::shared_ptr<TileSet>> tilesets;
    std::vector<std::shared_ptr<TileSet>> tilesetList;
    size_t tileset_index = 0;

    std::vector<LayoutPosition> layout;
};
