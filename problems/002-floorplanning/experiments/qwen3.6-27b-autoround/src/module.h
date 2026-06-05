#ifndef MODULE_H
#define MODULE_H

#include <string>
#include <vector>
#include <utility>
#include <cmath>

struct Module {
    std::string name;
    int x, y, w, h;

    Module() : x(0), y(0), w(0), h(0) {}
    Module(const std::string& n, int xx, int yy, int ww, int hh)
        : name(n), x(xx), y(yy), w(ww), h(hh) {}

    int center_x() const { return x + w / 2; }
    int center_y() const { return y + h / 2; }
    int area() const { return w * h; }
};

struct SoftModule : Module {
    int min_area;
    std::vector<std::pair<int,int>> shapes; // (w, h) candidates
    int temp_x, temp_y, temp_w, temp_h;

    SoftModule() : min_area(0), temp_x(0), temp_y(0), temp_w(0), temp_h(0) {}
    SoftModule(const std::string& n, int ma) : Module(n, 0, 0, 0, 0), min_area(ma),
        temp_x(0), temp_y(0), temp_w(0), temp_h(0) {
        generate_shapes();
    }

    void generate_shapes() {
        shapes.clear();
        if (min_area <= 0) return;
        int min_w = (int)std::ceil(std::sqrt(min_area * 0.5));
        int max_w = (int)std::floor(std::sqrt(min_area * 2.0));
        if (min_w < 1) min_w = 1;
        int step = 1;
        if (max_w - min_w > 20)
            step = (max_w - min_w) / 20;
        for (int w = min_w; w <= max_w; w += step) {
            int h = (int)std::ceil((double)min_area / w);
            // check aspect ratio h/w in [0.5, 2.0]
            if (w > 0) {
                double ratio = (double)h / w;
                if (ratio >= 0.5 - 1e-9 && ratio <= 2.0 + 1e-9) {
                    shapes.push_back({w, h});
                }
            }
        }
        // ensure at least one shape
        if (shapes.empty()) {
            int w = (int)std::sqrt(min_area);
            int h = (int)std::ceil((double)min_area / w);
            shapes.push_back({w, h});
        }
    }

    bool valid_aspect_ratio() const {
        if (w <= 0) return false;
        double ratio = (double)h / w;
        return ratio >= 0.5 - 1e-9 && ratio <= 2.0 + 1e-9;
    }

    bool valid_area() const {
        return w * h >= min_area;
    }

    void backup() {
        temp_x = x; temp_y = y; temp_w = w; temp_h = h;
    }
    void restore() {
        x = temp_x; y = temp_y; w = temp_w; h = temp_h;
    }
};

struct FixedModule : Module {
    FixedModule() : Module() {}
    FixedModule(const std::string& n, int xx, int yy, int ww, int hh)
        : Module(n, xx, yy, ww, hh) {}
};

#endif
