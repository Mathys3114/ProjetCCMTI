#include <iostream>
#include <cmath>
#include <vector>
#include <unordered_map>
#include <limits>
#include <future>
#include <mutex>
#include <atomic>
#include <algorithm>
#include <string>
#include "ImageBase.h"
#include "slic.hpp"
#include "compression.hpp"
#include "compEstimator.hpp"

const char *USAGE = "autoSLIC [mode] imgIN.ppm dest.ppm\n"
                    "mode: SLIC|classic or slicCC (default: slicCC when omitted)";

struct EvalResult
{
    double K = 0.0;
    int g = 0;
    double m = 0.0;
    double psnr = -std::numeric_limits<double>::infinity();
    double cf = std::numeric_limits<double>::infinity();
    bool valid = false;
};

static bool isBetter(const EvalResult &cand, const EvalResult &best, double psnrGoal)
{
    if (!best.valid)
        return cand.valid;
    if (!cand.valid)
        return false;

    const bool candFeasible = (cand.psnr >= psnrGoal);
    const bool bestFeasible = (best.psnr >= psnrGoal);

    if (candFeasible != bestFeasible)
        return candFeasible;

    if (!candFeasible)
        return cand.psnr > best.psnr;

    // Une fois PSNR >= objectif, on privilegie la compression.
    const double cfTie = 1e-6;
    if (cand.cf + cfTie < best.cf)
        return true;
    if (best.cf + cfTie < cand.cf)
        return false;

    return cand.psnr > best.psnr;
}

