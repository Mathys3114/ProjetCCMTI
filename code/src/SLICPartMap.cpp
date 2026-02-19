#include <iostream>
#include <cmath>
#include <vector>
#include "ImageBase.h"
#include "slic.hpp"
const char * USAGE = "SLICPartMap K m g imgIN.ppm imgOUT.ppm";


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
        bool onBorder = false;
        if (i >= w && l[i] != l[i - w])
            onBorder = true;
        else if (i % ((int)w) > 0 && l[i] != l[i-1])
            onBorder = true;
        else if (i % ((int)w) < w - 1 && l[i] != l[i+1])
            onBorder = true;
        else if (w*(h-2) > i && l[i+w] != l[i])
            onBorder = true;
        if (onBorder)
        {
            imOUT.getData()[i*3] = 255;
            imOUT.getData()[i*3+1] = 255;
            imOUT.getData()[i*3+2] = 255;
        }
        else
            for (int j = 0; j < 3; j++)
                imOUT.getData()[i*3+j] = imIN.getData()[i*3+j];
    }
    imOUT.save(imOUTn);
    return 0;
}