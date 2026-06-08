"""003 全域佈局 — 純 Python row-based legalizer（單列 Tetris）與「可合法化健康度」量測。

為什麼需要這支：
  global placement 允許 cell 重疊，HPWL 以「重疊態」計分（對齊官方門檻表 Min/Reference/Max，
  見 hpwl_eval.cpp / bookshelf.compute_hpwl）。但這留下漏洞——把所有可動 cell 塌縮到同一點
  可得近零 HPWL，卻仍滿足「在 core 內、固定未移動」的基本邊界檢查。

  本模組把模型輸出實際 legalize（攤平到 .scl 的 row/site、消除重疊），量測「攤平所需的平均位移」：
    - 真實展開解：cell 已大致就位，legalize 僅微調 → 平均位移 ≈ 0.006~0.011 × min(coreW, coreH)，
      且 legalized HPWL ≈ global HPWL（實證 R≈1.00）。
    - 塌縮解：cell 全擠一處，legalize 必須攤到整個 core → 平均位移 ≥ 0.15 × min(coreW, coreH)。
  兩者相差 ~15×，據此判定健康度。HPWL 指標本身維持 global（不受影響）；本模組只供 placement.py
  判合法性與報告 legalized 成本。

  關鍵前提（已對三個 public 測資實證）：所有可動 cell 高度 == row 高度（單列高），故每個 cell
  恰好佔一個 row，legalization 退化為「選 row + row 內排序」的 Tetris。
"""
from __future__ import annotations

import bisect
from typing import Dict, List, Tuple

from . import bookshelf


def parse_rows(scl_path: str) -> List[dict]:
    """逐 CoreRow 解析，回傳 [{y, h, x0, x1, sp}]（依出現順序）。

    y=row 底部座標、h=row 高、x0=row 左界、x1=row 右界、sp=site spacing。
    解析不出任何 row 時回傳空 list。
    """
    rows: List[dict] = []
    coord = height = spacing = sitewidth = None
    for t in bookshelf._data_lines(scl_path):
        key = t[0]
        if key == "Coordinate" and ":" in t:
            coord = float(t[t.index(":") + 1])
        elif key == "Height" and ":" in t:
            height = float(t[t.index(":") + 1])
        elif key == "Sitespacing" and ":" in t:
            spacing = float(t[t.index(":") + 1])
        elif key == "Sitewidth" and ":" in t:
            sitewidth = float(t[t.index(":") + 1])
        elif key == "SubrowOrigin":
            origin = float(t[t.index(":") + 1])
            nsites = float(t[-1])
            sp = spacing if spacing else (sitewidth if sitewidth else 1.0)
            rows.append({"y": coord, "h": height, "x0": origin,
                         "x1": origin + nsites * sp, "sp": sp})
    return rows


def legalize(names: List[str],
             pos: Dict[str, Tuple[float, float]],
             size: Dict[str, Tuple[float, float]],
             rows: List[dict],
             minside: float,
             abort_norm: float = 0.15) -> dict:
    """單列 Tetris legalize。

    參數：
      names    — 要 legalize 的可動 cell 名稱清單
      pos      — {name: (x, y)} 模型給的（重疊態）左下角座標
      size     — {name: (w, h)} cell 尺寸
      rows     — parse_rows() 結果
      minside  — min(coreW, coreH)，用於位移正規化與早停
      abort_norm — 放置過程中「平均位移/minside」超過此值即早停（塌縮快速偵測）

    回傳 dict：
      placed   — {name: (x, y)} 攤平後座標
      avg_disp — 平均位移（與輸入同單位）
      max_disp — 最大位移
      aborted  — 是否早停（明顯塌縮 / legalize 無法在合理位移內完成）
      n        — 實際放置數
    """
    if not rows or not names:
        return {"placed": {}, "avg_disp": 0.0, "max_disp": 0.0, "aborted": False, "n": 0}

    rows = sorted(rows, key=lambda r: r["y"])
    row_y = [r["y"] for r in rows]
    cur = [r["x0"] for r in rows]      # 各 row 下一個可用 x（cursor）
    x1 = [r["x1"] for r in rows]
    nrow = len(rows)

    order = sorted(names, key=lambda n: pos[n][0])  # 按 target x 掃描
    placed: Dict[str, Tuple[float, float]] = {}
    sum_disp = 0.0
    max_disp = 0.0
    cnt = 0

    for name in order:
        tx, ty = pos[name][0], pos[name][1]
        w = size[name][0]
        ri0 = bisect.bisect_left(row_y, ty)
        if ri0 >= nrow:
            ri0 = nrow - 1

        best_disp = float("inf")
        best = None
        radius = 0
        while radius < nrow:
            for ri in ({ri0 - radius, ri0 + radius} if radius else {ri0}):
                if ri < 0 or ri >= nrow:
                    continue
                vd = abs(row_y[ri] - ty)
                if vd >= best_disp:
                    continue
                px = cur[ri] if cur[ri] > tx else tx
                if px + w > x1[ri]:
                    continue  # 此 row 已滿
                d = abs(px - tx) + vd
                if d < best_disp:
                    best_disp = d
                    best = (ri, px)
            lo, hi = ri0 - radius, ri0 + radius
            vlo = abs(row_y[lo] - ty) if lo >= 0 else float("inf")
            vhi = abs(row_y[hi] - ty) if hi < nrow else float("inf")
            if best is not None and min(vlo, vhi) >= best_disp:
                break  # 垂直位移已不可能再小，剪枝
            if lo < 0 and hi >= nrow:
                break  # 掃完所有 row
            radius += 1

        if best is None:
            # 全 core 無空位（高 util 碎片）：放「剩餘空間最多」的 row，允許溢出，繼續（不中止）
            ri = min(range(nrow), key=lambda i: cur[i])
            best = (ri, cur[ri])

        ri, px = best
        placed[name] = (px, row_y[ri])
        cur[ri] = px + w
        d = abs(px - tx) + abs(row_y[ri] - ty)
        sum_disp += d
        if d > max_disp:
            max_disp = d
        cnt += 1
        if cnt >= 500 and sum_disp / cnt > abort_norm * minside:
            return {"placed": placed, "avg_disp": sum_disp / cnt,
                    "max_disp": max_disp, "aborted": True, "n": cnt}

    return {"placed": placed, "avg_disp": sum_disp / max(cnt, 1),
            "max_disp": max_disp, "aborted": False, "n": cnt}