int main(int argc, char *argv[])
{
    bool useConnectivity = true;
    char *imINn = nullptr;
    char *imOUTn = nullptr;

    if (argc == 3)
    {
        // Compatibilite historique: sans mode explicite, on garde le comportement slicCC.
        imINn = argv[1];
        imOUTn = argv[2];
    }
    else if (argc == 4)
    {
        std::string mode = argv[1];
        std::transform(mode.begin(), mode.end(), mode.begin(),
                       [](unsigned char c)
                       { return static_cast<char>(std::tolower(c)); });

        if (mode == "slic" || mode == "classic")
            useConnectivity = false;
        else if (mode == "sliccc")
            useConnectivity = true;
        else
        {
            std::cerr << "Unknown mode: " << argv[1] << std::endl;
            std::cerr << USAGE << std::endl;
            return 1;
        }

        imINn = argv[2];
        imOUTn = argv[3];
    }
    else
    {
        std::cerr << USAGE << std::endl;
        return 1;
    }

    ImageBase imIN;
    imIN.load(imINn);

    double w = imIN.getWidth();
    double h = imIN.getHeight();

    double N = w * h;
    const double psnrGoal = 30.0;
    const bool fastSearch = false;
    const bool verboseEval = false;
    const int maxEvaluations = 240;

    std::vector<double> KValues;
    {
        // Espace de recherche adaptatif en taille de superpixels (S),
        // puis conversion en K ~= N / S^2.
        const std::vector<int> SValues = {64, 56, 48, 40, 36, 32, 28, 24, 20, 18, 16, 14, 12, 10, 8, 6, 5, 4, 3, 2};
        for (size_t i = 0; i < SValues.size(); i++)
        {
            const double s = static_cast<double>(SValues[i]);
            double k = std::round(N / (s * s));
            if (k < 64.0)
                k = 64.0;
            if (k > N)
                k = N;
            KValues.push_back(k);
        }
        std::sort(KValues.begin(), KValues.end());
        KValues.erase(std::unique(KValues.begin(), KValues.end()), KValues.end());
    }
    const std::vector<int> GValues = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    const std::vector<double> MValues = {0.05, 0.1, 0.2, 0.3, 0.4, 0.5, 0.7, 1.0};

    const int kCount = static_cast<int>(KValues.size());
    const int gCount = static_cast<int>(GValues.size());
    const int mCount = static_cast<int>(MValues.size());

    std::unordered_map<int, EvalResult> cache;
    std::mutex cacheMutex;
    std::atomic<int> evalCount{0};

    auto computeEval = [&](double K, int g, double m)
    {
        EvalResult r;
        r.valid = true;
        r.K = K;
        r.g = g;
        r.m = m;

        int S = static_cast<int>(std::floor(std::sqrt(N / r.K)));
        if (S < 1)
            S = 1;

        std::vector<Superpixel> grid = genGrid(w, h, r.K);
        if (grid.empty())
        {
            r.valid = false;
            return r;
        }

        grid = nudgeAlongGradient(imIN, grid, r.g);
        std::vector<int> l = slic(imIN, S, grid, r.m);
        if (useConnectivity)
            enforceConnectivity(imIN, l, grid, static_cast<int>(w), static_cast<int>(h));

        r.cf = compressionFactor(l, grid.size());
        r.psnr = si_PSNR(grid, l, imIN);
        if (r.psnr < 0.0)
            r.psnr = 1e9;
        return r;
    };

    auto keyFor = [gCount, mCount](int ki, int gi, int mi)
    {
        return (ki * gCount + gi) * mCount + mi;
    };

    auto acquireEvalSlot = [&]()
    {
        int cur = evalCount.load(std::memory_order_relaxed);
        while (cur < maxEvaluations)
        {
            if (evalCount.compare_exchange_weak(cur, cur + 1, std::memory_order_relaxed))
                return true;
        }
        return false;
    };

    auto evaluate = [&](int ki, int gi, int mi)
    {
        const int key = keyFor(ki, gi, mi);
        {
            std::lock_guard<std::mutex> lock(cacheMutex);
            auto it = cache.find(key);
            if (it != cache.end())
                return it->second;
        }

        if (!acquireEvalSlot())
            return EvalResult{};

        EvalResult r = computeEval(KValues[ki], GValues[gi], MValues[mi]);

        {
            std::lock_guard<std::mutex> lock(cacheMutex);
            cache[key] = r;
        }
        if (verboseEval)
        {
            std::cout << "Eval #" << evalCount.load(std::memory_order_relaxed)
                      << " -> K=" << r.K
                      << " g=" << r.g
                      << " m=" << r.m
                      << " | PSNR=" << r.psnr
                      << " | CF=" << r.cf
                      << std::endl;
        }
        return r;
    };

    auto hillClimb = [&](int startK, int startG, int startM)
    {
        int ck = startK;
        int cg = startG;
        int cm = startM;

        EvalResult current = evaluate(ck, cg, cm);
        bool improved = true;

        while (improved)
        {
            if (evalCount.load(std::memory_order_relaxed) >= maxEvaluations)
                break;

            improved = false;

            int bestK = ck;
            int bestG = cg;
            int bestM = cm;
            EvalResult bestLocal = current;

            const int d[2] = {-1, 1};
            for (int i = 0; i < 2; i++)
            {
                int nk = ck + d[i];
                if (nk >= 0 && nk < kCount)
                {
                    EvalResult cand = evaluate(nk, cg, cm);
                    if (isBetter(cand, bestLocal, psnrGoal))
                    {
                        bestLocal = cand;
                        bestK = nk;
                        bestG = cg;
                        bestM = cm;
                        improved = true;
                    }
                }

                int ng = cg + d[i];
                if (ng >= 0 && ng < gCount)
                {
                    EvalResult cand = evaluate(ck, ng, cm);
                    if (isBetter(cand, bestLocal, psnrGoal))
                    {
                        bestLocal = cand;
                        bestK = ck;
                        bestG = ng;
                        bestM = cm;
                        improved = true;
                    }
                }

                int nm = cm + d[i];
                if (nm >= 0 && nm < mCount)
                {
                    EvalResult cand = evaluate(ck, cg, nm);
                    if (isBetter(cand, bestLocal, psnrGoal))
                    {
                        bestLocal = cand;
                        bestK = ck;
                        bestG = cg;
                        bestM = nm;
                        improved = true;
                    }
                }
            }

            if (improved)
            {
                ck = bestK;
                cg = bestG;
                cm = bestM;
                current = bestLocal;
            }
        }

        return current;
    };

    EvalResult best;
    const std::vector<std::vector<int>> starts = {
        {kCount / 2, gCount / 2, mCount / 2},
        {0, 0, 0},
        {kCount - 1, 0, 0},
        {0, gCount - 1, mCount - 1},
        {kCount - 1, gCount - 1, mCount - 1},
        {kCount / 3, gCount / 3, mCount / 3},
        {2 * kCount / 3, 2 * gCount / 3, 2 * mCount / 3}};

    std::vector<std::future<EvalResult>> futures;
    futures.reserve(starts.size());

    for (size_t i = 0; i < starts.size(); i++)
    {
        futures.push_back(std::async(std::launch::async, [&, i]()
                                     { return hillClimb(starts[i][0], starts[i][1], starts[i][2]); }));
    }

    for (size_t i = 0; i < futures.size(); i++)
    {
        EvalResult cand = futures[i].get();
        if (isBetter(cand, best, psnrGoal))
            best = cand;
    }

    if (!best.valid)
    {
        std::cerr << "No valid parameter set found." << std::endl;
        return 2;
    }

    if (best.psnr < psnrGoal)
    {
        // Rattrapage: validation exhaustive par K croissant avec connectivite active
        // pour forcer l'obtention de PSNR >= objectif si possible.
        EvalResult recovered;
        for (int ki = 0; ki < kCount; ki++)
        {
            const std::vector<double> mTry = {0.05, 0.1, 0.2, 0.3, 0.5};
            for (size_t mi = 0; mi < mTry.size(); mi++)
            {
                EvalResult cand = computeEval(KValues[ki], best.g > 0 ? best.g : 1, mTry[mi]);
                if (cand.psnr >= psnrGoal)
                {
                    if (isBetter(cand, recovered, psnrGoal))
                        recovered = cand;
                }
            }
            if (recovered.valid)
                break;
        }

        if (recovered.valid)
        {
            best = recovered;
        }
        else
        {
            std::cerr << "Warning: no candidate reached PSNR >= " << psnrGoal
                      << " dB. Using best available PSNR = " << best.psnr << " dB." << std::endl;
        }
    }

    std::cout << "Final best parameters:" << std::endl
              << " - mode : " << (useConnectivity ? "slicCC" : "SLIC") << std::endl
              << " - K : " << best.K << std::endl
              << " - g : " << best.g << std::endl
              << " - m : " << best.m << std::endl
              << " - PSNR : " << best.psnr << std::endl
              << " - Compression factor : " << best.cf << std::endl
              << " - Unique evaluations : " << evalCount.load(std::memory_order_relaxed) << std::endl
              << " - fastSearch : " << (fastSearch ? "ON" : "OFF") << std::endl
              << " - evaluation budget : " << maxEvaluations << std::endl;

    int S = static_cast<int>(std::floor(std::sqrt(N / best.K)));
    if (S < 1)
        S = 1;

    std::vector<Superpixel> grid = genGrid(w, h, best.K);
    grid = nudgeAlongGradient(imIN, grid, best.g);
    std::vector<int> l = slic(imIN, S, grid, best.m);

    if (useConnectivity)
        enforceConnectivity(imIN, l, grid, w, h);

    write(imOUTn, grid, l, w, h);

    return 0;
}