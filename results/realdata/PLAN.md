# Real datasets: plan, fixed before any real-data timing was seen

Written 2026-09-12, after the four files downloaded and before any structure
was timed on them.

## Question

Do the Stage 2 results (results/spline_pilot/PLAN.md) hold on real data?
Does `clusterjump` still beat the S+ tree?

## Data

SOSD benchmark (Kipf, Marcus et al.), Harvard Dataverse doi:10.7910/DVN/JGVF9A,
200 million uint64 keys each, kept in `data/` (not in git):

| file | what the keys are | md5 |
|---|---|---|
| wiki_ts_200M_uint64 | Wikipedia edit timestamps | 4f1402b1… (matches SOSD) |
| fb_200M_uint64 | Facebook user IDs | 3b0f820c… (matches SOSD) |
| books_200M_uint64 | Amazon book popularity | aeedc7be… (no published checksum; zstd frame check passed) |
| osm_cellids_200M_uint64 | OpenStreetMap cell IDs | a7f6b8d2… (no published checksum; zstd frame check passed) |

Duplicates are removed. Each size samples its keys at random from the file.
Each query lands at a random point between two neighbouring stored keys, so
queries follow the data's own shape.

## Runs

- Structures: `sorted_array`, `btree8_bl`, `splus8`, `splus16`, `spline32`,
  `clusterjump`. Nothing is tuned; all parameters are as in Stage 2.
- 4 datasets × throughput/latency × 3 sessions (seeds 20260910–12), 15
  repetitions, 2^8 to 2^25 keys, core 8, shuffled order, g++.

## Claim rule (unchanged)

A candidate beats the S+ tree in a cell (dataset × mode) at size n only if its
median is below the faster S+ tree's in all three sessions. Every cell is
reported, wins and losses.

## Expectations, written before running

- `fb` is the hard case for radix methods: a few huge outlier IDs stretch the
  key range, so most keys crowd into one top bucket. `clusterjump`'s second
  level should absorb that (the crowded bucket gets its own fine table), but
  if the keys inside that bucket are themselves uneven it will slow down.
- `osm` cell IDs are clustered at several scales, which is the known weakness
  (clustering at one scale only). Expect `clusterjump` to lose ground there.
- `spline32` has an error bound that holds for any shape, so its latency win
  should survive on all four; its throughput loss should too.

## Outcome

17:43–18:03 on 12 September, all 24 runs complete, checksums agree.
`Rscript analysis/confirm.R results/realdata results/realdata_summary.csv`.
At 2^25, candidate ÷ faster S+ tree, median of three sessions [lowest, highest]:

| dataset | clusterjump throughput | clusterjump latency | spline32 throughput | spline32 latency |
|---|---|---|---|---|
| books | **0.43** [0.43, 0.44] | **0.95** [0.95, 0.96] | 1.32 | **0.88** [0.87, 0.90] |
| wiki | **0.43** [0.43, 0.43] | **0.95** [0.93, 0.98] | 1.20 | **0.82** [0.79, 0.84] |
| fb | **0.64** [0.64, 0.65] | 1.00 [0.99, 1.00] * | 1.85 | 1.27 |
| osm | **0.60** [0.60, 0.61] | 1.00 [0.98, 1.01] | 1.73 | 1.17 |

Bold = below 1 in all three sessions. * fb latency passes the rule (below 1
in every session) but by 0.2%; it is reported as a tie, not a win.

Against the expectations:
- `clusterjump` throughput held on all four, including fb and osm, but with
  a smaller margin there (0.60–0.64 against 0.43), as expected.
- `clusterjump` latency: a small win on books and wiki, a tie on fb and osm.
- **`spline32` did worse than expected.** Its latency win held on books and
  wiki but turned into a loss on fb (1.27) and osm (1.17). Its error bound
  still holds (every answer is correct), but on these two datasets the shape
  needs many more pieces, so the piece search itself no longer fits in cache.
  That last sentence is a hypothesis, not a measurement.
