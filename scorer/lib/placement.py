"""003 — 全域佈局 (Global Placement) 指標與合法性（純 Python，零 Linux 依賴）。

HPWL 以純 Python 解析 bookshelf 重算（見 bookshelf.py 的座標慣例說明），適用純 Windows 環境。
此為「同一把尺」的獨立重算（重疊態 global HPWL，對齊本題 README 的 Min/Reference/Max 門檻表），
用於模型間公平比較。

合法性檢查（valid=OK 需全數通過）：
  1. 所有可動模組皆有輸出座標（無遺漏）。
  2. 可動模組落在 core 範圍內。
  3. 固定模組（terminal / FIXED）未被移動。
  4. 「可合法化健康度」：把模型輸出實際 legalize（攤平到 row/site，見 legalize.py），
     量測攤平所需的平均位移。真實展開解位移極小（≈0.01×coreSize）；塌縮解（cell 全疊一點）
     位移巨大（≥0.15×coreSize）。位移超門檻 → 判定疑似塌縮 / 不可合法化 → NG。
     這封住「把所有 cell 疊一點得近零 HPWL 卻仍通過基本邊界檢查」的漏洞。

  指標（metric）始終是重疊態 global HPWL（不受 legalize 影響，維持與門檻表可比）；
  legalized HPWL 與平均位移僅作報告（見 note 欄）。

若要與課程官方 computeHpwl 逐位元對標，可在 Linux 編譯
problems/003-global-placement/scorer/hpwl_eval（連結官方 obj）另行比對。
"""
from __future__ import annotations

from typing import List

from . import bookshelf, legalize
from .common import ScoreResult

# —— 可合法化健康度門檻（可調）——
# legalize 後「平均位移 / min(coreW, coreH)」上限；超過視為疑似塌縮 / 不可合法化。
# 實證：真實展開解 ≈0.006~0.011，塌縮解 ≥0.15，故 0.05 對真實解有 ~5× 安全邊界。
HEALTH_DISP_LIMIT = 0.05
# legalize 進行中，平均位移達此倍率即早停（明顯塌縮，省時且直接判 NG）。
ABORT_DISP_NORM = 0.15


def score(aux_path: str, gppl_path: str, case: str, repo_root: str = "") -> ScoreResult:
    r = ScoreResult(problem="003-global-placement", case=case, valid=None,
                    metric_name="hpwl", metric=None)
    try:
        files = bookshelf.parse_aux(aux_path)
        nodes = bookshelf.parse_nodes(files["nodes"])
        nets = bookshelf.parse_nets(files["nets"])
        base_pos = bookshelf.parse_pl(files["pl"]) if "pl" in files else {}
        core = bookshelf.parse_core(files["scl"]) if "scl" in files else None
        rows = legalize.parse_rows(files["scl"]) if "scl" in files else []
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
    mov_pos = {}   # name -> (x, y) 可動模組最終位置（供 legalize）
    mov_size = {}  # name -> (w, h)
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
        x, y, _ = positions[name]
        if core is not None:
            xmin, ymin, xmax, ymax = core
            if x < xmin - 1e-6 or y < ymin - 1e-6 or x + w > xmax + 1e-6 or y + h > ymax + 1e-6:
                out_of_core += 1
        mov_pos[name] = (x, y)
        mov_size[name] = (w, h)

    if missing:
        violations.append(f"{missing} 個可動模組無輸出座標")
    if moved_fixed:
        violations.append(f"{moved_fixed} 個固定模組被移動")
    if out_of_core:
        violations.append(f"{out_of_core} 個模組超出 core 範圍")

    # 指標：重疊態 global HPWL（維持與門檻表可比，不受 legalize 影響）
    r.metric = bookshelf.compute_hpwl(positions, nets)

    # 可合法化健康度：實際 legalize，量平均位移
    notes: List[str] = []
    if core is None:
        notes.append("未取得 core 邊界，略過邊界與健康度檢查")
    elif not rows:
        notes.append("未解析出 row，略過健康度檢查")
    elif mov_pos:
        xmin, ymin, xmax, ymax = core
        minside = min(xmax - xmin, ymax - ymin)
        lg = legalize.legalize(list(mov_pos), mov_pos, mov_size, rows, minside, ABORT_DISP_NORM)
        norm = (lg["avg_disp"] / minside) if minside > 0 else 0.0
        if lg["aborted"]:
            violations.append(
                f"疑似塌縮/不可合法化：legalize 早停（平均位移已達 {norm:.3f}×core ≥ {ABORT_DISP_NORM}）")
            notes.append(f"legalize=aborted avgDisp≈{norm:.3f}×core")
        else:
            lpos = dict(positions)
            for n, (x, y) in lg["placed"].items():
                lpos[n] = (x, y, False)
            legal_hpwl = bookshelf.compute_hpwl(lpos, nets)
            if norm > HEALTH_DISP_LIMIT:
                violations.append(
                    f"疑似塌縮/不可合法化：legalize 平均位移 {norm:.3f}×core > 門檻 {HEALTH_DISP_LIMIT}")
            notes.append(f"legalHPWL={legal_hpwl:.0f} avgDisp={norm:.3f}×core")

    r.violations = violations
    r.valid = (len(violations) == 0)
    r.note = "; ".join(notes)
    return r
