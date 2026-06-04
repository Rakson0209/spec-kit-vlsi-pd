"""001 — 多技術晶粒切割 (Die Partitioning) 指標與合法性。

指標：cut size = 同時有 cell 落在 DieA 與 DieB 的 net 數量（越小越好）。
合法性：每個晶粒的已用面積使用率 area/(W*H) <= util；所有 cell 恰被分配一次。
注意：同一 cell 在不同 die（不同 Tech）有不同面積，計算各 die 面積時須用該 die 的 Tech 尺寸。
"""
from __future__ import annotations

from typing import Dict, List, Tuple

from .common import ScoreResult, read_token_lines


def _parse_input(path: str):
    lines = read_token_lines(path)
    i = 0
    # techs: tech_name -> { libcell_name -> area }
    techs: Dict[str, Dict[str, float]] = {}
    num_techs = int(lines[i][1]); i += 1
    for _ in range(num_techs):
        _, tname, ncell = lines[i][0], lines[i][1], int(lines[i][2]); i += 1
        cells: Dict[str, float] = {}
        for _ in range(ncell):
            _, lname, w, h = lines[i][:4]; i += 1
            cells[lname] = float(w) * float(h)
        techs[tname] = cells
    # DieSize W H
    assert lines[i][0] == "DieSize", f"expect DieSize, got {lines[i]}"
    die_w, die_h = float(lines[i][1]), float(lines[i][2]); i += 1
    # DieA tech util%
    _, dieA_tech, dieA_util = lines[i][0], lines[i][1], float(lines[i][2]) / 100.0; i += 1
    _, dieB_tech, dieB_util = lines[i][0], lines[i][1], float(lines[i][2]) / 100.0; i += 1
    # cells
    assert lines[i][0] == "NumCells", f"expect NumCells, got {lines[i]}"
    ncells = int(lines[i][1]); i += 1
    cell_lib: Dict[str, str] = {}
    for _ in range(ncells):
        _, cname, lname = lines[i][:3]; i += 1
        cell_lib[cname] = lname
    # nets
    assert lines[i][0] == "NumNets", f"expect NumNets, got {lines[i]}"
    nnets = int(lines[i][1]); i += 1
    nets: List[List[str]] = []
    for _ in range(nnets):
        _, nname, deg = lines[i][0], lines[i][1], int(lines[i][2]); i += 1
        net_cells = []
        for _ in range(deg):
            _, cname = lines[i][:2]; i += 1
            net_cells.append(cname)
        nets.append(net_cells)
    return {
        "techs": techs, "die_area": die_w * die_h,
        "dieA_tech": dieA_tech, "dieA_util": dieA_util,
        "dieB_tech": dieB_tech, "dieB_util": dieB_util,
        "cell_lib": cell_lib, "nets": nets,
    }


def _parse_output(path: str) -> Tuple[float, List[str], List[str]]:
    lines = read_token_lines(path)
    i = 0
    self_cut = None
    if lines[i][0].lower() == "cutsize":
        self_cut = float(lines[i][1]); i += 1
    assert lines[i][0] == "DieA", f"expect DieA, got {lines[i]}"
    na = int(lines[i][1]); i += 1
    dieA = [lines[i + k][0] for k in range(na)]; i += na
    assert lines[i][0] == "DieB", f"expect DieB, got {lines[i]}"
    nb = int(lines[i][1]); i += 1
    dieB = [lines[i + k][0] for k in range(nb)]; i += nb
    return self_cut, dieA, dieB


def score(input_path: str, output_path: str, case: str) -> ScoreResult:
    r = ScoreResult(problem="001-partitioning", case=case, valid=None,
                    metric_name="cut_size", metric=None)
    inp = _parse_input(input_path)
    self_cut, dieA, dieB = _parse_output(output_path)
    r.self_reported = self_cut

    assign: Dict[str, str] = {}
    for c in dieA:
        assign[c] = "A"
    for c in dieB:
        assign[c] = "B"

    violations: List[str] = []
    # 覆蓋性檢查
    missing = [c for c in inp["cell_lib"] if c not in assign]
    extra = [c for c in assign if c not in inp["cell_lib"]]
    dup = len(dieA) + len(dieB) - len(set(dieA) | set(dieB))
    if missing:
        violations.append(f"{len(missing)} 個 cell 未分配 (如 {missing[:3]})")
    if extra:
        violations.append(f"{len(extra)} 個未知 cell (如 {extra[:3]})")
    if dup:
        violations.append(f"{dup} 個 cell 重複分配")

    # 面積使用率（各 die 用各自 Tech 尺寸）
    techs = inp["techs"]
    areaA = sum(techs[inp["dieA_tech"]][inp["cell_lib"][c]] for c in dieA
                if c in inp["cell_lib"] and inp["cell_lib"][c] in techs[inp["dieA_tech"]])
    areaB = sum(techs[inp["dieB_tech"]][inp["cell_lib"][c]] for c in dieB
                if c in inp["cell_lib"] and inp["cell_lib"][c] in techs[inp["dieB_tech"]])
    utilA = areaA / inp["die_area"] if inp["die_area"] else float("inf")
    utilB = areaB / inp["die_area"] if inp["die_area"] else float("inf")
    eps = 1e-9
    if utilA > inp["dieA_util"] + eps:
        violations.append(f"DieA 使用率 {utilA:.4f} > 上限 {inp['dieA_util']:.4f}")
    if utilB > inp["dieB_util"] + eps:
        violations.append(f"DieB 使用率 {utilB:.4f} > 上限 {inp['dieB_util']:.4f}")

    # cut size：net 同時碰到 A 與 B
    cut = 0
    for net in inp["nets"]:
        sides = {assign.get(c) for c in net}
        if "A" in sides and "B" in sides:
            cut += 1

    r.metric = float(cut)
    r.violations = violations
    r.valid = (len(violations) == 0)
    r.note = f"utilA={utilA:.3f} utilB={utilB:.3f}"
    return r
