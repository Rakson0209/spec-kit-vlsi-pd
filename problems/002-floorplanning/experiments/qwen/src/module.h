#pragma once

#include <string>
#include <vector>
#include <cmath>
#include <algorithm>

struct Module {
    std::string name;
    int x, y;
    int width, height;
    int min_area;
    bool fixed;

    // Candidate shapes: (width, height) pairs satisfying area >= min_area and h/w in [0.5, 2]
    std::vector<std::pair<int,int>> shapes;

    // Backup for undo
    int bk_x, bk_y, bk_w, bk_h;

    Module() : x(0), y(0), width(0), height(0), min_area(0), fixed(false),
               bk_x(0), bk_y(0), bk_w(0), bk_h(0) {}

    Module(std::string nm, int area)
        : name(nm), x(0), y(0), width(0), height(0), min_area(area), fixed(false),
          bk_x(0), bk_y(0), bk_w(0), bk_h(0) {}

    Module(std::string nm, int px, int py, int pw, int ph)
        : name(nm), x(px), y(py), width(pw), height(ph), min_area(pw*ph), fixed(true),
          bk_x(px), bk_y(py), bk_w(pw), bk_h(ph) {}

    int center_x() const { return x + width / 2; }
    int center_y() const { return y + height / 2; }
    int area() const { return width * height; }
    float aspect_ratio() const { return (width > 0) ? static_cast<float>(height) / width : 0.f; }

    bool is_valid_shape() const {
        if (width <= 0 || height <= 0) return false;
        if (width * height < min_area) return false;
        float ratio = static_cast<float>(height) / width;
        if (ratio < 0.5f - 1e-9f || ratio > 2.0f + 1e-9f) return false;
        return true;
    }

    void backup() {
        bk_x = x; bk_y = y; bk_w = width; bk_h = height;
    }

    void restore() {
        x = bk_x; y = bk_y; width = bk_w; height = bk_h;
    }

    void generate_shapes() {
        shapes.clear();
        if (min_area <= 0) { min_area = 1; }

        // min_w at h/w=2 => w*2w = min_area => w = sqrt(min_area/2)
        int min_w = static_cast<int>(std::ceil(std::sqrt(static_cast<double>(min_area) / 2.0)));
        if (min_w < 1) min_w = 1;

        // max_w at h/w=0.5 => w*0.5w = min_area => w = sqrt(2*min_area)
        int max_w = static_cast<int>(std::floor(std::sqrt(2.0 * static_cast<double>(min_area))));
        // Also bounded: w can be at most min_area (when h=1)
        max_w = std::min(max_w, min_area);

        int range = max_w - min_w;
        int step = (range < 25) ? 1 : range / 20;
        if (step < 1) step = 1;

        for (int w = min_w; w <= max_w; w += step) {
            int h = static_cast<int>(std::ceil(static_cast<double>(min_area) / w));
            // Verify constraints
            if (w * h < min_area) continue;
            float ratio = static_cast<float>(h) / w;
            if (ratio < 0.5f - 1e-9f || ratio > 2.0f + 1e-9f) continue;
            shapes.push_back(std::make_pair(w, h));
        }

        // Ensure at least one valid shape: try min_area x 1, 1 x min_area, sqrt(area) x sqrt(area)
        if (shapes.empty()) {
            int s = static_cast<int>(std::ceil(std::sqrt(static_cast<double>(min_area))));
            if (s * s >= min_area && s >= 1) {
                shapes.push_back(std::make_pair(s, s));
            }
        }
        if (shapes.empty()) {
            shapes.push_back(std::make_pair(1, min_area));
            // This might violate aspect ratio for very large areas, but it's the fallback
        }
    }
};
