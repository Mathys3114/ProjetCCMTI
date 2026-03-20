#include <iostream>
#include <vector>
#include <cstdlib>
#include "ImageBase.h"

void applyMeanFilter(ImageBase &imIN, ImageBase &imOUT, int radius) {
    int w = imIN.getWidth();
    int h = imIN.getHeight();

    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            long r = 0, g = 0, b = 0;
            int count = 0;

            for (int dy = -radius; dy <= radius; ++dy) {
                for (int dx = -radius; dx <= radius; ++dx) {
                    int nx = x + dx;
                    int ny = y + dy;

                    if (nx >= 0 && nx < w && ny >= 0 && ny < h) {
                        r += imIN[ny * 3][nx * 3];
                        g += imIN[ny * 3][nx * 3 + 1];
                        b += imIN[ny * 3][nx * 3 + 2];
                        count++;
                    }
                }
            }
            imOUT[y * 3][x * 3]     = (unsigned char)(r / count);
            imOUT[y * 3][x * 3 + 1] = (unsigned char)(g / count);
            imOUT[y * 3][x * 3 + 2] = (unsigned char)(b / count);
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] << " radius imgIN.ppm imgOUT.ppm" << std::endl;
        return 1;
    }

    int radius = std::atoi(argv[1]);
    char *imINn = argv[2];
    char *imOUTn = argv[3];

    ImageBase imIN;
    imIN.load(imINn); 

    int w = imIN.getWidth();
    int h = imIN.getHeight();
    ImageBase imOUT(w, h, true); 
    
    applyMeanFilter(imIN, imOUT, radius);

    if (!imOUT.save(imOUTn)) {
        std::cerr << "Erreur lors de la sauvegarde." << std::endl;
        return 1;
    }

    return 0;
}