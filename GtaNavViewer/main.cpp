// main.cpp
#include "ViewerApp.h"

int main(int argc, char** argv)
{
    ViewerApp app;
    if (!app.Init())
        return -1;

    app.Run();
    app.Shutdown();
    return 0;
}
