#pragma once

#include <cstdio>
#include <vector>
#include <cmath>
#include <cstdint>

typedef std::vector<bool> BIN;

BIN intToBin(int val)
{
    if (val <= 0)
        return BIN();

    uint64_t uval = (uint64_t)val;
    int num_bit = 0;
    while ((uval >> num_bit) != 0)
        num_bit++;

    BIN res(num_bit);
    for (int i = 0; i < num_bit; i++)
        res[i] = (uval >> (num_bit - (i + 1))) & 1ULL;

    return res;
}

BIN u64ToBin(uint64_t val)
{
    if (val == 0)
        return BIN();

    int num_bit = 0;
    while ((val >> num_bit) != 0)
        num_bit++;

    BIN res(num_bit);
    for (int i = 0; i < num_bit; i++)
        res[i] = (val >> (num_bit - (i + 1))) & 1ULL;

    return res;
}



struct bitFile
{
    char * file;
    __uint8_t buf = 0; // accumulateur
    int index = -1; // indice dans l'acc

    std::vector<unsigned char> dat = std::vector<unsigned char>();

    int readNBit(int n, int &ptr)
    {
        if (n <= 0)
            return 0;
        if (ptr < 0)
            ptr = 0;

        size_t total_bit = dat.size() * 8;
        if ((size_t)ptr + (size_t)n > total_bit)
            return 0;

        int res = 0;
        for (int i = 0; i < n; i++)
        {
            size_t byte_idx = (size_t)ptr / 8;
            int bit_idx = ptr % 8;
            int bit = (dat[byte_idx] >> bit_idx) & 1;
            res = (res << 1) | bit;
            ptr++;
        }
        return res;
    }

    uint64_t readNBit64(int n, int &ptr)
    {
        if (n <= 0)
            return 0;
        if (ptr < 0)
            ptr = 0;

        size_t total_bit = dat.size() * 8;
        if ((size_t)ptr + (size_t)n > total_bit)
            return 0;

        uint64_t res = 0;
        for (int i = 0; i < n; i++)
        {
            size_t byte_idx = (size_t)ptr / 8;
            int bit_idx = ptr % 8;
            uint64_t bit = (uint64_t)((dat[byte_idx] >> bit_idx) & 1);
            res = (res << 1) | bit;
            ptr++;
        }
        return res;
    }

    void addNBit(BIN val, int n)
    {
        if (n <= 0)
            return;

        int start = 0;
        if ((int)val.size() > n)
            start = (int)val.size() - n;

        int pad = n - ((int)val.size() - start);
        if (pad < 0)
            pad = 0;

        // padding
        for (int i = 0; i < pad; i++)
        {
            ++index;
            if (index == 7)
            {
                dat.push_back((unsigned char)buf);
                buf=0;
                index=-1;
            }
        }
        // copy
        for(int i = start; i < (int)val.size(); i++)
        {
            buf += val[i]<<(++index);
            if (index == 7)
            {
                dat.push_back((unsigned char)buf);
                buf=0;
                index=-1;
            }
        }
    }

    void addNBit(int val, int n)
    {
        addNBit(intToBin(val), n);
    }

    void addNBit(unsigned char val, int n)
    {
        addNBit((uint64_t)val, n);
    }

    void addNBit(uint64_t val, int n)
    {
        addNBit(u64ToBin(val), n);
    }
    void addNBit(double val, int n)
    {
        addNBit(((int)val), n);
    }

    void write()
    {
        if (index != -1)
        {
            dat.push_back((unsigned char)buf);
            buf=0;
            index=-1;
        }
        FILE *f = fopen(file, "wb");
        fwrite(dat.data(), sizeof(unsigned char),dat.size(), f);
        fclose(f);   
    }

    void load()
    {
        dat.clear();
        buf = 0;
        index = -1;

        FILE *f = fopen(file, "rb");
        if (f == nullptr)
            return;

        fseek(f, 0, SEEK_END);
        long size = ftell(f);
        if (size <= 0)
        {
            fclose(f);
            return;
        }
        fseek(f, 0, SEEK_SET);

        dat.resize((size_t)size);
        fread(dat.data(), sizeof(unsigned char), (size_t)size, f);
        fclose(f);
    }
};
