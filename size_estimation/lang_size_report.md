# Binary Size Report — Multi-Language Support

> Target: **flex** · Chain: **ethereum**

## 1. Binary Sizes

| Configuration | ROM (KB) | ΔROM (KB) | ΔROM (%) | RAM (KB) | ΔRAM |
|---|---:|---:|---:|---:|---:|
| Baseline (EN only) | 165.33 | — | — | 21.58 | — |
| EN + FR (French) | 168.33 | +3.00 | +1.81% | 21.58 | — |
| EN + FR + ES (+ Spanish) | 171.33 | +6.00 | +3.63% | 21.58 | — |
| EN + FR + ES + DE (+ German) | 174.33 | +9.00 | +5.44% | 21.58 | — |


## 2. Estimated Cost for N Additional Languages

Observed average marginal cost (FR / ES / DE): **3.00 KB/language**

| N languages | Estimated ROM (KB) | Total ΔROM | ΔROM (%) |
|---:|---:|---:|---:|
| 1 | ~168.33 | +3.00 KB | +1.81% |
| 2 | ~171.33 | +6.00 KB | +3.63% |
| 3 | ~174.33 | +9.00 KB | +5.44% |
| 5 | ~180.32 | +15.00 KB | +9.07% |
| 10 | ~195.33 | +30.00 KB | +18.15% |
| 20 | ~225.33 | +60.00 KB | +36.29% |

## 3. ELF Section Breakdown

| Configuration | text (KB) | data (KB) | bss (KB) | ROM = text+data | RAM = data+bss |
|---|---:|---:|---:|---:|---:|
| Baseline (EN only) | 165.33 | 0.00 | 21.58 | 165.33 KB | 21.58 KB |
| EN + FR | 168.33 | 0.00 | 21.58 | 168.33 KB | 21.58 KB |
| EN + FR + ES  | 171.33 | 0.00 | 21.58 | 171.33 KB | 21.58 KB |
| EN + FR + ES + DE  | 174.33 | 0.00 | 21.58 | 174.33 KB | 21.58 KB |

## 4. Saved Artifacts

All files are stored under `size_estimation/artifacts/`.

| Configuration | ELF | Map |
|---|---|---|
| Baseline (EN only) | `app_baseline_en.elf` | `app_baseline_en.map` |
| EN + FR  | `app_with_fr.elf` | `app_with_fr.map` |
| EN + FR + ES | `app_with_fr_es.elf` | `app_with_fr_es.map` |
| EN + FR + ES + DE | `app_with_fr_es_de.elf` | `app_with_fr_es_de.map` |

## 5. SDK-Level Strings

The sections above only measure the **app-layer** string tables.
The Ledger Secure SDK (`lib_nbgl/nbgl_use_case.c`) also hardcodes strings that
are emitted at run-time by the use-case helpers the app calls.  The script
`size_estimation/scripts/sdk_strings_estimation.py` scans all `nbgl_useCase*`
call sites in `src/` and maps them to the corresponding SDK strings.

### 5.1 Use Cases Called by the App

| Function | Call sites | SDK strings hardcoded? |
|---|---:|---|
| `nbgl_useCaseAdvancedReview` | 4 | ✅ Yes (39 strings) |
| `nbgl_useCaseReview` | 4 | ✅ Yes (15 strings) |
| `nbgl_useCaseAddressReview` | 3 | ✅ Yes (8 strings) |
| `nbgl_useCaseReviewStatus` | 16 | ✅ Yes (8 strings) |
| `nbgl_useCaseChoice` | 3 | ➖ App-provided only |
| `nbgl_useCaseStatus` | 3 | ➖ App-provided only |
| `nbgl_useCaseAction` | 1 | ➖ App-provided only |
| `nbgl_useCaseHomeAndSettings` | 1 | ➖ App-provided only |
| `nbgl_useCaseSpinner` | 1 | ➖ App-provided only |

### 5.2 SDK String Breakdown (per use case)

| Use case | Unique strings | Raw bytes |
|---|---:|---:|
| `nbgl_useCaseAdvancedReview` | 39 | 998 B |
| `nbgl_useCaseAddressReview` | 8 | 166 B |
| `nbgl_useCaseReview` | 15 | 213 B |
| `nbgl_useCaseReviewStatus` | 8 | 156 B |
| **Global unique (deduplicated)** | **55** | **1 320 B (1.29 KB)** |

> Note: strings shared across multiple use cases are counted once.

### 5.3 Full-Stack Cost per Language

| Layer | Strings | Cost / language |
|---|---:|---:|
| App layer (`src/lang_*.c`) — measured | 78 | **3.00 KB** |
| SDK layer (`nbgl_use_case.c`) — estimated | 55 | **1.29 KB** |
| **Full stack** | **133** | **4.29 KB** |

### 5.4 Full-Stack Extrapolation

| Languages | App layer only (KB) | Full stack (KB) |
|---:|---:|---:|
| 3 | 9.00 | 12.87 |
| 5 | 15.00 | 21.45 |
| 10 | 30.00 | 42.89 |
| 20 | 60.00 | 85.78 |

> The SDK cost is **an estimate** — it is borne by the firmware build, not the
> app binary, so it does not appear in the ELF measurements above.  To measure
> it precisely a firmware build comparison would be required.

---

## 6. Conclusions

- The current Ethereum app uses **165.33 KB of ROM** (target: flex).
- Adding one language at the **app layer** costs on average **+3.00 KB** of ROM (+1.81%).
- Adding one language at the full **app + SDK** stack costs approximately **+4.29 KB**.
- With 5 additional languages: **+15.00 KB** (app only) or **+21.45 KB** (full stack).


