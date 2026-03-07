#include <iostream>
#include <cmath>
#include <vector>
#include "ImageBase.h"
#include "slic.hpp"
#include "compression.hpp"


const char *USAGE = "SlicDecompress source.slic imgOUT.ppm\n decompress a slic image and save it as ppm";

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        std::cerr << USAGE << std::endl;
        return 1;
    }
    char *fileN = argv[1];
    char *imOUTn = argv[2];
    std::vector<Superpixel> grid;
    std::vector<int> l;
    int w = 0, h = 0;
    read(fileN, grid, l, w, h);

    ImageBase imOUT(w, h, true);

    for (size_t i = 0; i < l.size(); i++)
    {
        imOUT.getData()[i * 3] = grid[l[i]].mr;
        imOUT.getData()[i * 3 + 1] = grid[l[i]].mg;
        imOUT.getData()[i * 3 + 2] = grid[l[i]].mb;
    }
    imOUT.save(imOUTn);
    return 0;

    return 0;
}