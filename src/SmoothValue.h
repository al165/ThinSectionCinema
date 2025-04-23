#include <math.h>

#define TWO_PI 6.28318530717958647693

class SmoothValue
{
public:
    SmoothValue(float smoothTimeMS, float frameRate) : z(0.f)
    {
        a = std::exp(-TWO_PI / (smoothTimeMS * 0.001 * frameRate));
        b = 1.f - a;
    }

    float process(float input)
    {
        z = (input * b) + (z * a);
        return z;
    }

private:
    float a, b, z;
};

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

private:
    float currentValue, targetValue;
    float minimum, maximum;
    float speed;
    bool needsProcessing;
};