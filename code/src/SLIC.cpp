#include <iostream>
#include <cmath>
#include <vector>
#include "ImageBase.h"
#include "slic.hpp"
const char * USAGE = "SLIC K imgIN.ppm";


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

    return 0;
}