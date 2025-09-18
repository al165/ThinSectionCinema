#include "ofMain.h"

struct POI;
struct ParameterChange;
struct WaitSeconds;
struct WaitTheta;
struct Drill;

class Visitor
{
public:
    virtual void visit(POI &ev) = 0;
    virtual void visit(ParameterChange &ev) = 0;
    virtual void visit(WaitSeconds &ev) = 0;
    virtual void visit(WaitTheta &ev) = 0;
    virtual void visit(Drill &ev) = 0;
};

struct SequenceEvent
{
    std::string type;

    virtual std::string toString()
    {
        return type;
    }

    virtual void accept(Visitor &v) = 0;
    virtual void save(Json::Value &obj) = 0;
};

struct POI : public SequenceEvent
{
    std::string tileset;
    size_t poi;

    POI(const std::string &t, size_t i) : tileset(t), poi(i)
    {
        type = "poi";
    }

    std::string toString() override
    {
        return type + "::" + tileset + "::" + ofToString(poi);
    }

    void accept(Visitor &v) override
    {
        v.visit(*this);
    }

    void save(Json::Value &obj)
    {
        obj["type"] = type;
        obj["tileset"] = tileset;
        obj["poi"] = Json::Int(poi);
    }
};

struct ParameterChange : SequenceEvent
{
    std::string parameter;
    float value;

    ParameterChange(const std::string &p, float v) : parameter(p), value(v)
    {
        type = "parameter";
    }

    std::string toString() override
    {
        return type + "::" + parameter + "::" + ofToString(value);
    }

    void accept(Visitor &v) override
    {
        v.visit(*this);
    }

    void save(Json::Value &obj)
    {
        obj["type"] = type;
        obj["parameter"] = parameter;
        obj["value"] = value;
    }
};

struct WaitSeconds : SequenceEvent
{
    float value;
    WaitSeconds(float v) : value(v)
    {
        type = "wait-seconds";
    }

    std::string toString() override
    {
        return type + "::" + ofToString(value);
    }

    void accept(Visitor &v) override
    {
        v.visit(*this);
    }

    void save(Json::Value &obj)
    {
        obj["type"] = type;
        obj["value"] = value;
    }
};

struct WaitTheta : SequenceEvent
{
    float value;
    WaitTheta(float v) : value(v)
    {
        type = "wait-theta";
    }

    std::string toString() override
    {
        return type + "::" + ofToString(value);
    }

    void accept(Visitor &v) override
    {
        v.visit(*this);
    }

    void save(Json::Value &obj)
    {
        obj["type"] = type;
        obj["value"] = value;
    }
};

struct Drill : SequenceEvent
{
    float value;

    Drill(float v) : value(v)
    {
        type = "drill";
    }

    std::string toString() override
    {
        return type + "::" + ofToString(value);
    }

    void accept(Visitor &v) override
    {
        v.visit(*this);
    }

    void save(Json::Value &obj)
    {
        obj["type"] = type;
        obj["value"] = value;
    }
};