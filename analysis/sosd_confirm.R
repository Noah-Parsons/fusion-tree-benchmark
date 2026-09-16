# sosd_confirm.R — applies the Stage 2 claim rule of results/sosd_rivals/PLAN.md.
#
#   Rscript analysis/sosd_confirm.R cht256_e32 rmi_r7 [more rivals...]
#
# In each cell (dataset × mode) at n = 2^25, ClusterJump BEATS a rival if its
# median is lower in all three sessions, LOSES if higher in all three, and
# otherwise TIES. Rivals: those named, plus rs18_e16, pgm16 and the faster S+
# tree. Writes results/sosd_rivals_summary.csv.
args <- commandArgs(trailingOnly = TRUE)
rivals <- c(args, "rs18_e16", "pgm16", "best_splus")
dir <- "results/sosd_rivals"
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
  a <- aggregate(ns_per_query ~ structure, d, median)
  v <- setNames(a$ns_per_query, a$structure)
  v["best_splus"] <- min(v[["splus8"]], v[["splus16"]])
  data.frame(mode = m[2], dataset = m[3], session = as.integer(m[4]),
             structure = names(v), ns = unname(v))
}))

out <- NULL
for (rival in rivals) {
  for (g in split(rows, list(rows$mode, rows$dataset), drop = TRUE)) {
    cj <- g[g$structure == "clusterjump", c("session", "ns")]
    rv <- g[g$structure == rival, c("session", "ns")]
    if (!nrow(rv)) next
    s <- merge(cj, rv, by = "session", suffixes = c("_cj", "_rv"))
    r <- s$ns_cj / s$ns_rv
    verdict <- if (nrow(s) == 3 && all(r < 1)) "beats" else if (nrow(s) == 3 && all(r > 1)) "loses" else "ties"
    out <- rbind(out, data.frame(mode = g$mode[1], dataset = g$dataset[1], rival = rival, sessions = nrow(s),
                                 clusterjump_ns = median(s$ns_cj), rival_ns = median(s$ns_rv),
                                 ratio_median = median(r), ratio_min = min(r), ratio_max = max(r), verdict = verdict))
  }
}
out <- out[order(out$mode, out$rival, out$dataset), ]
write.csv(out, "results/sosd_rivals_summary.csv", row.names = FALSE)
print(out, row.names = FALSE, digits = 3)
