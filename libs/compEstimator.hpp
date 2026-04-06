#pragma once
#include <vector>
#include "ImageBase.h"
#include "slic.hpp"
#include "compression.hpp"
#include <cmath>
// Taille de base / RLE + bitpacking
double compressionFactor(std::vector<int> & l, const size_t & gridSize)
{
    size_t maxVal = gridSize;
    size_t maxRep = 0;
    size_t c = 0;
    for (size_t i = 0; i < l.size() - 1; i++)
    {

        if (c>maxRep)
            maxRep = c;
        if (l[i] == l[i+1])
            c++;
        else
            c = 0;
    }
    return static_cast<double>(l.size() * (numBitForLabel(maxVal) + numBitForLabel(maxRep))) / static_cast<double>(l.size() * 24);
}

double si_PSNR(const std::vector<Superpixel> & grid,const std::vector<int> & l, ImageBase & img)
{
    double mse = 0.0;
        
    for (size_t i = 0; i < l.size(); i++)
    {
        const double rS = grid[l[i]].mr;
        const double gS = grid[l[i]].mg;
        const double bS = grid[l[i]].mb;

        const double rI = img.getData()[i*3];
        const double gI = img.getData()[i*3+1];
        const double bI = img.getData()[i*3+2];

        const double dR = (rS - rI);
        const double dG = (gS - gI);
        const double dB = (bS - bI);

        mse += (dR * dR) + (dG * dG) + (dB * dB);
    }

    if (mse == 0.0)
    {
        // img identiques.
        return -1;
    }
    mse /= l.size() * 3.0;

    const double maxVal = 255.0;
    return 10.0 * std::log10((maxVal * maxVal) / mse);
}