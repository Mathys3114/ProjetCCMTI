#include <iostream>
#include <cmath>
#include <vector>
#include <cstdlib>
#include "ImageBase.h"
#include "slic.hpp"

const char *USAGE = "slicCC K m g imgIN.ppm imgOUT.ppm";

int main(int argc, char *argv[])
{
    if (argc != 6)
    {
        std::cerr << USAGE << std::endl;
        return 1;
    }
    double K = std::atof(argv[1]);
    double m = std::atof(argv[2]);
    double g = std::atof(argv[3]);
    char *imINn = argv[4];
    char *imOUTn = argv[5];
    ImageBase imIN;
    imIN.load(imINn);

    double w = imIN.getWidth();
    double h = imIN.getHeight();
    double N = w * h;
    int S = std::floor(std::sqrt(N / K));
    std::vector<Superpixel> grid = genGrid(w, h, K);
    grid = nudgeAlongGradient(imIN, grid, g);
    std::vector<int> l = slic(imIN, S, grid, m);
    CCResult res = labelConnectedComponents(w, h, l);

    std::vector<unsigned char> randomColors(res.num_components * 3);
    srand(67);
    for (int i = 0; i < res.num_components; i++)
    {
        randomColors[i * 3] = rand() % 256;
        randomColors[i * 3 + 1] = rand() % 256;
        randomColors[i * 3 + 2] = rand() % 256;
    }

    ImageBase imOUT(imIN.getWidth(), imIN.getHeight(), true);

    for (size_t i = 0; i < res.cc.size(); i++)
    {
        int component_id = res.cc[i];

        imOUT.getData()[i * 3] = randomColors[component_id * 3];
        imOUT.getData()[i * 3 + 1] = randomColors[component_id * 3 + 1];
        imOUT.getData()[i * 3 + 2] = randomColors[component_id * 3 + 2];
    }

    imOUT.save(imOUTn);
    return 0;
}