# Contract: Input File Format (`.txt`)

**Used by**: Parser (Step 1 of implementation)
**Source**: `problems/002-floorplanning/benchmark/testcase/*.txt`

---

## Format

```
ChipSize <W> <H>
                        ← blank line
NumSoftModules <n>
SoftModule <name> <area>
...                     ← n SoftModule lines
                        ← blank line
NumFixedModules <m>
FixedModule <name> <x> <y> <w> <h>
...                     ← m FixedModule lines
                        ← blank line
NumNets <k>
Net <moduleA> <moduleB> <weight>
...                     ← k Net lines
```

## Field Types

| Field | Type | Constraint |
|-------|------|------------|
| W, H | positive integer | chip dimensions |
| n, m, k | non-negative integer | counts |
| name | alphanumeric string | unique across all modules |
| area | positive integer | minimum area for soft module |
| x, y | non-negative integer | fixed module bottom-left corner |
| w, h (FixedModule) | positive integer | fixed module dimensions |
| moduleA, moduleB | name string | must reference a known module (soft or fixed) |
| weight | positive integer | net weight |

## Parsing Notes

- Blank lines separate sections; parser must skip empty lines
- Tokens are whitespace-delimited on each line
- Module names are unique across soft and fixed sets
- All integers fit in 32-bit signed int
- Net endpoints may reference either soft or fixed modules
