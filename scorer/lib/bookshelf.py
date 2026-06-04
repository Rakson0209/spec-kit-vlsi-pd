"""Bookshelf 格式解析與 HPWL 計算（純 Python，零 Linux 依賴）。

座標慣例（依本題 testcase 實證）：
- .nodes 行：<name> <w> <h> [terminal]
- .pl   行：<name> <x> <y> : <orient> [/FIXED]   (x,y 為模組左下角)
- .nets pin 行：<node> <I/O> : <xoff> <yoff>     (offset 相對模組左下角；
  證據：standard cell height=504 時 yoff 恆=252=h/2=cell 垂直中央)
- pin 全域座標 = (node.x + xoff, node.y + yoff)
- HPWL(net) = (max pin_x - min pin_x) + (max pin_y - min pin_y)
- 總 HPWL = Σ HPWL(net)

注意：此為獨立重算，定義與標準 bookshelf 一致，用於「模型間公平比較」（同一把尺）。
若要與課程官方 computeHpwl 逐位元對標，需在 Linux 用官方預編譯庫（見 hpwl_eval.cpp）。
"""
from __future__ import annotations

import os
from typing import Dict, List, Tuple


def _data_lines(path: str):
    """逐行 yield tokens，略過註解(#)、空行與 UCLA 標頭行。"""
    with open(path, "r", encoding="utf-8", errors="replace") as f:
        for line in f:
            s = line.strip()
            if not s or s.startswith("#") or s.startswith("UCLA"):
                continue
            yield s.split()


def parse_aux(aux_path: str) -> Dict[str, str]:
    """讀 .aux，回傳副檔名 -> 絕對路徑（同目錄）。"""
    base = os.path.dirname(os.path.abspath(aux_path))
    files: Dict[str, str] = {}
    with open(aux_path, "r", encoding="utf-8", errors="replace") as f:
        content = f.read()
    # 形如 "RowBasedPlacement : a.nodes a.nets a.wts a.pl a.scl"
    for tok in content.replace(":", " ").split():
        if "." in tok:
            ext = tok.rsplit(".", 1)[1].lower()
            files[ext] = os.path.join(base, tok)
    return files


def parse_nodes(path: str) -> Dict[str, Tuple[float, float, bool]]:
    """回傳 {name: (width, height, is_terminal)}。"""
    nodes: Dict[str, Tuple[float, float, bool]] = {}
    for t in _data_lines(path):
        if t[0] in ("NumNodes", "NumTerminals"):
            continue
        if len(t) < 3:
            continue
        name, w, h = t[0], float(t[1]), float(t[2])
        is_term = len(t) >= 4 and t[3].lower().startswith("terminal")
        nodes[name] = (w, h, is_term)
    return nodes


def parse_pl(path: str) -> Dict[str, Tuple[float, float, bool]]:
    """回傳 {name: (x, y, is_fixed)}；x,y 為模組左下角。"""
    pos: Dict[str, Tuple[float, float, bool]] = {}
    for t in _data_lines(path):
        if len(t) < 3:
            continue
        name = t[0]
        try:
            x, y = float(t[1]), float(t[2])
        except ValueError:
            continue
        is_fixed = any(tok.upper().lstrip("/") == "FIXED" for tok in t[3:])
        pos[name] = (x, y, is_fixed)
    return pos


def parse_nets(path: str) -> List[List[Tuple[str, float, float]]]:
    """回傳 nets 清單，每個 net = [(node, xoff, yoff), ...]。"""
    nets: List[List[Tuple[str, float, float]]] = []
    cur: List[Tuple[str, float, float]] = None  # type: ignore
    for t in _data_lines(path):
        if t[0] in ("NumNets", "NumPins"):
            continue
        if t[0] == "NetDegree":
            cur = []
            nets.append(cur)
            continue
        if cur is None:
            continue
        # <node> <I/O> : <xoff> <yoff>
        node = t[0]
        xoff = yoff = 0.0
        if ":" in t:
            ci = t.index(":")
            if len(t) > ci + 2:
                try:
                    xoff, yoff = float(t[ci + 1]), float(t[ci + 2])
                except ValueError:
                    xoff = yoff = 0.0
        cur.append((node, xoff, yoff))
    return nets


def parse_core(scl_path: str):
    """從 .scl 的 CoreRow 推算 core 邊界 (xmin, ymin, xmax, ymax)；失敗回 None。"""
    try:
        ys: List[float] = []
        y_tops: List[float] = []
        x_origins: List[float] = []
        x_rights: List[float] = []
        coord = height = spacing = None
        for t in _data_lines(scl_path):
            key = t[0]
            if key == "Coordinate" and ":" in t:
                coord = float(t[t.index(":") + 1])
            elif key == "Height" and ":" in t:
                height = float(t[t.index(":") + 1])
            elif key == "Sitespacing" and ":" in t:
                spacing = float(t[t.index(":") + 1])
            elif key == "SubrowOrigin":
                # SubrowOrigin : <x>  NumSites : <n>
                origin = float(t[t.index(":") + 1])
                nsites = float(t[-1])
                x_origins.append(origin)
                if spacing is not None:
                    x_rights.append(origin + nsites * spacing)
                if coord is not None:
                    ys.append(coord)
                if coord is not None and height is not None:
                    y_tops.append(coord + height)
        if not x_origins or not ys:
            return None
        return (min(x_origins), min(ys), max(x_rights), max(y_tops))
    except Exception:  # noqa: BLE001
        return None


def compute_hpwl(positions: Dict[str, Tuple[float, float, bool]],
                 nets: List[List[Tuple[str, float, float]]]) -> float:
    """以 pin 全域座標 (node.x+xoff, node.y+yoff) 計算總 HPWL。"""
    total = 0.0
    for net in nets:
        xs: List[float] = []
        ys: List[float] = []
        for name, xoff, yoff in net:
            p = positions.get(name)
            if p is None:
                continue
            xs.append(p[0] + xoff)
            ys.append(p[1] + yoff)
        if xs:
            total += (max(xs) - min(xs)) + (max(ys) - min(ys))
    return total
