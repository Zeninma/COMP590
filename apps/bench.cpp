/**
 *  Copyright 2015 Mike Reed
 */

#include "bench.h"
#include "GCanvas.h"
#include "GBitmap.h"
#include "GTime.h"
#include <memory>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

static bool is_dir(const char path[]) {
    struct stat status;
    return !stat(path, &status) && (status.st_mode & S_IFDIR);
}

static bool mk_dir(const char path[]) {
    if (is_dir(path)) {
        return true;
    }
    if (!access(path, F_OK)) {
        fprintf(stderr, "%s exists but is not a directory\n", path);
        return false;
    }
    if (mkdir(path, 0777)) {
        fprintf(stderr, "error creating dir %s\n", path);
        return false;
    }
    return true;
}

static int pixel_diff(GPixel p0, GPixel p1) {
    int da = abs(GPixel_GetA(p0) - GPixel_GetA(p1));
    int dr = abs(GPixel_GetR(p0) - GPixel_GetR(p1));
    int dg = abs(GPixel_GetG(p0) - GPixel_GetG(p1));
    int db = abs(GPixel_GetB(p0) - GPixel_GetB(p1));
    return std::max(da, std::max(dr, std::max(dg, db)));
}

static double compare(const GBitmap& a, const GBitmap& b, int tolerance, bool verbose) {
    GASSERT(a.width() == b.width());
    GASSERT(a.height() == b.height());

    const int total = a.width() * a.height() * 255;

    const GPixel* rowA = a.pixels();
    const GPixel* rowB = b.pixels();

    int total_diff = 0;
    int max_diff = 0;

    for (int y = 0; y < a.height(); ++y) {
        for (int x = 0; x < a.width(); ++x) {
            int diff = pixel_diff(rowA[x], rowB[x]) - tolerance;
            if (diff > 0) {
                total_diff += diff;
                max_diff = std::max(max_diff, diff);
            }
        }
        rowA = (const GPixel*)((const char*)rowA + a.rowBytes());
        rowB = (const GPixel*)((const char*)rowB + b.rowBytes());
    }
    
    double score = 1.0 * (total - total_diff) / total;
    if (verbose) {
        printf("    - score %d, max_diff %d\n", (int)(score * 100), max_diff);
    }
    return score;
}

/////////////////////////////////////////////////////////////////////////////////////////////////

static void setup_bitmap(GBitmap* bitmap, int w, int h) {
    bitmap->fWidth = w;
    bitmap->fHeight = h;
    bitmap->fRowBytes = w * sizeof(GPixel);
    bitmap->fPixels = (GPixel*)calloc(bitmap->rowBytes(), bitmap->height());
}

static GPixel* get_addr(const GBitmap& bm, int x, int y) {
    GASSERT(x >= 0 && x < bm.width());
    GASSERT(y >= 0 && y < bm.height());
    return bm.pixels() + x + y * (bm.rowBytes() >> 2);
}

static double handle_proc(GBenchmark* bench, const char path[], GBitmap* bitmap, bool forever) {
    GISize size = bench->size();
    setup_bitmap(bitmap, size.fWidth, size.fHeight);

    std::unique_ptr<GCanvas> canvas(GCanvas::Create(*bitmap));
    if (!canvas) {
        fprintf(stderr, "failed to create canvas for [%d %d] %s\n",
                size.fWidth, size.fHeight, bench->name());
        return 0;
    }

    const int N = 100;
    GMSec now = GTime::GetMSec();
    for (int i = 0; i < N || forever; ++i) {
        canvas->save();
        bench->draw(canvas.get());
        canvas->restore();
    }
    GMSec dur = GTime::GetMSec() - now;
    return dur * 1.0 / N;
}

static bool is_arg(const char arg[], const char name[]) {
    std::string str("--");
    str += name;
    if (!strcmp(arg, str.c_str())) {
        return true;
    }

    char shortVers[3];
    shortVers[0] = '-';
    shortVers[1] = name[0];
    shortVers[2] = 0;
    return !strcmp(arg, shortVers);
}

int main(int argc, char** argv) {
    bool verbose = false;
    bool forever = false;
    const char* match = NULL;
    const char* report = NULL;
    const char* author = NULL;
    const char* write_dir = nullptr;
    FILE* reportFile = NULL;

    for (int i = 1; i < argc; ++i) {
        if (is_arg(argv[i], "report") && i+2 < argc) {
            report = argv[++i];
            author = argv[++i];
            reportFile = fopen(report, "a");
            if (!reportFile) {
                printf("----- can't open %s for author %s\n", report, author);
                return -1;
            }
        } else if (is_arg(argv[i], "verbose")) {
            verbose = true;
        } else if (is_arg(argv[i], "match") && i+1 < argc) {
            match = argv[++i];
        } else if (is_arg(argv[i], "forever")) {
            forever = true;
        } else if (is_arg(argv[i], "write") && i+1 < argc) {
            write_dir = argv[++i];
        }
    }

    for (int i = 0; gBenchFactories[i]; ++i) {
        std::unique_ptr<GBenchmark> bench(gBenchFactories[i]());
        const char* name = bench->name();
        
        if (match && !strstr(name, match)) {
            continue;
        }
        if (verbose) {
            printf("image: %s\n", name);
        }
        
        GBitmap testBM;
        double dur = handle_proc(bench.get(), name, &testBM, forever);
        printf("bench: %s %g\n", name, dur);

        if (write_dir) {
            std::string path(write_dir);
            path += "/";
            path += name;
            path += ".png";
            testBM.writeToFile(path.c_str());

        }
        free(testBM.fPixels);
    }
    return 0;
}
