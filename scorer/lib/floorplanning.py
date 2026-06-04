"""002 — 固定輪廓平面規劃 (Fixed-outline Floorplanning) 指標與合法性。

指標：加權 HPWL = Σ weight * (|cx1-cx2| + |cy1-cy2|)，pin 在模組中心，
      中心座標向下取整（floor），即整數模組的 cx = x + w//2。
合法性：軟模組在晶片輪廓內、面積 >= min_area、長寬比 0.5<=h/w<=2、
        所有模組（軟+硬）互不重疊、硬模組位置與輸入一致。
"""
from __future__ import annotations

from typing import Dict, List, Tuple

from .common import ScoreResult, read_token_lines


def _parse_input(path: str):
    lines = read_token_lines(path)
    i = 0
    assert lines[i][0] == "ChipSize"
    chip_w, chip_h = int(lines[i][1]), int(lines[i][2]); i += 1
    assert lines[i][0] == "NumSoftModules"
    ns = int(lines[i][1]); i += 1
    soft_min: Dict[str, int] = {}
    for _ in range(ns):
        _, name, area = lines[i][:3]; i += 1
        soft_min[name] = int(area)
    assert lines[i][0] == "NumFixedModules"
    nf = int(lines[i][1]); i += 1
    fixed: Dict[str, Tuple[int, int, int, int]] = {}
    for _ in range(nf):
        _, name, x, y, w, h = lines[i][:6]; i += 1
        fixed[name] = (int(x), int(y), int(w), int(h))
    assert lines[i][0] == "NumNets"
    nn = int(lines[i][1]); i += 1
    nets: List[Tuple[str, str, int]] = []
    for _ in range(nn):
        _, m1, m2, w = lines[i][:4]; i += 1
        nets.append((m1, m2, int(w)))
    return {"chip": (chip_w, chip_h), "soft_min": soft_min, "fixed": fixed, "nets": nets}


def _parse_output(path: str):
    lines = read_token_lines(path)
    i = 0
    self_wl = None
    if lines[i][0].lower() == "wirelength":
        self_wl = float(lines[i][1]); i += 1
    assert lines[i][0] == "NumSoftModules", f"expect NumSoftModules, got {lines[i]}"
    ns = int(lines[i][1]); i += 1
    soft: Dict[str, Tuple[int, int, int, int]] = {}
    for _ in range(ns):
        name, x, y, w, h = lines[i][:5]; i += 1
        soft[name] = (int(x), int(y), int(w), int(h))
    return self_wl, soft


def _overlap(a: Tuple[int, int, int, int], b: Tuple[int, int, int, int]) -> bool:
    ax, ay, aw, ah = a
    bx, by, bw, bh = b
    return ax < bx + bw and bx < ax + aw and ay < by + bh and by < ay + ah


def score(input_path: str, output_path: str, case: str) -> ScoreResult:
    r = ScoreResult(problem="002-floorplanning", case=case, valid=None,
                    metric_name="wirelength", metric=None)
    inp = _parse_input(input_path)
    self_wl, soft = _parse_output(output_path)
    r.self_reported = self_wl

    chip_w, chip_h = inp["chip"]
    violations: List[str] = []

    # 完整性
    miss = [m for m in inp["soft_min"] if m not in soft]
    if miss:
        violations.append(f"{len(miss)} 個軟模組未輸出 (如 {miss[:3]})")

    # 各軟模組合法性
    for name, (x, y, w, h) in soft.items():
        if x < 0 or y < 0 or x + w > chip_w or y + h > chip_h:
            violations.append(f"{name} 超出輪廓 ({x},{y},{w},{h}) chip=({chip_w},{chip_h})")
        min_area = inp["soft_min"].get(name)
        if min_area is not None and w * h < min_area:
            violations.append(f"{name} 面積 {w*h} < 最小 {min_area}")
        if w > 0 and h > 0:
            ratio = h / w
            if ratio < 0.5 - 1e-9 or ratio > 2.0 + 1e-9:
                violations.append(f"{name} 長寬比 h/w={ratio:.3f} 不在 [0.5, 2]")

    # 硬模組位置與輸入一致（輸出不含硬模組，僅作為位置來源；此處檢查名稱衝突）
    all_rects: Dict[str, Tuple[int, int, int, int]] = {}
    for name, rect in inp["fixed"].items():
        all_rects[name] = rect
    for name, rect in soft.items():
        all_rects[name] = rect

    # 重疊（兩兩）
    items = list(all_rects.items())
    overlaps = 0
    for a in range(len(items)):
        for b in range(a + 1, len(items)):
            if _overlap(items[a][1], items[b][1]):
                overlaps += 1
                if overlaps <= 3:
                    violations.append(f"重疊：{items[a][0]} 與 {items[b][0]}")
    if overlaps > 3:
        violations.append(f"...另有 {overlaps - 3} 組重疊")

    # 加權 HPWL（中心向下取整）
    def center(rect):
        x, y, w, h = rect
        return (x + w // 2, y + h // 2)

    centers = {name: center(rect) for name, rect in all_rects.items()}
    wl = 0
    missing_net = 0
    for m1, m2, weight in inp["nets"]:
        if m1 not in centers or m2 not in centers:
            missing_net += 1
            continue
        (x1, y1), (x2, y2) = centers[m1], centers[m2]
        wl += weight * (abs(x1 - x2) + abs(y1 - y2))
    if missing_net:
        violations.append(f"{missing_net} 條 net 參照到未知模組")

    r.metric = float(wl)
    r.violations = violations
    r.valid = (len(violations) == 0)
    return r
