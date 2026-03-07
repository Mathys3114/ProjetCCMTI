#include <iostream>
#include <cmath>
#include <vector>
#include "ImageBase.h"
#include "slic.hpp"
#include "compression.hpp"


const char *USAGE = "SlicCompress K m g imgIN.ppm dest.slic\nCompress an image using slic, and write the result to a file";

int main(int argc, char *argv[])
{
    if (argc != 6)
    {
        std::cerr << USAGE << std::endl;
        return 1;
    }
    double K = std::atof(argv[1]); // number of superpixel
    double m = std::atof(argv[2]); // scaling spatial/color
    double g = std::atof(argv[3]); // search window
    char *imINn = argv[4];
    char *imOUTn = argv[5];
    ImageBase imIN;
    imIN.load(imINn);

    double w = imIN.getWidth();
    double h = imIN.getHeight();

    double N = w * h;
    double S = std::floor(std::sqrt(N / K)); // step size

    std::vector<Superpixel> grid = genGrid(w, h, K);
    grid = nudgeAlongGradient(imIN, grid, g);
    std::vector<int> l = slic(imIN, S, grid, m);

    write(imOUTn, grid, l, w, h);
    
    
    return 0;
}