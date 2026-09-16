# sosd_select.R — applies the Stage 1 selection rule of results/sosd_rivals/PLAN.md.
#
# For each library (CHT, RMI) and each mode (tput, lat), at n = 2^25: every
# setting's median time ÷ the faster S+ tree's, per dataset; the geometric
# mean over the four datasets; the lowest wins. Settings within 2% of the best
# go to the one using less memory. An RMI setting is a rank on each dataset's
# Pareto list; a rank counts only if an RMI was matched for all four datasets
# (checked by its bytes being larger than the keys alone).
#
# Run: Rscript analysis/sosd_select.R
dir <- "results/sosd_rivals"
# Stage 1 runs in groups of at most 8 structures (tune_<mode>_<dataset>_g<k>.csv),
# each with both S+ trees, so every ratio is taken within one run.
pat <- "^tune_(tput|lat)_([a-z]+)_g[0-9]+\\.csv$"
files <- list.files(dir, pattern = pat, full.names = TRUE)
if (!length(files)) stop("no tuning files in ", dir)
N <- 2^25

rows <- do.call(rbind, lapply(files, function(f) {
  m <- regmatches(basename(f), regexec(pat, basename(f)))[[1]]
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

gm <- aggregate(ratio ~ structure + mode, rows, function(x) exp(mean(log(x))))
names(gm)[3] <- "geomean_ratio"
cnt <- aggregate(ratio ~ structure + mode, rows, length)
names(cnt)[3] <- "datasets"
gm <- merge(gm, cnt)
gm$family <- ifelse(grepl("^cht", gm$structure), "CHT", ifelse(grepl("^rmi", gm$structure), "RMI", "other"))

mem_file <- file.path(dir, "index_bytes.csv")
if (file.exists(mem_file)) {
  mem <- aggregate(bytes_per_key ~ structure, read.csv(mem_file), mean)
  gm <- merge(gm, mem, all.x = TRUE)
  # An RMI rank with no matched RMI on some dataset has index bytes == keys only.
  unmatched <- read.csv(mem_file)
  unmatched <- unique(unmatched$structure[grepl("^rmi", unmatched$structure) & unmatched$bytes_per_key <= 8.001])
  gm <- gm[!(gm$structure %in% unmatched), ]
}
gm <- gm[gm$datasets == 4, ]
print(gm[order(gm$mode, gm$family, gm$geomean_ratio), ], row.names = FALSE, digits = 3)

cat("\nChosen settings:\n")
for (mode in c("tput", "lat")) for (fam in c("CHT", "RMI")) {
  g <- gm[gm$mode == mode & gm$family == fam, ]
  if (!nrow(g)) { cat(sprintf("  %-4s %-3s -> none available\n", mode, fam)); next }
  near <- g[g$geomean_ratio <= min(g$geomean_ratio) * 1.02, ]
  pick <- if ("bytes_per_key" %in% names(near) && nrow(near) > 1) near[which.min(near$bytes_per_key), ] else g[which.min(g$geomean_ratio), ]
  cat(sprintf("  %-4s %-3s -> %s (geomean ratio to S+ %.3f)\n", mode, fam, pick$structure, pick$geomean_ratio))
}
