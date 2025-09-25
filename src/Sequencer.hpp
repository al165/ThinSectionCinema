#include "ofMain.h"

struct POI;
struct ParameterChange;
struct WaitSeconds;
struct WaitTheta;
struct Drill;
struct Overview;
struct Jump;
struct Load;
struct End;

class Visitor
{
public:
    virtual void visit(POI &ev) = 0;
    virtual void visit(ParameterChange &ev) = 0;
    virtual void visit(WaitSeconds &ev) = 0;
    virtual void visit(WaitTheta &ev) = 0;
    virtual void visit(Drill &ev) = 0;
    virtual void visit(Overview &ev) = 0;
    virtual void visit(Jump &ev) = 0;
    virtual void visit(Load &ev) = 0;
    virtual void visit(End &ev) = 0;
};

struct SequenceEvent
{
    std::string type;
    float value;

    virtual std::string toString()
    {
        return type + "::" + ofToString(value);
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
        value = static_cast<float>(i);
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

    ParameterChange(const std::string &p, float v) : parameter(p)
    {
        type = "parameter";
        value = v;
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
    WaitSeconds(float v)
    {
        type = "wait-seconds";
        value = v;
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
    WaitTheta(float v)
    {
        type = "wait-theta";
        value = v;
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

    Drill(float v)
    {
        type = "drill";
        value = v;
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

struct Overview : SequenceEvent
{
    std::string tileset;
    Overview(const std::string &ts, float v)
    {
        type = "overview";
        tileset = ts;
        value = v;
    }

    void accept(Visitor &v) override
    {
        v.visit(*this);
    }

    std::string toString() override
    {
        return type + "::" + tileset + "::" + ofToString(value);
    }

    void save(Json::Value &obj)
    {
        obj["type"] = type;
        obj["tileset"] = tileset;
        obj["value"] = value;
    }
};

struct Jump : SequenceEvent
{
    std::string state;

    Jump(const std::string &s, float v)
    {
        type = "jump";
        state = s;
        value = v;
    }

    void accept(Visitor &v) override
    {
        v.visit(*this);
    }

    std::string toString() override
    {
        return type + "::" + state + "::" + ofToString(value);
    }

    void save(Json::Value &obj)
    {
        obj["type"] = type;
        obj["state"] = state;
        obj["value"] = value;
    }
};

struct Load : SequenceEvent
{
    std::string statePath;

    Load(const std::string &s)
    {
        type = "load";
        statePath = s;
    }

    void accept(Visitor &v) override
    {
        v.visit(*this);
    }

    std::string toString() override
    {
        return type + "::" + statePath;
    }

    void save(Json::Value &obj)
    {
        obj["type"] = type;
        obj["statePath"] = statePath;
    }
};

struct End : SequenceEvent
{
    End()
    {
        type = "end";
    }

    void accept(Visitor &v) override
    {
        v.visit(*this);
    }

    void save(Json::Value &obj)
    {
        obj["type"] = type;
    }

    std::string toString() override
    {
        return "<END>";
    }
};