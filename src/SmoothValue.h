
#include <math.h>
#include "ofMain.h"

class SmoothValueLinear
{
public:
    SmoothValueLinear(
        float s,
        float startValue = 0.f,
        float min = 0.f,
        float max = 1.f) : speed(s),
                           currentValue(startValue),
                           targetValue(startValue),
                           minimum(min),
                           maximum(max),
                           needsProcessing(true)
    {
    }

    void setTarget(float target)
    {
        targetValue = std::clamp(target, minimum, maximum);
        needsProcessing = true;
    }

    void setValue(float value)
    {
        currentValue = std::clamp(value, minimum, maximum);
        needsProcessing = true;
    }

    void jumpTo(float value)
    {
        currentValue = std::clamp(value, minimum, maximum);
        targetValue = currentValue;
        needsProcessing = true;
    }

    float getValue() const
    {
        return currentValue;
    }

    float getTargetValue() const
    {
        return targetValue;
    }

    bool process(float deltaS)
    {
        if (!needsProcessing)
            return false;

        float diff = targetValue - currentValue;

        if (std::fabs(diff) < 0.001f)
        {
            currentValue = targetValue;
            needsProcessing = false;
        }
        else
        {
            float step = speed * deltaS;
            currentValue += diff * std::min(step, 1.f);
        }
        currentValue = std::clamp(currentValue, minimum, maximum);
        return true;
    }

    float speed;

private:
    float currentValue, targetValue;
    float minimum, maximum;
    bool needsProcessing;
};

class SmoothVec2Linear
{
public:
    SmoothVec2Linear(
        float s,
        ofVec2f startValue) : speed(s),
                              currentValue(startValue),
                              targetValue(startValue),
                              needsProcessing(true)
    {
    }

    void setTarget(ofVec2f target)
    {
        targetValue.set(target);
        needsProcessing = true;
    }

    void setValue(ofVec2f value)
    {
        currentValue.set(value);
        needsProcessing = true;
    }

    void jumpTo(float value)
    {
        currentValue.set(value);
        targetValue.set(currentValue);
        needsProcessing = true;
    }

    ofVec2f getValue() const
    {
        return currentValue;
    }

    ofVec2f getTargetValue() const
    {
        return targetValue;
    }

    bool process(float deltaS)
    {
        if (!needsProcessing)
            return false;

        ofVec2f diff = targetValue - currentValue;

        if (diff.lengthSquared() < 0.01f)
        {
            currentValue = targetValue;
            needsProcessing = false;
        }
        else
        {
            float step = speed * deltaS;
            currentValue += diff * std::min(step, 1.f);
        }
        return true;
    }

    float speed;

private:
    ofVec2f currentValue, targetValue;
    bool needsProcessing;
};