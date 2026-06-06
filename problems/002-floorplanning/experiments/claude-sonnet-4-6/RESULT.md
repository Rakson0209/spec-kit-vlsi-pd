# Result: Fixed-outline Floorplanning — claude-sonnet-4-6

**Model**: claude-sonnet-4-6
**Algorithm**: Sequence Pair + Simulated Annealing + Multi-restart (intensification/diversification)
**Compile**: `g++ -std=c++20 -O3 -o hw3 main.cpp`（專案內建 portable C++ 環境 `tools\mingw64\`）
**Run**: `./hw3 <input.txt> <output.floorplan>`
**Date**: 2026-06-05

---

## Results Table

| testcase | valid | HPWL | vs Min | vs Reference | runtime (s) |
|----------|:-----:|-----:|-------:|-------------:|------------:|
| sample   | ✅    | 215  | —      | —            | 580         |
| public1  | ✅    | 154,807,962 | **−4.2% (Beat Min)** | −35.5% | 580 |
| public2  | ✅    | 22,561,432  | +7.6% | −41.4% | 580 |
| public3  | ✅    | 1,979,508   | +6.6% | −24.5% | 580 |
| public4  | ✅    | 63,984,975  | +1.5% | −53.5% | 580 |

---

## Thresholds Reference

| testcase | Min (target ≤) | Reference | Our HPWL | Beat Min? | Beat Reference? |
|----------|---------------:|----------:|---------:|:---------:|:---------------:|
| public1  | 161,609,972    | 239,984,392 | 154,807,962 | ✅ | ✅ |
| public2  | 20,966,863     | 38,494,434  | 22,561,432  | ❌ | ✅ |
| public3  | 1,856,276      | 2,621,582   | 1,979,508   | ❌ | ✅ |
| public4  | 63,024,850     | 137,686,350 | 63,984,975  | ❌ | ✅ |

**Summary**: 4/4 testcases valid, 4/4 beat Reference, 1/4 beat Min.

---

## Gap Analysis (Constitution Principle VI)

Per constitution principle VI: if Min threshold not fully reached, record gap and reason.

- **public2**: HPWL 22,561,432 vs Min 20,966,863 (+7.6%). Gap ~1.6M.
  - Cause: SA converges to same local minimum across restarts; diversification with random shuffle of best SP may not escape this basin.
  - Mitigation tried: multi-restart (intensification + diversification), 580s full budget.

- **public3**: HPWL 1,979,508 vs Min 1,856,276 (+6.6%). Gap ~123K.
  - Cause: Same convergence issue; public3 has more modules (longer SA per restart), fewer restarts completed.

- **public4**: HPWL 63,984,975 vs Min 63,024,850 (+1.5%). Gap ~960K — very close to Min.
  - Cause: Near-optimal; further improvement likely requires better AR perturbation or B*-tree representation.

---

## Algorithm Notes

- **Sequence Pair + SA** with O(n²) packing (n ≤ 20 soft modules)
- **Multi-restart**: alternates intensification (start from best SP) and diversification (shuffle SP randomly)
- **Fixed-outline enforcement**: iterative lower-bound propagation (8 iterations) to push soft modules away from fixed modules
- **Cost function**: HPWL + λ × overflow penalty (λ = 10 × initial HPWL)
- **SA parameters**: T₀ calibrated (mean |ΔC| from 500 random moves), α=0.92, T_min=0.1, N=max(50n², 10000)
- **Time limit**: 580s per testcase
