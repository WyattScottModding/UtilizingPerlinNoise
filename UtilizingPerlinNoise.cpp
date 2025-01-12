
#include <iostream>
#include <ostream>

#include "GraphicsEngine.h"


using namespace std;

int main(int argc, char* argv[])
{
    srand(time(NULL));

    GraphicsEngine::Init();
    GraphicsEngine::CreateScreen();
    GraphicsEngine::UpdateScreen();

    while (true)
    {
        if (!GraphicsEngine::DrawScreen())
            break;
    }
    return 0;
}
