#include "ofMain.h"
#include "ofApp.h"

//========================================================================
int main()
{
    ofGLFWWindowSettings settings;

    settings.setSize(3840, 2160);
    settings.windowMode = OF_WINDOW;
    settings.setGLVersion(3, 2);

    auto window = ofCreateWindow(settings);

    ofRunApp(window, make_shared<ofApp>());
    ofRunMainLoop();
}
