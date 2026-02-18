#include <cmath>
#include <vector>

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