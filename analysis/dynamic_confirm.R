# dynamic_confirm.R — applies the claim rule of results/dynamic/PLAN.md.
#
# In each cell (key shape × insert share), ClusterJumpD BEATS a rival if its
# median ns per operation is lower in all three sessions, LOSES if higher in
# all three, and otherwise TIES. Also reports memory per key at the end.
#
# Input:  results/dynamic/<shape>_w<pct>_s<session>.csv (bench_dyn output)
# Output: results/dynamic_summary.csv
# Run:    Rscript analysis/dynamic_confirm.R
dir <- "results/dynamic"
pat <- "^([a-z]+)_w([0-9]+)_s([0-9]+)\\.csv$"
files <- list.files(dir, pattern = pat, full.names = TRUE)
if (!length(files)) stop("no result files in ", dir)

rows <- do.call(rbind, lapply(files, function(f) {
  m <- regmatches(basename(f), regexec(pat, basename(f)))[[1]]
  d <- read.csv(f)
  # Every structure ran the same operations: one checksum per file.
  if (length(unique(d$checksum)) != 1)
    stop("checksums disagree in ", f, ": ", paste(unique(paste(d$structure, d$checksum)), collapse = "; "))
  w <- as.integer(m[3])
  keys_end <- d$n[1] / 2 + (d$n[1] / 2) * w / 100
  a <- aggregate(cbind(ns_per_op, bytes_end) ~ structure, d, median)
  a$bytes_per_key <- a$bytes_end / keys_end
  a$shape <- m[2]; a$write_pct <- w; a$session <- as.integer(m[4])
  a
}))

out <- NULL
for (rival in c("tlx_btree", "alex")) {
  for (g in split(rows, list(rows$shape, rows$write_pct), drop = TRUE)) {
    cj <- g[g$structure == "clusterjump_d", ]
    rv <- g[g$structure == rival, ]
    s <- merge(cj, rv, by = "session", suffixes = c("_cj", "_rv"))
    r <- s$ns_per_op_cj / s$ns_per_op_rv
    verdict <- if (nrow(s) == 3 && all(r < 1)) "beats" else if (nrow(s) == 3 && all(r > 1)) "loses" else "ties"
    out <- rbind(out, data.frame(
      shape = g$shape[1], write_pct = g$write_pct[1], rival = rival, sessions = nrow(s),
      clusterjump_ns = median(s$ns_per_op_cj), rival_ns = median(s$ns_per_op_rv),
      ratio_median = median(r), ratio_min = min(r), ratio_max = max(r), verdict = verdict,
      clusterjump_bytes_per_key = median(s$bytes_per_key_cj), rival_bytes_per_key = median(s$bytes_per_key_rv)))
  }
}
out <- out[order(out$rival, out$write_pct, out$shape), ]
write.csv(out, "results/dynamic_summary.csv", row.names = FALSE)
print(out, row.names = FALSE, digits = 3)
