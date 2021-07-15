#ifndef IMAGE_H_
#define IMAGE_H_
#pragma pack(push, 1)

#define _CRT_SECURE_NO_WARNINGS
#define WINDOWS_IGNORE_PACKING_MISMATCH

#include <curl/curl.h>
#include <fstream>
#include <sstream>
#include <vector>

namespace lab_bmp {
struct except : public std::exception {
private:
    std::string text;

public:
    explicit except(std::string part) : text(std::move(part)) {
    }

    except(const std::string &part1, int param, const std::string &part2) {
        std::stringstream ss;
        ss << part1 << param << part2;
        text = ss.str();
    }

    [[nodiscard]] std::string result() const noexcept {
        return text;
    }
};

struct BITMAPFILEHEADER {
    uint16_t bfType{};
    uint32_t bfSize{};
    uint16_t bfReserved1{};
    uint16_t bfReserved2{};
    uint32_t bfOffBits{};
};

struct BITMAPINFOHEADER {
    uint32_t biSize{};
    int32_t biWidth{};
    int32_t biHeight{};
    uint16_t biPlanes{};
    uint16_t biBitCount{};
    uint32_t biCompression{};
    uint32_t biSizeImage{};
    int32_t biXPelsPerMeter{};
    int32_t biYPelsPerMeter{};
    uint32_t biClrUsed{};
    uint32_t biClrImportant{};
};

struct Pixel {
    uint8_t Blue{};
    uint8_t Green{};
    uint8_t Red{};
};

[[nodiscard]] inline uint16_t convert(std::size_t number,
                                      const std::string &from_internet) {
    return static_cast<uint16_t>(from_internet[number + 1]) * 256 +
           static_cast<uint16_t>(from_internet[number]);
}

[[nodiscard]] inline uint32_t convert(std::size_t number,
                                      const std::string &from_internet,
                                      int) {
    return static_cast<uint32_t>(from_internet[number + 3]) * 256 * 256 * 256 +
           static_cast<uint32_t>(from_internet[number + 2]) * 256 * 256 +
           static_cast<uint32_t>(from_internet[number + 1]) * 256 +
           static_cast<uint32_t>(from_internet[number]);
}

inline void Read_BITMAPFILEHEADER(BITMAPFILEHEADER &BitMapFileHeader,
                                  const std::string &from_internet) {
    BitMapFileHeader.bfType = convert(0, from_internet);
    BitMapFileHeader.bfSize = convert(2, from_internet, 1);
    BitMapFileHeader.bfReserved1 = convert(6, from_internet);
    BitMapFileHeader.bfReserved2 = convert(8, from_internet);
    BitMapFileHeader.bfOffBits = convert(10, from_internet, 1);
}

inline void Read_BITMAPINFOHEADER(BITMAPINFOHEADER &BitMapInfoHeader,
                                  const std::string &from_internet) {
    BitMapInfoHeader.biSize = convert(14, from_internet, 1);
    BitMapInfoHeader.biWidth =
        static_cast<int32_t>(convert(18, from_internet, 1));
    BitMapInfoHeader.biHeight =
        static_cast<int32_t>(convert(22, from_internet, 1));
    BitMapInfoHeader.biPlanes = convert(26, from_internet);
    BitMapInfoHeader.biBitCount = convert(28, from_internet);
    BitMapInfoHeader.biCompression = convert(30, from_internet, 1);
    BitMapInfoHeader.biSizeImage = convert(34, from_internet, 1);
    BitMapInfoHeader.biXPelsPerMeter =
        static_cast<int32_t>(convert(38, from_internet, 1));
    BitMapInfoHeader.biYPelsPerMeter =
        static_cast<int32_t>(convert(42, from_internet, 1));
    BitMapInfoHeader.biClrUsed = convert(46, from_internet, 1);
    BitMapInfoHeader.biClrImportant = convert(50, from_internet, 1);
}

[[nodiscard]] inline uint32_t ExtraBytes(uint32_t Pixels) {
    uint32_t Count = 3 * Pixels;
    uint32_t Answer = 0;
    while (Count % 4 != 0) {
        Answer++;
        Count++;
    }
    return Answer;
}

struct image {
private:
    std::vector<Pixel> Data;
    uint8_t Rotator{};
    int32_t LeftBorder{};
    int32_t RightBorder{};
    int32_t TopBorder{};
    int32_t BottomBorder{};
    uint32_t Width{};
    uint32_t Height{};
    BITMAPFILEHEADER BitMapFileHeader;
    BITMAPINFOHEADER BitMapInfoHeader;

