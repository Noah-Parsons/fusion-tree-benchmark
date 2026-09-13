# learned_confirm.R — applies the Stage 2 claim rule of results/learned/PLAN.md.
#
# In each cell (dataset × mode) at n = 2^25, ClusterJump BEATS a rival if its
# median is lower in all three sessions, LOSES if higher in all three, and
# otherwise TIES. Rivals: rs18_e16, pgm16, and the faster S+ tree.
#
# Output: results/learned_summary.csv
# Run:    Rscript analysis/learned_confirm.R
dir <- "results/learned"
pat <- "^(tput|lat)_([a-z]+)_s([0-9]+)\\.csv$"
files <- list.files(dir, pattern = pat, full.names = TRUE)
if (!length(files)) stop("no Stage 2 files in ", dir)
N <- 2^25

rows <- do.call(rbind, lapply(files, function(f) {
  m <- regmatches(basename(f), regexec(pat, basename(f)))[[1]]
  d <- read.csv(f)
  bad <- aggregate(checksum ~ n, d, function(x) length(unique(x)))
  if (any(bad$checksum > 1)) stop("checksum mismatch in ", f)
  d <- d[d$n == N, ]
  med <- setNames(aggregate(ns_per_query ~ structure, d, median)$ns_per_query,
                  aggregate(ns_per_query ~ structure, d, median)$structure)
  data.frame(mode = m[2], dataset = m[3], session = as.integer(m[4]),
             clusterjump = med[["clusterjump"]], rs18_e16 = med[["rs18_e16"]],
             pgm16 = med[["pgm16"]], best_splus = min(med[["splus8"]], med[["splus16"]]))
}))

out <- NULL
for (rival in c("rs18_e16", "pgm16", "best_splus")) {
  s <- do.call(rbind, lapply(split(rows, list(rows$mode, rows$dataset), drop = TRUE), function(g) {
    r <- g$clusterjump / g[[rival]]
    verdict <- if (nrow(g) == 3 && all(r < 1)) "beats" else if (nrow(g) == 3 && all(r > 1)) "loses" else "ties"
    data.frame(mode = g$mode[1], dataset = g$dataset[1], rival = rival, sessions = nrow(g),
               clusterjump_ns = median(g$clusterjump), rival_ns = median(g[[rival]]),
               ratio_median = median(r), ratio_min = min(r), ratio_max = max(r), verdict = verdict)
  }))
  out <- rbind(out, s)
}
out <- out[order(out$mode, out$rival, out$dataset), ]
write.csv(out, "results/learned_summary.csv", row.names = FALSE)
print(out, row.names = FALSE, digits = 3)
