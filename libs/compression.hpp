#pragma once

#include "slic.hpp"
#include "bitFile.hpp"
#include "stdio.h"

#include <cstdint>
#include <limits>


int numBitForLabel(int numLabel)
{
    int num_bit = 0;
    while (((uint64_t)1 << num_bit) < (uint64_t)numLabel)
        num_bit++;
    return num_bit;
}


/// @brief déclare le nombre de superpixels.
/// @param grid la grille de superpixels
/// @param l l'association pixel -> superpixel
/// @param file le fichier de destination
void writeHeader(bitFile & file, std::vector<Superpixel> & grid, std::vector<int> &l, int w, int h, int & occ_bits)
{
    // on dit combien ya de sp
    file.addNBit((uint64_t)grid.size(), 32);
    file.addNBit((uint64_t)l.size(), 64);
    file.addNBit(w, 32);
    file.addNBit(h, 32); 
    // on compte le nombre max d'occ dans l pour compresser
    int max = 1;
    int pMax = 1;
    for (size_t i = 0; i + 1 < l.size(); i++)
    {
        if (l[i + 1] == l[i])
        {
            max++;
        }
        else
        {
            if (max > pMax)
                pMax = max;
            max = 1;
        }
    }
    if (max > pMax)
        pMax = max;
    occ_bits = numBitForLabel(pMax);
    file.addNBit(occ_bits, 32);
}

void writeColors(bitFile & file, std::vector<Superpixel> & grid)
{
    int c = sizeof(unsigned char) * 8;
    for (size_t i = 0; i < grid.size(); i++)
    {
        file.addNBit(grid[i].mr, c);
        file.addNBit(grid[i].mg, c);
        file.addNBit(grid[i].mb, c);
    }
}



void writePx(bitFile & file, std::vector<Superpixel> & grid, std::vector<int> & l, int occ_bits)
{
    int num_byte = numBitForLabel((int)grid.size());
    
    for (size_t i = 0; i < l.size(); i++)
    {
        uint32_t c = 1;
        while (i + 1 < l.size() && l[i] == l[i + 1] && c < std::numeric_limits<uint32_t>::max())
        {
            c++;
            i++;
        }
        file.addNBit(l[i], num_byte);
        file.addNBit((uint64_t)c, occ_bits);
    }
}

void write(char * fileName, std::vector<Superpixel> & grid, std::vector<int> &l, int w, int h)
{
    bitFile b;
    b.file = fileName;
    int occ_bits = 0;
    writeHeader(b, grid, l, w, h, occ_bits);
    writeColors(b, grid);
    writePx(b, grid, l, occ_bits);
    b.write();
}

void read(char * fileName, std::vector<Superpixel> & grid, std::vector<int> & l, int & w, int & h)
{
    bitFile b;
    b.file = fileName;
    b.load();
    int ptr = 0;
    int numSPX = b.readNBit(32, ptr);
    uint64_t numPX64 = b.readNBit64(64, ptr);
    w = b.readNBit(32, ptr);
    h = b.readNBit(32, ptr);
    int occ_bits = b.readNBit(32, ptr);
    size_t numPX = (numPX64 > (uint64_t)std::numeric_limits<size_t>::max()) ? 0 : (size_t)numPX64;
    grid = std::vector<Superpixel>((size_t)numSPX);
    int c = sizeof(unsigned char) * 8;
    for (int i = 0; i < numSPX; i++)
    {
        int mr = b.readNBit(c, ptr);
        int mg = b.readNBit(c, ptr);
        int mb = b.readNBit(c, ptr);
        grid[i] = Superpixel(0, 0, mr, mg, mb);
    }
    l = std::vector<int>(numPX);
    int num_byte = numBitForLabel(numSPX);
    size_t out = 0;
    while (out < numPX)
    {
        int label = b.readNBit(num_byte, ptr);
        uint64_t run = (occ_bits == 0) ? 1ULL : (uint64_t)b.readNBit(occ_bits, ptr);
        if (run == 0)
            run = 1;

        size_t remaining = numPX - out;
        size_t toWrite = (run > (uint64_t)remaining) ? remaining : (size_t)run;
        for (size_t k = 0; k < toWrite; k++)
            l[out + k] = label;
        out += toWrite;
    }
}