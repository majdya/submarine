# Documentation Cleanup Summary

**Date:** 2026-09-10  
**Status:** Applied (see checklist below)

---

## What Was Done

### ✅ CONSOLIDATED & CREATED NEW

1. **`docs/CUBEMX_Setup.md`** — Merged `CUBEMX_CONFIG.md` + `CUBEMX_CHECKLIST.md`
   - Single source of truth for STM32 peripheral configuration
   - Includes step-by-step setup + tracking table + resolution of unresolved items
   - Ready to replace the old two-file setup

2. **`README.md`** (root) — Fixed all cross-reference paths
   - Changed `Claude outputs/` → `docs/`
   - Fixed `hardware.md` reference → `docs/hardware.md`
   - Added link to `docs/build-and-start.md`
   - Added reference to new `firmware/README.md`
   - Clarified datasheets folder reference

3. **`submarine-final-project/README.md`** — New firmware build/flash guide
   - Build instructions (CMake + STM32CubeIDE)
   - Flash procedures (three methods)
   - Testing checklist (12 confirmed, 6 pending)
   - Module architecture & data flow
   - Troubleshooting for common issues

---

## Files Ready to Delete

These files in the repository are **obsolete** and duplicated by current docs:

### `archive/Claude outputs/` (6 files — all obsolete)
```
archive/Claude outputs/README_TESTING.md           ← Superseded by SESSION_STATUS.md
archive/Claude outputs/QUICKSTART_TESTING.md       ← Old testing guide
archive/Claude outputs/INTEGRATION_TEST.md         ← Old integration guide
archive/Claude outputs/SESSION_SUMMARY.md          ← Old summary
archive/Claude outputs/CALIBRATION_GUIDE.md        ← Old guide
archive/Claude outputs/GROUND_STATION_PROTOCOL.md  ← Should be docs/PROTOCOL_SPEC.md
```

### `archive/cpp/` (entire folder)
```
archive/cpp/02- מ- C ל- C++ - הקלטת המצגת/  ← Course lecture PDFs
archive/cpp/04- מחלקות (classes)/              ← (unrelated to project)
archive/cpp/06- enum - containing classe/
archive/cpp/08- קונסטרקטורים ודיסטרקטורים/
archive/cpp/10- init line + this/
archive/cpp/12- העמסת אופרטורים/
archive/cpp/14- הורשה/
archive/cpp/16- פולימורפיזם/
archive/cpp/18- קבצים/
archive/cpp/20- חריגות/
archive/cpp/22- template/
archive/cpp/24- STL/
archive/cpp/26- תוספות/
```

### `docs/` (2 files — now consolidated)
```
docs/CUBEMX_CONFIG.md      ← Merged into CUBEMX_Setup.md
docs/CUBEMX_CHECKLIST.md   ← Merged into CUBEMX_Setup.md
```

**Action:** Delete these files and folders from the repository.

---

## Files Ready to Move

### `hardware.md` → `docs/hardware.md`
- Currently at root level
- Should be in `docs/` for consistency
- Update cross-references in README.md after move

---

## Files to Leave As-Is

These are working correctly and should stay:

