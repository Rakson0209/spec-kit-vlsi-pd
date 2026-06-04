#!/usr/bin/env python3
"""VLSI Physical Design 批次計分器。

對某一題，掃描 testcase、配對模型輸出，重新計算最佳化指標（cut size / wirelength / HPWL）、
做合法性檢查、（選用）呼叫官方 verifier，並彙整成 stdout 表格 / Markdown / CSV。

範例：
  # 先用某實作對所有 testcase 產生輸出，再計分（--run 會自動執行實作）
  python scorer/score.py 002 --run path/to/hw3 --output-dir /tmp/fp_out --label reference

  # 已有輸出，只計分並輸出報表
  python scorer/score.py 001 --output-dir experiments/qwen3-coder/out \\
        --label qwen3-coder --md docs/001-qwen3.md --csv docs/001-qwen3.csv

  # 003 需先在 problems/003-global-placement/scorer/ 執行 make 編譯 hpwl_eval（Linux）
  python scorer/score.py 003 --run path/to/hw4 --output-dir /tmp/gp_out --label reference --verify

說明：--run 與 --verify 的程式多為 Linux 執行檔（baseline / verify 為 Linux ELF），
請在 Linux/WSL 執行該步驟；純計分（partitioning/floorplanning）可跨平台。
"""
from __future__ import annotations

import argparse
import csv as csvmod
import glob
import os
import subprocess
import sys
from typing import List, Optional

for _stream in (sys.stdout, sys.stderr):
    try:
        _stream.reconfigure(encoding="utf-8")  # Windows cp950 console 也能輸出中文/符號
    except Exception:  # noqa: BLE001
        pass

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from lib import partitioning, floorplanning, placement  # noqa: E402
from lib.common import ScoreResult  # noqa: E402

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

PROBLEMS = {
    "001-partitioning": {
        "aliases": ["001", "partition", "partitioning"],
        "out_ext": ".out", "kind": "flat_txt", "metric": "cut_size",
    },
    "002-floorplanning": {
        "aliases": ["002", "floorplan", "floorplanning"],
        "out_ext": ".floorplan", "kind": "flat_txt", "metric": "wirelength",
    },
    "003-global-placement": {
        "aliases": ["003", "placement", "global-placement", "gp"],
        "out_ext": ".gp.pl", "kind": "bookshelf", "metric": "hpwl",
    },
}


def resolve_problem(name: str) -> str:
    name = name.lower()
    for full, cfg in PROBLEMS.items():
        if name == full or name in cfg["aliases"]:
            return full
    sys.exit(f"未知題目：{name}（可用：{', '.join(PROBLEMS)} 或別名）")


def discover_cases(full: str, testcase_dir: str):
    """回傳 [(case_name, input_path)]。"""
    kind = PROBLEMS[full]["kind"]
    cases = []
    if kind == "flat_txt":
        for p in sorted(glob.glob(os.path.join(testcase_dir, "*.txt"))):
            cases.append((os.path.splitext(os.path.basename(p))[0], p))
    else:  # bookshelf：子目錄含 *.aux
        for sub in sorted(glob.glob(os.path.join(testcase_dir, "*"))):
            if os.path.isdir(sub):
                auxs = glob.glob(os.path.join(sub, "*.aux"))
                if auxs:
                    cases.append((os.path.basename(sub), auxs[0]))
    return cases


def run_impl(exe: str, input_path: str, output_path: str) -> Optional[str]:
    """執行實作產生輸出；回傳錯誤訊息或 None。"""
    os.makedirs(os.path.dirname(os.path.abspath(output_path)), exist_ok=True)
    try:
        proc = subprocess.run([exe, input_path, output_path],
                              capture_output=True, text=True, timeout=1200)
    except Exception as e:  # noqa: BLE001
        return f"執行失敗：{e}"
    if proc.returncode != 0:
        return f"非零退出 {proc.returncode}：{proc.stderr.strip()[:160]}"
    return None


def run_verifier(full: str, input_path: str, output_path: str) -> Optional[str]:
    verify = os.path.join(REPO_ROOT, "problems", full, "benchmark", "verifier", "verify")
    if not os.path.isfile(verify):
        return None
    try:
        proc = subprocess.run([verify, input_path, output_path],
                              capture_output=True, text=True, timeout=600)
    except Exception as e:  # noqa: BLE001
        return f"verify執行失敗:{e}"
    text = (proc.stdout + proc.stderr)
    if "[Success]" in text:
        return "Success"
    if "[Error]" in text:
        # 擷取第一個 Error 行
        for line in text.splitlines():
            if "[Error]" in line:
                return "Error: " + line.split("[Error]", 1)[1].strip()[:80]
        return "Error"
    return text.strip()[:80] or "?"


