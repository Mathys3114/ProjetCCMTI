#pragma once

#include "slic.hpp"
#include "bitFile.hpp"
#include "stdio.h"





/// @brief déclare le nombre de superpixels.
/// @param grid la grille de superpixels
/// @param l l'association pixel -> superpixel
/// @param file le fichier de destination
void writeHeader(bitFile & file, std::vector<Superpixel> & grid, std::vector<int> &l, int w, int h)
{
    // on dit combien ya de sp
    file.addNBit((uint64_t)grid.size(), 32);
    file.addNBit((uint64_t)l.size(), 64);
    file.addNBit(w, 32);
    file.addNBit(h, 32);
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

int numBitForLabel(int numLabel)
{
    int num_bit = 0;
    while (((uint64_t)1 << num_bit) < (uint64_t)numLabel)
        num_bit++;
    return num_bit;
}

void writePx(bitFile & file, std::vector<Superpixel> & grid, std::vector<int> & l)
{
    int num_byte = numBitForLabel((int)grid.size());
    for (size_t i = 0; i < l.size(); i++)
        file.addNBit(l[i], num_byte);
}

void write(char * fileName, std::vector<Superpixel> & grid, std::vector<int> &l, int w, int h)
{
    bitFile b;
    b.file = fileName;
    writeHeader(b, grid, l, w, h);
    writeColors(b, grid);
    writePx(b, grid, l);
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
    for (size_t i = 0; i < numPX; i++)
    {
        l[i] = b.readNBit(num_byte, ptr);
    }
}