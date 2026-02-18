#include <iostream>
#include <cmath>
#include <vector>
#include "ImageBase.h"
#include "slic.hpp"
const char * USAGE = "SLIC K m g imgIN.ppm imgOUT.ppm";


int main(int argc, char * argv[])
{
    if (argc != 6)
    {
        std::cerr << USAGE << std::endl;
        return 1;
    }
    double K = std::atof(argv[1]); // number of superpixel
    double m = std::atof(argv[2]); // scaling spatial/color
    double g = std::atof(argv[3]); // search window
    char * imINn = argv[4];
    char * imOUTn = argv[5];
    ImageBase imIN;
    imIN.load(imINn);

    double w = imIN.getWidth();
    double h = imIN.getHeight();

    double N = w * h;
    double S = std::sqrt(N / K); // step size

    std::vector<Superpixel> grid = genGrid(w, h, K);
    grid = nudgeAlongGradient(imIN, grid, g);
    std::vector<int> l = slic(imIN, S, grid, m);


    ImageBase imOUT(imIN.getWidth(), imIN.getHeight(), true);

    for (size_t i = 0; i < l.size(); i++)
    {
        imOUT.getData()[i*3] = grid[l[i]].mr;
        imOUT.getData()[i*3+1] = grid[l[i]].mg;
        imOUT.getData()[i*3+2] = grid[l[i]].mb;
    }
    imOUT.save(imOUTn);
    return 0;
}