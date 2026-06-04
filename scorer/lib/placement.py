"""003 — 全域佈局 (Global Placement) 指標。

HPWL 由題目官方預編譯函式庫計算：呼叫 C++ helper `hpwl_eval`
（problems/003-global-placement/scorer/hpwl_eval，需先在 Linux 用 make 編譯），
載入「題目 .aux + 模型輸出 .gp.pl」後輸出官方 computeHpwl()，確保與官方一致。

合法性（是否可合法化、邊界）交給官方 verifier/verify 判定（見 score.py --verify）。
"""
from __future__ import annotations

import os
import subprocess
from typing import Optional

from .common import ScoreResult

# helper 預設路徑（相對於 repo 根）
HELPER_REL = os.path.join("problems", "003-global-placement", "scorer", "hpwl_eval")


def _find_helper(repo_root: str) -> Optional[str]:
    for cand in (os.path.join(repo_root, HELPER_REL), os.path.join(repo_root, HELPER_REL + ".exe")):
        if os.path.isfile(cand):
            return cand
    return None


def score(aux_path: str, gppl_path: str, case: str, repo_root: str) -> ScoreResult:
    r = ScoreResult(problem="003-global-placement", case=case, valid=None,
                    metric_name="hpwl", metric=None)
    helper = _find_helper(repo_root)
    if helper is None:
        r.note = ("找不到 hpwl_eval；請先在 problems/003-global-placement/scorer/ "
                  "執行 make（Linux）。")
        return r
    try:
        proc = subprocess.run([helper, aux_path, gppl_path],
                              capture_output=True, text=True, timeout=600)
    except Exception as e:  # noqa: BLE001
        r.note = f"執行 hpwl_eval 失敗：{e}"
        return r
    if proc.returncode != 0:
        r.note = f"hpwl_eval 非零退出：{proc.stderr.strip()[:200]}"
        return r
    out = proc.stdout.strip().splitlines()
    if not out:
        r.note = "hpwl_eval 無輸出"
        return r
    try:
        r.metric = float(out[-1].strip())
    except ValueError:
        r.note = f"無法解析 HPWL：{out[-1][:80]}"
    return r
