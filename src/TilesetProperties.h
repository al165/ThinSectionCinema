#pragma once

#include "ofMain.h"
#include <unordered_map>

using Theta = float;
using Zoom = int;

template <class T>
inline void hash_combine(std::size_t &seed, const T &v)
{
    std::hash<T> hasher;
    seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

struct TileKey
{
    int zoom;
    int x;
    int y;
    int width;
    int height;
    int theta;
    std::string filepath;
    std::string tileset;
    size_t hash;

    TileKey(int z, int xx, int yy, int w, int h, int t, std::string path, std::string set) : zoom(z),
                                                                                             x(xx), y(yy),
                                                                                             width(w), height(h),
                                                                                             theta(t),
                                                                                             filepath(std::move(path)),
                                                                                             tileset(std::move(set))
    {
        size_t h1 = std::hash<int>()(zoom);
        size_t h2 = std::hash<int>()(x);
        size_t h3 = std::hash<int>()(y);
        size_t h4 = std::hash<int>()(theta);
        size_t h5 = std::hash<std::string>()(tileset);
        hash = h1;
        hash_combine(hash, h2);
        hash_combine(hash, h3);
        hash_combine(hash, h4);
        hash_combine(hash, h5);
    }

    bool operator==(const TileKey &other) const
    {
        return zoom == other.zoom && theta == other.theta && x == other.x && y == other.y && width == other.width && height == other.height;
    }
};

struct TileSet
{
    std::string name;
    ofFbo fboA, fboB, fboMain;
    ofVec2f offset;
    Theta t1, t2;
    float blendAlpha = 0.f;
    std::vector<Theta> thetaLevels;
    std::vector<ofVec2f> viewTargets;
    std::unordered_map<Zoom, std::unordered_map<Theta, std::vector<TileKey>>> avaliableTiles;
    std::unordered_map<Zoom, ofVec2f> zoomWorldSizes;
    TileSet()
    {
        int fboW = ofGetWidth();
        int fboH = ofGetHeight();
        fboA.allocate(fboW, fboH, GL_RGBA);
        fboB.allocate(fboW, fboH, GL_RGBA);
        fboMain.allocate(fboW, fboH, GL_RGBA);

        t1 = 0;
        t2 = 1;
    }
};

enum Position
{
    RIGHT,
    BELOW,
    LEFT,
    ABOVE
};

enum Alignment
{
    START,
    CENTER,
    END
};

struct LayoutPosition
{
    std::string name = "";
    Position position = Position::RIGHT;
    Alignment alignment = Alignment::START;
    std::string relativeTo = "";
};