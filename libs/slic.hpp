#include <cmath>
#include <vector>
#include "ImageBase.h"

class Superpixel
{
    public:
        double mx = 0;
        double my = 0;
        double mr = 0;
        double mg = 0;
        double mb = 0;
        Superpixel(){}
        Superpixel(double x, double y, double r, double g, double b)
        {
            mx = x;
            my = y;
            mr = r;
            mg = g;
            mb = b;
        }
};
/// @brief génère une grille homogène centrée, visant K centres (si possible)
/// @param w largueur de l'image
/// @param h hauteur de l'image
/// @param K nombre de centres
/// @return la liste des centres.
std::vector<Superpixel> genGrid(double w, double h, double K)
{
    double N = w * h;
    double S = std::sqrt(N / K); // step size

    double pad_x = w - S * std::floor(w / S);
    double pad_y = h - S * std::floor(h / S);

    // spawn la grille
    std::vector<Superpixel> X_k(std::floor(w/S) * std::floor(h/S));

    for (int i = 0; i < std::floor(w / S); i++)
        for (int j = 0; j < std::floor(h / S); j++)
            X_k[i * std::floor(h / S) + j] = Superpixel(((double)i) * S + S/2.0 + pad_x/2.0, ((double)j) * S + S/2.0 + pad_y/2.0, 0.0, 0.0, 0.0);
    return X_k;
}


/// @brief fait glisser les centres vers les gradients les plus faibles dans une zone de 2g*2g autour d'eux.
/// @param imIN image à lire (ppm)
/// @param grid grille à nudge
/// @param g rayon de recherche
/// @return la grille dont les centres on été nudge
std::vector<Superpixel> & nudgeAlongGradient(ImageBase & imIN, std::vector<Superpixel> & grid, int g)
{
    for (size_t i = 0; i < grid.size(); i++)
    {
        int x = grid[i].mx;
        int y = grid[i].my;
        double m = MAXFLOAT;
        for (int h = -g; h < g;h++)
            for (int w = -g; w < g; w++)
            {
                double r0 = imIN[(y+h)*3][(x+w)*3];
                double g0 = imIN[(y+h)*3][(x+w)*3+1];
                double b0 = imIN[(y+h)*3][(x+w)*3+2];
                
                double r1 = imIN[(y+h)*3][(x+w+1)*3];
                double g1 = imIN[(y+h)*3][(x+w+1)*3+1];
                double b1 = imIN[(y+h)*3][(x+w+1)*3+2];

                double r2 = imIN[(y+1+h)*3][(x+w)*3];
                double g2 = imIN[(y+1+h)*3][(x+w)*3+1];
                double b2 = imIN[(y+1+h)*3][(x+w)*3+2];

                double r0s = r0 - r1;
                double g0s = g0 - g1;
                double b0s = b0 - b1;

                double r1s = r0 - r2;
                double g1s = g0 - g2;
                double b1s = b0 - b2;

                double G = (r0s * r0s) + (g0s * g0s) + (b0s * b0s) + (r1s * r1s) + (g1s * g1s) + (b1s * b1s);
                if (m > G)
                {
                    m = G;
                    grid[i].mx = x+w;
                    grid[i].my = y+h;
                }
            }
    }
    return grid;
}