def score_one(full: str, case: str, input_path: str, output_path: str) -> ScoreResult:
    if not os.path.isfile(output_path):
        r = ScoreResult(problem=full, case=case, valid=None,
                        metric_name=PROBLEMS[full]["metric"], metric=None)
        r.note = "找不到輸出檔"
        return r
    try:
        if full == "001-partitioning":
            return partitioning.score(input_path, output_path, case)
        if full == "002-floorplanning":
            return floorplanning.score(input_path, output_path, case)
        return placement.score(input_path, output_path, case, REPO_ROOT)
    except Exception as e:  # noqa: BLE001
        r = ScoreResult(problem=full, case=case, valid=False,
                        metric_name=PROBLEMS[full]["metric"], metric=None)
        r.note = f"計分例外：{type(e).__name__}: {e}"
        return r


def fmt_table(results: List[ScoreResult], label: str) -> str:
    metric = results[0].metric_name if results else "metric"
    header = f"| testcase | {metric} | 合法 | verifier | 自報 | 問題 |"
    sep = "|" + "---|" * 6
    rows = [header, sep]
    for r in results:
        valid = "-" if r.valid is None else ("OK" if r.valid else "NG")
        veri = r.verifier or "-"
        selfr = "-" if r.self_reported is None else f"{r.self_reported:.0f}"
        viol = "; ".join(r.violations[:2])
        if len(r.violations) > 2:
            viol += f" (+{len(r.violations)-2})"
        if not viol:
            viol = r.note
        rows.append(f"| {r.case} | {r.metric_str()} | {valid} | {veri} | {selfr} | {viol} |")
    return f"### {label}\n\n" + "\n".join(rows) + "\n"


def main() -> None:
    ap = argparse.ArgumentParser(description="VLSI PD 批次計分器")
    ap.add_argument("problem", help="題目：001/002/003 或別名")
    ap.add_argument("--testcase-dir", help="testcase 根目錄（預設該題 benchmark/testcase）")
    ap.add_argument("--output-dir", required=True, help="模型輸出檔所在目錄（檔名 <case><ext>）")
    ap.add_argument("--run", help="實作執行檔；指定則對每個 testcase 自動執行產生輸出")
    ap.add_argument("--cases", help="只跑指定 case（逗號分隔）")
    ap.add_argument("--verify", action="store_true", help="呼叫官方 verifier/verify（Linux）")
    ap.add_argument("--label", help="此次評測標籤（預設 output-dir 名）")
    ap.add_argument("--md", help="寫出 Markdown 報表到此檔")
    ap.add_argument("--csv", help="寫出 CSV 到此檔")
    args = ap.parse_args()

    full = resolve_problem(args.problem)
    cfg = PROBLEMS[full]
    testcase_dir = args.testcase_dir or os.path.join(
        REPO_ROOT, "problems", full, "benchmark", "testcase")
    label = args.label or os.path.basename(os.path.normpath(args.output_dir))

    cases = discover_cases(full, testcase_dir)
    if args.cases:
        want = set(args.cases.split(","))
        cases = [c for c in cases if c[0] in want]
    if not cases:
        sys.exit(f"在 {testcase_dir} 找不到 testcase")

    results: List[ScoreResult] = []
    for case, input_path in cases:
        output_path = os.path.join(args.output_dir, case + cfg["out_ext"])
        if args.run:
            err = run_impl(args.run, input_path, output_path)
            if err:
                r = ScoreResult(problem=full, case=case, valid=False,
                                metric_name=cfg["metric"], metric=None)
                r.note = err
                results.append(r)
                print(f"[{case}] 執行失敗：{err}", file=sys.stderr)
                continue
        r = score_one(full, case, input_path, output_path)
        if args.verify and os.path.isfile(output_path):
            r.verifier = run_verifier(full, input_path, output_path)
        results.append(r)
        print(f"[{case}] {r.metric_name}={r.metric_str()} "
              f"valid={r.valid} verify={r.verifier or '-'}"
              + (f" :: {r.note}" if r.note else ""))

    table = fmt_table(results, label)
    print("\n" + table)

    if args.md:
        os.makedirs(os.path.dirname(os.path.abspath(args.md)), exist_ok=True)
        with open(args.md, "w", encoding="utf-8") as f:
            f.write(f"# {full} 計分報表\n\n{table}")
        print(f"已寫出 Markdown：{args.md}")
    if args.csv:
        os.makedirs(os.path.dirname(os.path.abspath(args.csv)), exist_ok=True)
        with open(args.csv, "w", encoding="utf-8", newline="") as f:
            w = csvmod.writer(f)
            w.writerow(["label", "problem", "case", "metric_name", "metric",
                        "valid", "verifier", "self_reported", "violations"])
            for r in results:
                w.writerow([label, full, r.case, r.metric_name, r.metric,
                            r.valid, r.verifier, r.self_reported,
                            "; ".join(r.violations)])
        print(f"已寫出 CSV：{args.csv}")

    # 摘要
    ok = sum(1 for r in results if r.valid)
    scored = [r for r in results if r.metric is not None]
    print(f"\n摘要：{len(results)} 個 testcase，合法 {ok}，"
          f"成功計分 {len(scored)}，標籤={label}")


if __name__ == "__main__":
    main()
