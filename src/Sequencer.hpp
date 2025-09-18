#include "ofMain.h"

struct POI;
struct ParameterChange;
struct Wait;

class Visitor
{
public:
    virtual void visit(POI &ev) = 0;
    virtual void visit(ParameterChange &ev) = 0;
    virtual void visit(Wait &ev) = 0;
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
    POI(const std::string &t, size_t i) : tileset(t), poi(i) { type = "poi"; }
    std::string toString() override
    {
        return type + "::" + tileset + "::" + ofToString(poi);
    }
    void accept(Visitor &v) override { v.visit(*this); }
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
    ParameterChange(const std::string &p, float v) : parameter(p), value(v) { type = "parameter"; }
    std::string toString() override
    {
        return type + "::" + parameter + "::" + ofToString(value);
    }
    void accept(Visitor &v) override { v.visit(*this); }
    void save(Json::Value &obj)
    {
        obj["type"] = type;
        obj["parameter"] = parameter;
        obj["value"] = value;
    }
};

struct Wait : SequenceEvent
{
    float value;
    Wait(float v) : value(v) { type = "wait"; }
    std::string toString() override
    {
        return type + "::" + ofToString(value);
    }
    void accept(Visitor &v) override { v.visit(*this); }
    void save(Json::Value &obj)
    {
        obj["type"] = type;
        obj["value"] = value;
    }
};