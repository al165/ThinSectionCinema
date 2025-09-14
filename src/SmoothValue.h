
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
                           epsilon(0.0001f),
                           currentValue(startValue),
                           targetValue(startValue),
                           minimum(min),
                           maximum(max),
                           needsProcessing(true)
    {
    }

    void setTarget(float target, bool resetElapsed = true)
    {
        targetValue = std::clamp(target, minimum, maximum);
        needsProcessing = true;

        if (resetElapsed)
            elapsedTime = 0.f;
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

    void skip()
    {
        jumpTo(targetValue);
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
        if (!needsProcessing || deltaS < 0.0001f)
            return false;

        float diff = targetValue - currentValue;
        // ofLog() << "SmoothValue targetValue " << targetValue << ", currentValue " << currentValue << ", diff " << diff;

        if (std::fabs(diff) < epsilon)
        {
            currentValue = targetValue;
            needsProcessing = false;
            lastChange = 0.f;

            SmoothValueEvent ev;
            ev.value = currentValue;
            ev.who = this;
            ofNotifyEvent(valueReached, ev, this);
        }
        else
        {
            float step = speed * deltaS;
            float warmUpT;
            if (warmUp < 0.0001f)
                warmUpT = 1.f;
            else
                warmUpT = std::min(elapsedTime, warmUp) / warmUp;
            lastChange = diff * std::min(step, maxStep) * warmUpT;
            currentValue += lastChange;
        }
        currentValue = std::clamp(currentValue, minimum, maximum);

        elapsedTime += deltaS;

        return true;
    }

    float speed;
    float maxStep = 1.f;
    float warmUp = 0.1f;
    float lastChange = 0.f;
    float epsilon;

    struct SmoothValueEvent
    {
        float value;
        SmoothValueLinear *who;
    };
    ofFastEvent<SmoothValueEvent> valueReached;

private:
    float currentValue, targetValue;
    float minimum, maximum;
    bool needsProcessing;
    float elapsedTime = 0.f;
};
