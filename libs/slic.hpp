#include <cmath>
#include <vector>
#include <map>
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
                size_t dy = std::max(0, std::min(imIN.getHeight() - 2, y+h));
                size_t dx = std::max(0, std::min(imIN.getWidth() - 2, x+w));
                double r0 = imIN[(dy)*3][(dx)*3];
                double g0 = imIN[(dy)*3][(dx)*3+1];
                double b0 = imIN[(dy)*3][(dx)*3+2];
                
                double r1 = imIN[(dy)*3][(dx+1)*3];
                double g1 = imIN[(dy)*3][(dx+1)*3+1];
                double b1 = imIN[(dy)*3][(dx+1)*3+2];

                double r2 = imIN[(dy+1)*3][(dx)*3];
                double g2 = imIN[(dy+1)*3][(dx)*3+1];
                double b2 = imIN[(dy+1)*3][(dx)*3+2];

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
double _ds(
    double px, double py, 
    double sx, double sy
)
{
    double x = (px - sx);
    double y = (py - sy);
    return std::sqrt((x * x) + (y * y));
}

double _dc(
    double pr, double pg, double pb,
    double sr, double sg, double sb
)
{
    double r = (pr - sr);
    double g = (pg - sg);
    double b = (pb - sb);
    return std::sqrt((r*r)+(g*g)+(b*b));
}

double dist(
    double px, double py, double pr, double pg, double pb, 
    double sx, double sy, double sr, double sg, double sb, 
    double m, double S
)
{
    double ds = _ds(px, py, sx, sy);
    double dc = _dc(pr, pg, pb, sr, sg, sb);
    return m*m*((ds * ds) / (S * S)) + (dc * dc);
}

std::vector<Superpixel> _updt(ImageBase & imIN, std::vector<int> & l, size_t K)
{
    std::vector<Superpixel> nG(K);
    std::vector<double> n(K);
    for (size_t k = 0; k < K; k++)
    {
        nG[k] = Superpixel(0, 0, 0, 0, 0);
        n[k] = 0.0;
    }

    const int W = imIN.getWidth();
    const int H = imIN.getHeight();
    const int size = W * H;

    for (int i = 0; i < size; i++)
    {
        const int k = l[i];
        const int x = i % W;
        const int y = i / W;

        nG[k].mx += (double)x;
        nG[k].my += (double)y;
        nG[k].mr += (double)imIN.getData()[i * 3];
        nG[k].mg += (double)imIN.getData()[i * 3 + 1];
        nG[k].mb += (double)imIN.getData()[i * 3 + 2];
        n[k] += 1.0;
    }
    for (size_t k = 0; k < K; k++)
    {
        if (n[k] <= 0.0)
            continue;
        nG[k].mx /= n[k];
        nG[k].my /= n[k];
        nG[k].mr /= n[k];
        nG[k].mg /= n[k];
        nG[k].mb /= n[k];
    }
    return nG;
}

std::vector<int> slic(ImageBase & imIN, int S, std::vector<Superpixel> & grid, double m)
{
    std::vector<double> d(imIN.getWidth() * imIN.getHeight());
    std::vector<int> l(imIN.getWidth() * imIN.getHeight());

    for (size_t k = 0; k < grid.size(); k++)
    {
        int x = std::max(0, std::min((int)grid[k].mx, imIN.getWidth() - 2));
        int y = std::max(0, std::min((int)grid[k].my, imIN.getHeight() - 2));
        grid[k].mr = (double)imIN[y * 3][x * 3];
        grid[k].mg = (double)imIN[y * 3][x * 3 + 1];
        grid[k].mb = (double)imIN[y * 3][x * 3 + 2];
    }

    double E = MAXFLOAT;
    while (E > 0.5)
    {
        for (size_t i = 0; i < d.size(); i++)
            d[i] = MAXFLOAT;
        for (size_t k = 0; k < grid.size(); k++)
            for (int h = -S; h <= S; h++)
                for (int w = -S; w <= S; w++)
                {
                    int x = std::max(0, std::min(((int)grid[k].mx + w), imIN.getWidth() - 2));
                    int y = std::max(0, std::min(((int)grid[k].my + h), imIN.getHeight() - 2));
                    double e = dist(x, y, imIN[y*3][x*3], imIN[y*3][x*3+1], imIN[y*3][x*3+2], grid[k].mx, grid[k].my, grid[k].mr, grid[k].mg, grid[k].mb, m, S);
                    if (d[y * imIN.getWidth() + x] > e)
                    {
                        d[y * imIN.getWidth() + x] = e;
                        l[y * imIN.getWidth() + x] = k;
                    }
                }
        std::vector<Superpixel> nG = _updt(imIN, l, grid.size());
        double acc = 0.0;
        for (size_t k = 0; k < grid.size(); k++)
        {
            double ds = _ds(grid[k].mx, grid[k].my, nG[k].mx, nG[k].my);
            acc += (ds * ds);
        }
        acc/=grid.size();
        E = acc;
        grid = nG;
    }

    return l;
}