    void ConstructorNormalize() {
        if (BitMapInfoHeader.biSize != 40) {
            throw except(
                "Invalid BMP: expected version 3 and header size 40, but "
                "header size is ",
                BitMapInfoHeader.biSize, "\n");
        }
        if (BitMapInfoHeader.biHeight <= 0) {
            throw except("Invalid BMP: expected positive biHeight, got ",
                         BitMapInfoHeader.biHeight, "\n");
        }
        if (BitMapInfoHeader.biBitCount != 24) {
            throw except("Invalid BMP: expected 24 bits per pixel, got ",
                         BitMapInfoHeader.biBitCount, "\n");
        }
        if (BitMapInfoHeader.biCompression != 0) {
            throw except("Invalid BMP: compression is unsupported\n");
        }
        if (BitMapInfoHeader.biClrUsed != 0) {
            throw except("Invalid BMP: color palette is unsupported\n");
        }
        Width = BitMapInfoHeader.biWidth;
        Height = BitMapInfoHeader.biSizeImage / (3 * Width + ExtraBytes(Width));
        RightBorder = static_cast<int32_t>(Width);
        BottomBorder = static_cast<int32_t>(Height);
        Data.resize(Width * Height);
    }

    void Normalize() {
        if (LeftBorder < 0 || RightBorder < LeftBorder ||
            static_cast<int32_t>(Width) < RightBorder || TopBorder < 0 ||
            BottomBorder < TopBorder ||
            static_cast<int32_t>(Height) < BottomBorder) {
            throw except("The requested area is not a subimage\n");
        }
        BitMapFileHeader.bfSize =
            54 + real_height() * (3 * real_width() + ExtraBytes(real_width()));
        BitMapInfoHeader.biWidth = static_cast<int32_t>(real_width());
        BitMapInfoHeader.biHeight = static_cast<int32_t>(real_height());
        BitMapInfoHeader.biSizeImage =
            real_height() * (3 * real_width() + ExtraBytes(real_width()));
    }

public:
    image() = delete;

    image(image &&) = delete;

    image(const image &) = delete;

    image &operator=(image &&) = delete;

    image &operator=(const image &) = delete;

    explicit image(const char *Name) {
        std::ifstream fin(Name, std::ios::binary);
        if (!fin.is_open()) {
            std::stringstream ss;
            ss << "Unable to open file \"" << Name << "\"\n";
            throw except(ss.str());
        }
        fin.read(reinterpret_cast<char *>(&BitMapFileHeader),
                 sizeof(BitMapFileHeader));
        fin.read(reinterpret_cast<char *>(&BitMapInfoHeader),
                 sizeof(BitMapInfoHeader));
        ConstructorNormalize();
        for (uint32_t i = 0; i < Height; i++) {
            for (uint32_t j = 0; j < Width; j++) {
                fin.read(reinterpret_cast<char *>(&Data[i * Width + j]), 3);
            }
            char Zero{};
            for (uint32_t j = 0; j < ExtraBytes(Width); j++) {
                fin.read(&Zero, 1);
            }
        }
        fin.close();
    }

    explicit image(const std::string &from_internet) {
        Read_BITMAPFILEHEADER(BitMapFileHeader, from_internet);
        Read_BITMAPINFOHEADER(BitMapInfoHeader, from_internet);
        ConstructorNormalize();
        std::size_t current = 54;
        for (uint32_t i = 0; i < Height; i++) {
            for (uint32_t j = 0; j < Width; j++) {
                Data[i * Width + j].Blue = from_internet[current];
                Data[i * Width + j].Green = from_internet[current + 1];
                Data[i * Width + j].Red = from_internet[current + 2];
                current += 3;
            }
            current += ExtraBytes(Width);
        }
    }

    [[nodiscard]] BITMAPFILEHEADER *get_BitMapFileHeader() {
        return &BitMapFileHeader;
    }

