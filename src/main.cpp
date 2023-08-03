#include <iostream>
#include "cubemap_bsp.h"
int main(int argc, char** argv) {

    for(int i = 1; i < argc; i++)
    {
        auto fileName = std::string(argv[i]);
        if(!fileName.ends_with(".bsp"))
        {
            std::cout << "CRITICAL ERROR: File \"" + fileName + "\" is not a BSP";
            return 1;
        }
        CCubeMapBSP bsp{"/home/trico/CLionProjects/LuxCubemapProcessor/ep2_outland_05.bsp"};

        if(!bsp.Available())
        {
            std::cout << "CRITICAL ERROR: BSP File \"" + fileName + "\" is corrupt and could not be loaded." << std::endl;
            return 1;
        }
        std::cout << "BSP File \"" + fileName + "\" processed." << std::endl;
    }

    return 0;
}
