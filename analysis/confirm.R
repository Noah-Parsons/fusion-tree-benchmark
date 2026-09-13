# confirm.R — applies the Stage 2 claim rule of results/spline_pilot/PLAN.md.
#
# For every session, key set (uniform/clustered), mode (tput/lat) and size,
# take each structure's median over repetitions. A candidate "beats the S+
# tree" in a cell at size n only if its median is below the faster of splus8
# and splus16 in EVERY session.
#
# Input files: <dir>/<mode>_<keys>_s<session>.csv (bench_pred output)
# Output:      results/spline_confirm_summary.csv   medians, ratios, verdicts
#
# Run: Rscript analysis/confirm.R results/spline_confirm
#      Rscript analysis/confirm.R results/realdata results/realdata_summary.csv
args <- commandArgs(trailingOnly = TRUE)
dir <- if (length(args) >= 1) args[1] else "results/spline_confirm"
outfile <- if (length(args) >= 2) args[2] else "results/spline_confirm_summary.csv"
pat <- "^(tput|lat)_([A-Za-z0-9]+)_s([0-9]+)\\.csv$"   # keys: uniform, clustered, wiki, fb, ...
files <- list.files(dir, pattern = pat, full.names = TRUE)
if (!length(files)) stop("no result files in ", dir)

rows <- do.call(rbind, lapply(files, function(f) {
  m <- regmatches(basename(f), regexec(pat, basename(f)))[[1]]
  d <- read.csv(f)
  # Every structure must have answered the same queries.
  bad <- aggregate(checksum ~ n, d, function(x) length(unique(x)))
  if (any(bad$checksum > 1)) stop("checksum mismatch in ", f, " at n = ", paste(bad$n[bad$checksum > 1], collapse = ","))
  med <- aggregate(ns_per_query ~ structure + n, d, median)
  med$mode <- m[2]; med$keys <- m[3]; med$session <- as.integer(m[4])
  med
}))

wide <- reshape(rows, idvar = c("mode", "keys", "session", "n"), timevar = "structure", direction = "wide")
names(wide) <- sub("^ns_per_query\\.", "", names(wide))
wide$best_splus <- pmin(wide$splus8, wide$splus16)

out <- NULL
for (cand in c("clusterjump", "spline32")) {
  wide$ratio <- wide[[cand]] / wide$best_splus
  s <- do.call(rbind, lapply(split(wide, list(wide$mode, wide$keys, wide$n), drop = TRUE), function(g) data.frame(
    candidate = cand, mode = g$mode[1], keys = g$keys[1], n = g$n[1], sessions = nrow(g),
    candidate_ns = median(g[[cand]]), best_splus_ns = median(g$best_splus),
    ratio_median = median(g$ratio), ratio_min = min(g$ratio), ratio_max = max(g$ratio),
    beats_splus = nrow(g) == 3 && all(g$ratio < 1))))
  out <- rbind(out, s)
}
out <- out[order(out$candidate, out$mode, out$keys, out$n), ]
write.csv(out, outfile, row.names = FALSE)

big <- out[out$n == 2^25, c("candidate", "mode", "keys", "candidate_ns", "best_splus_ns", "ratio_median", "ratio_min", "ratio_max", "beats_splus")]
print(big, row.names = FALSE, digits = 3)