    [[nodiscard]] BITMAPINFOHEADER *get_BitMapInfoHeader() {
        return &BitMapInfoHeader;
    }

    void rotate_clockwise() {
        Rotator++;
        if (Rotator == 4) {
            Rotator = 0;
        }
        Normalize();
    }

    [[nodiscard]] uint32_t real_width() const {
        if (Rotator % 2 == 0) {
            return RightBorder - LeftBorder;
        }
        return BottomBorder - TopBorder;
    }

    [[nodiscard]] uint32_t real_height() const {
        if (Rotator % 2 == 0) {
            return BottomBorder - TopBorder;
        }
        return RightBorder - LeftBorder;
    }

    void crop(uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
        switch (Rotator) {
            case 0: {
                LeftBorder += static_cast<int32_t>(x);
                TopBorder += static_cast<int32_t>(y);
                RightBorder = static_cast<int32_t>(LeftBorder + w);
                BottomBorder = static_cast<int32_t>(TopBorder + h);
                break;
            }
            case 1: {
                BottomBorder -= static_cast<int32_t>(x);
                LeftBorder += static_cast<int32_t>(y);
                RightBorder = static_cast<int32_t>(LeftBorder + h);
                TopBorder = static_cast<int32_t>(BottomBorder - w);
                break;
            }
            case 2: {
                RightBorder -= static_cast<int32_t>(x);
                BottomBorder -= static_cast<int32_t>(y);
                LeftBorder = static_cast<int32_t>(RightBorder - w);
                TopBorder = static_cast<int32_t>(BottomBorder - h);
                break;
            }
            default: {
                TopBorder += static_cast<int32_t>(x);
                RightBorder -= static_cast<int32_t>(y);
                LeftBorder = static_cast<int32_t>(RightBorder - h);
                BottomBorder = static_cast<int32_t>(TopBorder + w);
            }
        }
        Normalize();
    }

    [[nodiscard]] Pixel &pixel(uint32_t x, uint32_t y) {
        switch (Rotator) {
            case 0: {
                return Data[Width * (Height - BottomBorder + y) + LeftBorder +
                            x];
            }
            case 1: {
                return Data[Width * (Height - BottomBorder + x) + LeftBorder +
                            real_height() - y - 1];
            }
            case 2: {
                return Data[Width * (Height - BottomBorder + real_height() - y -
                                     1) +
                            LeftBorder + real_width() - x - 1];
            }
            default: {
                return Data[Width *
                                (Height - BottomBorder + real_width() - x - 1) +
                            LeftBorder + y];
            }
        }
    }

    ~image() = default;
};

struct curl {
private:
    CURL *cURL = curl_easy_init();
    char error_buffer[CURL_ERROR_SIZE]{};

    static std::size_t writer(char *data,
                              std::size_t size,
                              std::size_t nmemb,
                              std::string *buffer) {
        buffer->append(data, size * nmemb);
        return size * nmemb;
    }

public:
    curl() = delete;

    curl(curl &&) = delete;

    curl(const curl &) = delete;

    curl &operator=(curl &&) = delete;

    curl &operator=(const curl &) = delete;

    explicit curl(const char *Url) {
        curl_easy_setopt(cURL, CURLOPT_ERRORBUFFER, error_buffer);
        curl_easy_setopt(cURL, CURLOPT_URL, Url);
    }

    void read(std::string &image_data) {
        curl_easy_setopt(cURL, CURLOPT_WRITEFUNCTION, writer);
        curl_easy_setopt(cURL, CURLOPT_WRITEDATA, &image_data);
        CURLcode p_code = curl_easy_perform(cURL);
        long code{};
        curl_easy_getinfo(cURL, CURLINFO_RESPONSE_CODE, &code);
        if (code == 404) {
            throw except("Server responded 404: page not found\n");
        }
        if (code != 200) {
            throw except("Server responded ", static_cast<int>(code), "\n");
        }
        if (p_code != CURLE_OK) {
            std::stringstream ss;
            ss << "libcurl error: " << curl_easy_strerror(p_code) << "\n";
            throw except(ss.str());
        }
    }

    ~curl() {
        curl_easy_cleanup(cURL);
    }
};
}  // namespace lab_bmp

#pragma pack(pop)
#endif  // IMAGE_H_
