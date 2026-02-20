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

## 5. Conclusions

- The current Ethereum app uses **165.33 KB of ROM** (target: flex).
- Adding one language costs on average **+3.00 KB** of ROM (+1.81%).
- With 5 additional languages the total overhead would be approximately **+15.00 KB** (+9.07%).


