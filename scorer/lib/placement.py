"""003 — 全域佈局 (Global Placement) 指標與合法性（純 Python，零 Linux 依賴）。

HPWL 以純 Python 解析 bookshelf 重算（見 bookshelf.py 的座標慣例說明），
適用純 Windows 環境。此為「同一把尺」的獨立重算，用於模型間公平比較。

若要與課程官方 computeHpwl 逐位元對標，可在 Linux 編譯
problems/003-global-placement/scorer/hpwl_eval（連結官方 obj）另行比對。

合法性檢查：所有可動模組皆有座標且落在 core 範圍內；固定模組(terminal/FIXED)未被移動。
（完整「可合法化」檢查仍以課程官方 verifier 為準，本工具做基本邊界檢查。）
"""
from __future__ import annotations

from typing import List

from . import bookshelf
from .common import ScoreResult


def score(aux_path: str, gppl_path: str, case: str, repo_root: str = "") -> ScoreResult:
    r = ScoreResult(problem="003-global-placement", case=case, valid=None,
                    metric_name="hpwl", metric=None)
    try:
        files = bookshelf.parse_aux(aux_path)
        nodes = bookshelf.parse_nodes(files["nodes"])
        nets = bookshelf.parse_nets(files["nets"])
        base_pos = bookshelf.parse_pl(files["pl"]) if "pl" in files else {}
        core = bookshelf.parse_core(files["scl"]) if "scl" in files else None
    except Exception as e:  # noqa: BLE001
        r.note = f"解析 bookshelf 輸入失敗：{type(e).__name__}: {e}"
        return r

    try:
        model_pos = bookshelf.parse_pl(gppl_path)
    except Exception as e:  # noqa: BLE001
        r.note = f"解析輸出 .gp.pl 失敗：{e}"
        return r

    # 最終位置：以原始 .pl 為底，可動模組由模型輸出覆蓋
    positions = dict(base_pos)
    for name, (x, y, _fx) in model_pos.items():
        keep_fixed = base_pos.get(name, (0.0, 0.0, False))[2]
        positions[name] = (x, y, keep_fixed)

    violations: List[str] = []
    missing = 0
    moved_fixed = 0
    out_of_core = 0
    for name, (w, h, is_term) in nodes.items():
        fixed = is_term or base_pos.get(name, (0.0, 0.0, False))[2]
        if fixed:
            if name in model_pos and name in base_pos:
                bx, by, _ = base_pos[name]
                mx, my, _ = model_pos[name]
                if abs(bx - mx) > 1e-6 or abs(by - my) > 1e-6:
                    moved_fixed += 1
            continue
        if name not in model_pos:
            missing += 1
            continue
        if core is not None:
            x, y, _ = positions[name]
            xmin, ymin, xmax, ymax = core
            if x < xmin - 1e-6 or y < ymin - 1e-6 or x + w > xmax + 1e-6 or y + h > ymax + 1e-6:
                out_of_core += 1

    if missing:
        violations.append(f"{missing} 個可動模組無輸出座標")
    if moved_fixed:
        violations.append(f"{moved_fixed} 個固定模組被移動")
    if out_of_core:
        violations.append(f"{out_of_core} 個模組超出 core 範圍")

    r.metric = bookshelf.compute_hpwl(positions, nets)
    r.violations = violations
    r.valid = (len(violations) == 0)
    if core is None:
        r.note = "未取得 core 邊界，略過邊界檢查"
    return r
