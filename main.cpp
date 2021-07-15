#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>
#include "image.h"

void solve(lab_bmp::image &image, char *argv[]) {
    std::stringstream ss;
    ss << argv[4] << " " << argv[5] << " " << argv[6] << " " << argv[7];
    uint32_t x{};
    uint32_t y{};
    uint32_t w{};
    uint32_t h{};
    ss >> x >> y >> w >> h;
    image.crop(x, y, w, h);
    image.rotate_clockwise();
    std::ofstream fout(argv[3], std::ios::binary);
    if (!fout.is_open()) {
        std::stringstream sss;
        sss << "Unable to open file \"" << argv[3] << "\"\n";
        throw lab_bmp::except(sss.str());
    }
    fout.write(reinterpret_cast<char *>(image.get_BitMapFileHeader()), 14);
    fout.write(reinterpret_cast<char *>(image.get_BitMapInfoHeader()), 40);
    for (uint32_t i = 0; i < image.real_height(); i++) {
        for (uint32_t j = 0; j < image.real_width(); j++) {
            fout.write(reinterpret_cast<char *>(&image.pixel(j, i)), 3);
        }
        char Zero{};
        for (uint32_t j = 0; j < lab_bmp::ExtraBytes(image.real_width()); j++) {
            fout.write(&Zero, 1);
        }
    }
    fout.close();
}

int main(int argc, char *argv[]) {
    if (argc < 8) {
        std::cerr << "Missing arguments\n";
        return 1;
    }
    try {
        if (strcmp(argv[1], "crop-rotate") == 0) {
            lab_bmp::image i(argv[2]);
            solve(i, argv);
        }
        if (strcmp(argv[1], "download-crop-rotate") == 0) {
            lab_bmp::curl c(argv[2]);
            std::string s;
            c.read(s);
            lab_bmp::image i(s);
            solve(i, argv);
        }
    } catch (std::bad_alloc &) {
        std::cerr << "Insufficient memory\n";
        return 1;
    } catch (lab_bmp::except &exception) {
        std::cerr << exception.result();
        return 1;
    }
    return 0;
}
