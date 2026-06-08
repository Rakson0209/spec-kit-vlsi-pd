# I/O Format Contract: Bookshelf in → `.gp.pl` out

**Plan**: [../plan.md](../plan.md) | **Source of truth**: [`scorer/lib/bookshelf.py`](../../../../../../scorer/lib/bookshelf.py), [`legalize.py`](../../../../../../scorer/lib/legalize.py)

All parsers MUST skip blank lines, `#` comments, and the leading `UCLA ...` header, and tokenize on whitespace (matches `bookshelf._data_lines`).

## Input

### `.aux`
```
RowBasedPlacement : public1.nodes public1.nets public1.wts public1.pl public1.scl
```
Resolve each listed file **relative to the `.aux`'s directory** (by extension). (Scorer: `parse_aux`.)

### `.nodes` — `<name> <w> <h> [terminal]`
```
NumNodes : 12028
NumTerminals : 0
        a0   1056.0   504.0
        ...
```
- Skip `NumNodes` / `NumTerminals` lines. `terminal` (4th token, prefix-match) ⇒ **fixed**.

### `.pl` — `<name> <x> <y> : <orient> [/FIXED]`  (x,y = **lower-left**)
```
        a0          0          0 : N
```
- `/FIXED` (any case, leading `/` stripped) ⇒ fixed. Initial movable coords are all `(0,0)` (must be replaced — they are collapsed/out-of-core).

### `.nets` — `NetDegree : k` then `k` pin lines `<node> <I/O> : <xoff> <yoff>`
```
NetDegree : 3
        a10828   I : 88 252
        ...
```
- `xoff,yoff` are offsets relative to the cell's **lower-left**. Pin global = `(cell.x+xoff, cell.y+yoff)`. (Std-cell height 504 ⇒ `yoff≈252=h/2`.) Skip `NumNets`/`NumPins`.

### `.scl` — `CoreRow … End` blocks
```
CoreRow Horizontal
 Coordinate   : -33208      # row bottom y
 Height       : 504         # row height (== movable cell height)
 Sitewidth    : 66
 Sitespacing  : 66          # site pitch
 SubrowOrigin : -33330  NumSites : 1011   # left x ; site count
End
```
- Core = `( min SubrowOrigin, min Coordinate, max(SubrowOrigin+NumSites·Sitespacing), max(Coordinate+Height) )`. **Coordinates may be negative.** (Scorer: `parse_core` / `parse_rows`.)

### `.wts` — `<net-or-node> <weight>`
- Present, but the scored HPWL **ignores weights** (research §3). Read optionally as a search hint only.

## Output `.gp.pl`

Bookshelf placement format, parseable by `bookshelf.parse_pl` (reads tokens `name x y` then `:`):

```
UCLA pl 1.0

        a0      12345.0     -3300.0 : N
        a1      12480.0     -3300.0 : N
        ...
```

| Rule | Why |
|------|-----|
| One line per **movable** cell: `<name> <x> <y> : N` | FR-008; movable coverage mandatory (FR-003) |
| `x,y` = **lower-left** = `centerX − w/2`, `centerY − h/2` | scorer reads lower-left (FR-004) |
| Emitting fixed cells is optional; if emitted, coords MUST equal input | FR-005 (scorer flags moved fixed > `1e-6`) |
| Header `UCLA pl 1.0` line allowed (skipped by parser); blank lines/comments allowed | `_data_lines` skips them |
| Coordinates may be real numbers | scorer parses `float` |

### Scoring recomputation (what the output is judged on)
1. Overlay output onto base `.pl`; movable coords ← output, fixed ← base.
2. **Legality**: all movable present (no missing); all movable in-core; no fixed moved; **legalizability health** — single-row Tetris spread avg disp `≤ 0.05×min(coreW,coreH)`, not aborted at `0.15×`.
3. **Metric**: unweighted `Σ_nets (max−min)x + (max−min)y` over pin global coords (overlapping placement).

> `valid=OK` requires **all** legality checks to pass; the metric only counts once legal.