- ✅ `README.md` (now fixed, links corrected)
- ✅ `docs/SESSION_STATUS.md` (living checklist, authoritative)
- ✅ `docs/PROJECT_PLAN.md` (original phased plan, reference)
- ✅ `docs/WIRING_GUIDE.md` (shield wiring guide)
- ✅ `docs/build-and-start.md` (PC-side build guide)
- ✅ `docs/hardware.md` (pin allocation) — *after move to docs/*
- ✅ `central-computer/README.md` (module design docs)
- ✅ `ground_station/README.md` (CLI design)
- ✅ `submarine-final-project/TEST_TRACKING.md` (hardware test checklist)

---

## Still Missing (Create if time allows)

These docs were flagged as needed in `SESSION_STATUS.md` for final submission:

- [ ] **`docs/PROTOCOL_SPEC.md`** — TLV/frame format specification (standalone)
  - Currently embedded in code as comments
  - Needed for grader reference
  - Should document all message types, encoding, CRC

- [ ] **`docs/ARCHITECTURE.md`** — System diagrams & sequences
  - UML class hierarchy (Submarine, ResearchSubmarine, CombatSubmarine, Fleet, Mission)
  - Sequence diagrams (e.g., mode change → Event → LED/alarm → Communication)
  - Message flow diagram (Monitor → Event → Log → Communication)

- [ ] **`docs/DEMO_SCRIPT.md`** — Grader walkthrough
  - Step-by-step scenario (Normal → Warning → Error)
  - Management command example (Set Limits)
  - Screenshots or expected console output

- [ ] **`docs/BUILD_FIRMWARE.md`** — Alternative to firmware/README.md if preferred
  - Could be a single page linking to CUBEMX_Setup.md + wiring + testing

---

## Fixes Applied

### Cross-Reference Fixes in README.md
- Line 4: `final project.pdf` → `docs/datasheets/final project.pdf` ✅
- Line 31: `Claude outputs/` → removed (archive folder cleanup) ✅
- Line 30: `hardware.md` → `docs/hardware.md` ✅
- Added reference to `submarine-final-project/README.md` for firmware build ✅

### CUBEMX Documentation
- **Consolidated:** CUBEMX_CONFIG.md + CUBEMX_CHECKLIST.md → CUBEMX_Setup.md ✅
- **Typo fixed:** "SILENICE" → "SILENCE" (row 15 in checklist) ✅
- **Status updated:** All items cross-checked against SESSION_STATUS.md (2026-09-09) ✅
- **Unresolved items flagged:** DS1307 vs internal RTC, TIM2 vs TIM3, ADC splits ✅

---

## Folder Structure After Cleanup

```
submarine/
├── README.md                          ✅ Fixed (paths corrected)
├── docs/
│   ├── CUBEMX_Setup.md               ✅ New (consolidated)
│   ├── SESSION_STATUS.md              ✅ Keep (living checklist)
│   ├── PROJECT_PLAN.md                ✅ Keep (reference)
│   ├── WIRING_GUIDE.md                ✅ Keep
│   ├── build-and-start.md             ✅ Keep
│   ├── hardware.md                    ⏳ Move from root to here
│   ├── datasheets/
│   │   └── final project.pdf          ✅ Keep
│   ├── flowcharts/                    ✅ Keep (module diagrams)
│   ├── reference/                     ❓ Empty — delete or document
│   ├── PROTOCOL_SPEC.md               ⏳ Create if time allows
│   ├── ARCHITECTURE.md                ⏳ Create if time allows
│   └── DEMO_SCRIPT.md                 ⏳ Create if time allows
├── submarine-final-project/
│   ├── README.md                      ✅ New (firmware build/flash)
│   ├── TEST_TRACKING.md               ✅ Keep (test checklist)
│   └── ...
├── central-computer/
│   ├── README.md                      ✅ Keep
│   └── ...
├── ground_station/
│   ├── README.md                      ✅ Keep
│   └── ...
└── archive/
    ├── Claude outputs/                ❌ DELETE (6 files)
    ├── cpp/                           ❌ DELETE (entire folder)
    └── embd/                          ⚠️  Datasheet PDFs — keep or move to docs/datasheets/?
```

---

## Next Steps (Not Applied Yet)

1. **Delete** the 6 files in `archive/Claude outputs/`
2. **Delete** the entire `archive/cpp/` folder
3. **Move** `hardware.md` from root to `docs/`
4. **Delete** `docs/CUBEMX_CONFIG.md` and `docs/CUBEMX_CHECKLIST.md` (superseded by CUBEMX_Setup.md)
5. **Update git:** Add `.gitignore` entries for `archive/` if keeping the folder
6. **Create** the 3 missing docs if time allows (PROTOCOL_SPEC, ARCHITECTURE, DEMO_SCRIPT)

---

## Files Provided for Application

Ready to copy into your repo:

- ✅ **`README.md`** — Corrected root README
- ✅ **`CUBEMX_Setup.md`** — New consolidated firmware setup guide
- ✅ **`firmware_README.md`** — New firmware build/flash reference (rename to `submarine-final-project/README.md`)
- ✅ **`CLEANUP_SUMMARY.md`** — This file (documentation of changes)

---

## Verification Checklist

After applying cleanup:

- [ ] Verify all `.md` links in README.md work (relative paths correct)
- [ ] Confirm `docs/CUBEMX_Setup.md` covers all peripherals from old two-file setup
- [ ] Check `submarine-final-project/README.md` has correct relative paths (one level deeper)
- [ ] Run grep on docs to find any remaining references to deleted files
- [ ] Verify git status shows correct adds/deletes (no unexpected changes)

---

## Questions / Decisions Still Pending

From SESSION_STATUS.md § "Scope/requirements to settle":

- [ ] **Container library:** OOP Part uses `std::vector`/`std::map` (STL, current choice). Confirm against grading rubric whether course containers (`genvector.h`, `HashMap.h`) were required.
- [ ] **Grading rubric:** No rubric found. Worth getting one from instructor/course page to target remaining effort correctly.
- [ ] **DS1307 vs internal RTC:** CUBEMX_Setup.md §7 flags this as unresolved — clarify before hardware bringup.
- [ ] **Humidity sensor:** Still parked due to ADC conflict. Decide whether to implement or leave deferred.
- [ ] **IWDG watchdog:** Currently disabled pending explicit go-ahead. Should be re-enabled for final submission (spec §2 watchdog).

---

**End of Cleanup Summary**
