#pragma once

#include <vector>
#include <utility>
#include <algorithm>
#include <climits>

struct Rect {
    int x, y, w, h;
    Rect() : x(0), y(0), w(0), h(0) {}
    Rect(int X, int Y, int W, int H) : x(X), y(Y), w(W), h(H) {}
    int x2() const { return x + w; }
    int y2() const { return y + h; }
    bool overlaps(const Rect& o) const {
        return x < o.x2() && o.x < x2() && y < o.y2() && o.y < y2();
    }
};

// Iterative interval tree — no recursion, no dynamic allocation per node
class IntervalTree {
private:
    struct Node {
        int lo, hi, max_hi, idx;
        int left, right;  // indices; -1 = null
    };
    std::vector<Node> nodes;
    int root;

    int make_node(int lo, int hi, int idx) {
        Node n;
        n.lo = lo; n.hi = hi; n.max_hi = hi; n.idx = idx;
        n.left = -1; n.right = -1;
        nodes.push_back(n);
        return (int)nodes.size() - 1;
    }

    int update_max(int i) {
        int m = nodes[i].hi;
        if (nodes[i].left >= 0) m = std::max(m, nodes[nodes[i].left].max_hi);
        if (nodes[i].right >= 0) m = std::max(m, nodes[nodes[i].right].max_hi);
        nodes[i].max_hi = m;
        return m;
    }

public:
    IntervalTree() : root(-1) {}

    void insert(int lo, int hi, int idx) {
        // Iterative insert
        if (root < 0) {
            root = make_node(lo, hi, idx);
            return;
        }

        // Walk down to find insertion point
        std::vector<int> path;  // stack of nodes on path
        int cur = root;
        while (cur >= 0) {
            path.push_back(cur);
            if (lo <= nodes[cur].lo) {
                if (nodes[cur].left < 0) {
                    nodes[cur].left = make_node(lo, hi, idx);
                    break;
                }
                cur = nodes[cur].left;
            } else {
                if (nodes[cur].right < 0) {
                    nodes[cur].right = make_node(lo, hi, idx);
                    break;
                }
                cur = nodes[cur].right;
            }
        }

        // Update max_hi on path (bottom-up)
        for (int k = (int)path.size() - 1; k >= 0; k--)
            update_max(path[k]);
    }

    bool has_overlap(int qlo, int qhi) const {
        std::vector<int> stack;
        stack.push_back(root);
        while (!stack.empty()) {
            int i = stack.back(); stack.pop_back();
            if (i < 0) continue;
            const Node& n = nodes[i];
            if (n.max_hi <= qlo) continue;
            if (n.lo >= qlo && n.hi > qlo && n.lo < qhi) return true;
            if (n.left >= 0 && nodes[n.left].max_hi > qlo)
                stack.push_back(n.left);
            if (n.right >= 0 && nodes[n.right].lo < qhi)
                stack.push_back(n.right);
        }
        return false;
    }

    void clear() {
        nodes.clear();
        root = -1;
    }
};

class Grid {
public:
    int chip_width, chip_height;
    std::vector<Rect> placed;
    std::vector<int> placed_idx;
    IntervalTree itree_x, itree_y;

    Grid() : chip_width(0), chip_height(0) {}
    Grid(int w, int h) : chip_width(w), chip_height(h) {}

    void init(int w, int h) {
        chip_width = w;
        chip_height = h;
        placed.clear();
        placed_idx.clear();
        itree_x.clear();
        itree_y.clear();
    }

    bool in_chip(int x, int y, int w, int h) const {
        return x >= 0 && y >= 0 && x + w <= chip_width && y + h <= chip_height;
    }

    bool can_place(int x, int y, int w, int h) const {
        if (!in_chip(x, y, w, h)) return false;
        Rect test(x, y, w, h);
        for (const auto& r : placed)
            if (test.overlaps(r)) return false;
        return true;
    }

    bool has_overlap(int x, int y, int w, int h) const {
        Rect test(x, y, w, h);
        for (const auto& r : placed)
            if (test.overlaps(r)) return true;
        return false;
    }

    void add_rect(int x, int y, int w, int h, int idx = -1) {
        placed.push_back(Rect(x, y, w, h));
        placed_idx.push_back(idx);
        itree_x.insert(x, x + w, idx);
        itree_y.insert(y, y + h, idx);
    }

    void remove_at(int pi) {
        if (pi < 0 || pi >= (int)placed.size()) return;
        itree_x.clear();
        itree_y.clear();
        placed.erase(placed.begin() + pi);
        placed_idx.erase(placed_idx.begin() + pi);
        for (size_t i = 0; i < placed.size(); i++) {
            itree_x.insert(placed[i].x, placed[i].x2(), (int)i);
            itree_y.insert(placed[i].y, placed[i].y2(), (int)i);
        }
    }

    int compact_y_up(int x, int y, int w) const {
        int ny = 0;
        for (const auto& r : placed) {
            if (r.x2() <= x || r.x >= x + w) continue;
            if (r.y2() > ny) ny = r.y2();
        }
        return std::max(0, std::min(ny, y));
    }

    int compact_x_left(int y, int h, int w) const {
        int nx = 0;
        for (const auto& r : placed) {
            if (r.y2() <= y || r.y >= y + h) continue;
            if (r.x2() > nx) nx = r.x2();
        }
        return std::max(0, std::min(nx, chip_width - w));
    }

    int compact_y_down(int x, int y, int w, int h) const {
        int ny = chip_height - h;
        for (const auto& r : placed) {
            if (r.x2() <= x || r.x >= x + w) continue;
            if (r.y < ny + h) ny = r.y - h;
        }
        return std::max(0, std::min(ny, chip_height - h));
    }

    int compact_x_right(int y, int h, int w) const {
        int nx = chip_width - w;
        for (const auto& r : placed) {
            if (r.y2() <= y || r.y >= y + h) continue;
            if (r.x < nx + w) nx = r.x - w;
        }
        return std::max(0, std::min(nx, chip_width - w));
    }

    std::pair<int,int> compact_up_left(int x, int y, int w, int h) const {
        int ny = compact_y_up(x, y, w);
        int nx = compact_x_left(ny, h, w);
        return std::make_pair(nx, ny);
    }

    std::pair<int,int> compact_down_right(int x, int y, int w, int h) const {
        int ny = compact_y_down(x, y, w, h);
        int nx = compact_x_right(ny, h, w);
        return std::make_pair(nx, ny);
    }
};
