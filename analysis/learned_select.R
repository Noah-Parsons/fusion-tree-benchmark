# learned_select.R — applies the Stage 1 selection rule of results/learned/PLAN.md.
#
# For each library (RadixSpline, PGM-index) and each mode (tput, lat), at
# n = 2^25: every setting's median time ÷ the faster S+ tree's, per dataset;
# the geometric mean over the four datasets; the lowest wins. Settings within
# 2% of the best go to the one using less memory (index_bytes.csv).
#
# Run: Rscript analysis/learned_select.R
dir <- "results/learned"
files <- list.files(dir, pattern = "^tune_(tput|lat)_([a-z]+)\\.csv$", full.names = TRUE)
if (!length(files)) stop("no tuning files in ", dir)
N <- 2^25

rows <- do.call(rbind, lapply(files, function(f) {
  m <- regmatches(basename(f), regexec("^tune_(tput|lat)_([a-z]+)\\.csv$", basename(f)))[[1]]
  d <- read.csv(f)
  bad <- aggregate(checksum ~ n, d, function(x) length(unique(x)))
  if (any(bad$checksum > 1)) stop("checksum mismatch in ", f)
  d <- d[d$n == N, ]
  med <- aggregate(ns_per_query ~ structure, d, median)
  best <- min(med$ns_per_query[med$structure %in% c("splus8", "splus16")])
  med$ratio <- med$ns_per_query / best
  med$mode <- m[2]; med$dataset <- m[3]
  med
}))

mem_file <- file.path(dir, "index_bytes.csv")
mem <- if (file.exists(mem_file)) aggregate(bytes_per_key ~ structure, read.csv(mem_file), mean) else NULL

gm <- aggregate(ratio ~ structure + mode, rows, function(x) exp(mean(log(x))))
names(gm)[3] <- "geomean_ratio"
if (!is.null(mem)) gm <- merge(gm, mem, all.x = TRUE)
gm$family <- ifelse(grepl("^rs", gm$structure), "RadixSpline",
             ifelse(grepl("^pgm", gm$structure), "PGM-index", "other"))
print(gm[order(gm$mode, gm$family, gm$geomean_ratio), ], row.names = FALSE, digits = 3)

cat("\nChosen settings:\n")
for (mode in c("tput", "lat")) for (fam in c("RadixSpline", "PGM-index")) {
  g <- gm[gm$mode == mode & gm$family == fam, ]
  near <- g[g$geomean_ratio <= min(g$geomean_ratio) * 1.02, ]
  pick <- if (!is.null(mem) && nrow(near) > 1) near[which.min(near$bytes_per_key), ] else g[which.min(g$geomean_ratio), ]
  cat(sprintf("  %-4s %-11s -> %s (geomean ratio to S+ %.3f)\n", mode, fam, pick$structure, pick$geomean_ratio))
}
