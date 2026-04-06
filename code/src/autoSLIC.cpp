#include <iostream>
#include <cmath>
#include <vector>
#include "ImageBase.h"
#include "slic.hpp"
#include "compression.hpp"
#include "compEstimator.hpp"

const char *USAGE = "autoSLIC imgIN.ppm dest.slic\nCompress an image using slic, and write the result to a file";

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        std::cerr << USAGE << std::endl;
        return 1;
    }
    char *imINn = argv[1];
    char *imOUTn = argv[2];
    ImageBase imIN;
    imIN.load(imINn);

    double w = imIN.getWidth();
    double h = imIN.getHeight();

    double N = w * h;
    double K = 0.0; // TO FIND
    double g = 0.0; // TO FIND
    double m = 0.0; // TO FIND

    // Controle de variation (pas ultra clean, mais fait le taf pour tester l'espace)
    const size_t num_step_K = 8;
    const double step_K = 256;
    const double offset_K = 256;
    const size_t num_step_G = 10;
    const double step_G = 1;
    const double offset_G = 1;
    const size_t num_step_M = 10;
    const double step_M = 0.1;
    const double offset_M = 0.1;

    

    const double psnrGoal = 30.0;
    double min_Comp = MAXFLOAT;
    double best_psnrSDiff = MAXFLOAT;

    double bK = 0.0;
    double bG = 0.0;
    double bM = 0.0;
    for (size_t a = 0; a < num_step_K; a++)
    {
        for (size_t b = 0; b < num_step_G; b++)
        {
            for (size_t c = 0; c < num_step_M; c++)
            {
                K = offset_K + (a * step_K);
                g = offset_G + (b * step_G);
                m = offset_M + (c * step_M);


                double S = std::floor(std::sqrt(N / K)); // step size

                std::vector<Superpixel> grid = genGrid(w, h, K);
                grid = nudgeAlongGradient(imIN, grid, g);
                std::vector<int> l = slic(imIN, S, grid, m);
                enforceConnectivity(imIN, l, grid, w, h);

                double cf = compressionFactor(l, grid.size());
                double ps = si_PSNR(grid, l, imIN);
                std::cout << "Current PSNR : " << ps << std::endl
                          << "Current compression factor : " << cf << std::endl;
                // Selection : 
                // - Si psnr < psnrGoal => ignore
                // - Si meilleure Comp ou meilleure diffPSNR => update comp + best
                if (ps < psnrGoal)
                    continue;
                
                if (psnrGoal - ps < best_psnrSDiff || cf < min_Comp)
                {
                    if (psnrGoal - ps < best_psnrSDiff)
                        best_psnrSDiff = psnrGoal - ps;
                    if (cf < min_Comp)
                        min_Comp = cf;

                    bK = K;
                    bG = g;
                    bM = m;
                    std::cout << "Current best : " << std::endl
                              << " - K : " << bK << std::endl
                              << " - g : " << bG << std::endl
                              << " - m : " << bM << std::endl;
                }
            }
        }
    }

    double S = std::floor(std::sqrt(N / bK)); // step size

    std::vector<Superpixel> grid = genGrid(w, h, bK);
    grid = nudgeAlongGradient(imIN, grid, bG);
    std::vector<int> l = slic(imIN, S, grid, bM);

    enforceConnectivity(imIN, l, grid, w, h);

    write(imOUTn, grid, l, w, h);    
    
    return 0;
}