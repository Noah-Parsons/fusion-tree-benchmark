"""rmi_grid.py — turn the RMI optimizer's Pareto list into a ranked training grid.

    python3 rmi_grid.py <opt.json> <dataset> <seed> <ranks|all> <grid_out.json> <manifest.csv>

The optimizer (rmi <keys> --optimize opt.json) writes {"configs": [...]}, each
entry with "layers", "branching factor" and "size". Entries are ordered by
size, smallest first, and numbered: rank 1, 2, ... For the requested ranks
this writes a --param-grid file whose namespaces are
rmi_<dataset>_<seed>_r<rank>, and appends one row per RMI to the manifest
(replacing any earlier row for the same namespace).
"""
import csv
import json
import os
import sys


def main():
    opt, dataset, seed, ranks, grid_out, manifest = sys.argv[1:7]
    configs = json.load(open(opt))["configs"]
    configs = sorted(configs, key=lambda c: (c["size"], c["layers"], c["branching factor"]))
    wanted = None if ranks == "all" else {int(r) for r in ranks.split(",")}

    grid, rows = [], []
    for rank, c in enumerate(configs, start=1):
        if wanted is not None and rank not in wanted:
            continue
        ns = f"rmi_{dataset}_{seed}_r{rank}"
        grid.append({"layers": c["layers"], "branching factor": c["branching factor"],
                     "namespace": ns, "binary": True})
        rows.append({"namespace": ns, "dataset": dataset, "seed": seed, "rank": rank,
                     "layers": c["layers"], "branching": c["branching factor"],
                     "optimizer_size_bytes": c["size"],
                     "optimizer_avg_log2_error": c.get("average log2 error", "")})
    json.dump({"configs": grid}, open(grid_out, "w"), indent=1)

    fields = ["namespace", "dataset", "seed", "rank", "layers", "branching",
              "optimizer_size_bytes", "optimizer_avg_log2_error"]
    old = []
    if os.path.exists(manifest):
        new_ns = {r["namespace"] for r in rows}
        old = [r for r in csv.DictReader(open(manifest, newline="")) if r["namespace"] not in new_ns]
    with open(manifest, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=fields, lineterminator="\n")
        w.writeheader()
        w.writerows(old + rows)
    print(f"{dataset} {seed}: {len(grid)} RMIs in {grid_out} (of {len(configs)} on the Pareto list)")


if __name__ == "__main__":
    main()
