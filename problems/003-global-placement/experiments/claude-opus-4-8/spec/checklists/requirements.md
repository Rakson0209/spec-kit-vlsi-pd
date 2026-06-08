# Specification Quality Checklist: Global Placement (HPWL Minimization)

**Purpose**: Validate specification completeness and quality before proceeding to planning
**Created**: 2026-06-08
**Feature**: [spec.md](../spec.md)

## Content Quality

- [x] No implementation details (languages, frameworks, APIs)
- [x] Focused on user value and business needs
- [x] Written for non-technical stakeholders
- [x] All mandatory sections completed

## Requirement Completeness

- [x] No [NEEDS CLARIFICATION] markers remain
- [x] Requirements are testable and unambiguous
- [x] Success criteria are measurable
- [x] Success criteria are technology-agnostic (no implementation details)
- [x] All acceptance scenarios are defined
- [x] Edge cases are identified
- [x] Scope is clearly bounded
- [x] Dependencies and assumptions identified

## Feature Readiness

- [x] All functional requirements have clear acceptance criteria
- [x] User scenarios cover primary flows
- [x] Feature meets measurable outcomes defined in Success Criteria
- [x] No implementation details leak into specification

## Notes

- The scorer (`scorer/lib/placement.py` + `bookshelf.py` + `legalize.py`) is the single source of truth (constitution R6). Legality, the anti-collapse legalizability health check, and the unweighted HPWL metric are all defined by the scorer; the spec mirrors those definitions exactly.
- Domain terms (HPWL, core, row/site, Bookshelf, terminal, pin offset) are intrinsic to the problem and the scorer's I/O contract, not implementation choices — their presence does not violate the "no implementation details" criterion.
- The single most consequential, easy-to-miss requirement is **FR-006 / SC-006** (anti-collapse): HPWL is scored on overlapping coordinates, so a tool that collapses cells would minimize HPWL but is rejected by the legalizability health check. This is surfaced in Overview, US1, Edge Cases, FR-006, and SC-006.
- All thresholds (Min/Reference/Max, ~590s runtime, 0.05× displacement limit) are taken from the problem README and the scorer source, no clarification needed.
