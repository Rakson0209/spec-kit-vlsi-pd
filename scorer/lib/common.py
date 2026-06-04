"""共用工具：解析、結果資料結構。"""
from __future__ import annotations

from dataclasses import dataclass, field
from typing import List, Optional


@dataclass
class ScoreResult:
    """單一 (input, output) 的評測結果。"""
    problem: str
    case: str                       # testcase 名稱
    valid: Optional[bool]           # 合法性；None = 未檢查（例如 003 未跑官方 verify）
    metric_name: str                # cut_size / wirelength / hpwl
    metric: Optional[float]         # 重算之指標值；None = 算不出（檔案缺失/格式錯）
    self_reported: Optional[float] = None   # 輸出檔自報的指標（用於對照）
    verifier: Optional[str] = None  # 官方 verify 回傳摘要：Success / Error / None(未跑)
    violations: List[str] = field(default_factory=list)
    note: str = ""

    def metric_str(self) -> str:
        if self.metric is None:
            return "-"
        return f"{self.metric:.0f}" if float(self.metric).is_integer() else f"{self.metric:.3f}"


def read_token_lines(path: str) -> List[List[str]]:
    """讀檔，回傳每個非空行 tokenize 後的字串清單（空白分隔，自動略過空行）。"""
    out: List[List[str]] = []
    with open(path, "r", encoding="utf-8", errors="replace") as f:
        for line in f:
            toks = line.split()
            if toks:
                out.append(toks)
    return out
