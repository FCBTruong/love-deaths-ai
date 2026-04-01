#include "core/App.h"

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    App app;
    if (!app.Initialize()) {
        return 1;
    }

    return app.Run();
}