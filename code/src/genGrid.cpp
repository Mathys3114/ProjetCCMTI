#include <iostream>
#include <cmath>
#include <vector>
#include "ImageBase.h"
#include "slic.hpp"
const char * USAGE = "genGrid K imgIN.ppm imgOUT.ppm";


int main(int argc, char * argv[])
{
    if (argc != 4)
    {
        std::cerr << USAGE << std::endl;
    }
    double K = std::atof(argv[1]); // number of superpixel
    char * imINn = argv[2];
    char * imOUTn = argv[3];
    ImageBase imIN;
    imIN.load(imINn);

    double w = imIN.getWidth();
    double h = imIN.getHeight();

    std::vector<Superpixel> grid = genGrid(w, h, K);

    ImageBase imOUT(imIN.getWidth(), imIN.getHeight(), true);

    for (int y = 0; y < imOUT.getHeight(); ++y)
        for (int x = 0; x < imOUT.getWidth(); ++x)
            for (int i = 0; i < 3; i++)
                imOUT[y * 3][x * 3 + i]  = imIN[y*3][x*3 + i];
    for (int i = 0; i < grid.size(); i++)
    {
        int y = grid[i].my;
        int x = grid[i].mx;

        for (int w = 0; w < 3; w++)
            for (int h = 0; h < 3; h++)
                for (int j = 0; j < 3; j++)
                    imOUT[(y - h - 1) * 3][(x - w - 1) * 3 + j] = 0;
    }   
    imOUT.save(imOUTn);
    return 0;
}