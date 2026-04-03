#include <cmath>
#include <iostream>
#include <vector>
#include <cstring>
#include "../../libs/image_ppm.h"

int main(int argc, char *argv[])
{
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " image_ref.ppm image_test.ppm\n";
        return 1;
    }

    const char *refFile = argv[1];
    const char *testFile = argv[2];

    int refRows, refCols;
    int testRows, testCols;

    // Lecture des dimensions
    lire_nb_lignes_colonnes_image_ppm((char*)refFile, &refRows, &refCols);
    lire_nb_lignes_colonnes_image_ppm((char*)testFile, &testRows, &testCols);

    if (refRows != testRows || refCols != testCols) {
        std::cerr << "Erreur : dimensions différentes (" << refCols << "x" << refRows
                  << " vs " << testCols << "x" << testRows << ")\n";
        return 2;
    }

    int pixelCount = refRows * refCols;
    int totalComponents = pixelCount * 3;

    std::vector<OCTET> refData(totalComponents);
    std::vector<OCTET> testData(totalComponents);

    // Chargement des données
    lire_image_ppm((char*)refFile, refData.data(), pixelCount);
    lire_image_ppm((char*)testFile, testData.data(), pixelCount);

    double mse = 0.0;
    for (int i = 0; i < totalComponents; i++) {
        double diff = double(refData[i]) - double(testData[i]);
        mse += diff * diff;
    }

    mse /= double(totalComponents);
    if (mse <= 0.0) {
        std::cout << "PSNR = Infinity (images identiques)\n";
        return 0;
    }

    double maxVal = 255.0;
    double psnr = 10.0 * std::log10((maxVal * maxVal) / mse);

    std::cout << "PSNR = " << psnr << " dB" << std::endl;
    return 0;
}
