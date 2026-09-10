#!/usr/bin/env Rscript
# analyse.R — turns the raw CSVs into the figures and tables for the report.
#
# Run from the project root:
#   Rscript analysis/analyse.R                               # results/timing_raw.csv
#   Rscript analysis/analyse.R results/timing_lat.csv lat    # any campaign, tagged
#
# Inputs :  a timing CSV (default results/timing_raw.csv), results/spread.csv
# Outputs:  figures/timing[_tag].png, figures/ratio[_tag].png, figures/spread.png
#           results/summary[_tag].csv, results/ratios[_tag].csv
#
# Only base R plus ggplot2 is used, so there is nothing exotic to install:
#   install.packages(c("ggplot2"))

suppressPackageStartupMessages(library(ggplot2))

args   <- commandArgs(trailingOnly = TRUE)
infile <- if (length(args) >= 1) args[1] else "results/timing_raw.csv"
tag    <- if (length(args) >= 2) paste0("_", args[2]) else ""
label  <- if (length(args) >= 2) sprintf(" (%s)", args[2]) else ""
dir.create("figures", showWarnings = FALSE)
set.seed(20260910)
cat(sprintf("Input: %s\n", infile))

# ---------------------------------------------------------------------------
# 0. Every structure must have produced the same checksum for a given n.
#    If not, they answered different questions and no timing is comparable.
# ---------------------------------------------------------------------------
t <- read.csv(infile)
cs <- tapply(t$checksum, t$n, function(v) length(unique(v)))
if (any(cs != 1)) {
  stop("checksums disagree across structures at n = ",
       paste(names(cs)[cs != 1], collapse = ", "))
}
cat("Checksums agree across structures at every n.\n")

# ---------------------------------------------------------------------------
# 1. Timing: median and interquartile range per (structure, n).
#
# The median is used rather than the mean because a single interrupted
# repetition can move a mean a long way and cannot move a median at all.
# The IQR is reported rather than a standard deviation for the same reason:
# it does not assume the runs are normally distributed, and they are not.
# ---------------------------------------------------------------------------
agg <- aggregate(ns_per_query ~ structure + n, data = t,
                 FUN = function(v) c(med = median(v),
                                     lo  = quantile(v, 0.25),
                                     hi  = quantile(v, 0.75),
                                     nrep = length(v)))
summary_df <- data.frame(
  structure = agg$structure,
  n         = agg$n,
  median_ns = agg$ns_per_query[, 1],
  q25_ns    = agg$ns_per_query[, 2],
  q75_ns    = agg$ns_per_query[, 3],
  reps      = agg$ns_per_query[, 4]
)
write.csv(summary_df, sprintf("results/summary%s.csv", tag), row.names = FALSE)

p1 <- ggplot(summary_df, aes(x = n, y = median_ns, colour = structure)) +
  geom_line(linewidth = 0.7) +
  geom_point(size = 1.6) +
  geom_errorbar(aes(ymin = q25_ns, ymax = q75_ns), width = 0.06, alpha = 0.6) +
  scale_x_log10() + scale_y_log10() +
  labs(x = "number of stored keys (log scale)",
       y = "nanoseconds per predecessor query (log scale)",
       title = sprintf("Predecessor query cost%s", label),
       subtitle = "median of repetitions; bars show interquartile range") +
  theme_minimal(base_size = 11)
ggsave(sprintf("figures/timing%s.png", tag), p1, width = 7, height = 4.5, dpi = 300)

# ---------------------------------------------------------------------------
# 2. Ratios: fusion time / baseline time, with a bootstrap 95% interval.
#
# The interval resamples the repetitions of each structure with replacement
# and recomputes the ratio of medians 10,000 times. No normality assumed.
# ---------------------------------------------------------------------------
baselines <- intersect(c("btree8_bl", "btree8", "sorted_array"), unique(t$structure))
B <- 10000
boot_med <- function(v) {
  m <- matrix(sample(v, B * length(v), replace = TRUE), nrow = B)
  apply(m, 1, median)
}
rows <- list()
for (b in baselines) for (nn in sort(unique(t$n))) {
  f  <- t$ns_per_query[t$structure == "fusion8" & t$n == nn]
  bb <- t$ns_per_query[t$structure == b & t$n == nn]
  r  <- boot_med(f) / boot_med(bb)
  rows[[length(rows) + 1]] <- data.frame(
    baseline = b, n = nn, ratio = median(f) / median(bb),
    lo = unname(quantile(r, 0.025)), hi = unname(quantile(r, 0.975)))
}
ratios <- do.call(rbind, rows)
write.csv(ratios, sprintf("results/ratios%s.csv", tag), row.names = FALSE)

p2 <- ggplot(ratios, aes(x = n, y = ratio, colour = baseline, fill = baseline)) +
  geom_ribbon(aes(ymin = lo, ymax = hi), alpha = 0.2, colour = NA) +
  geom_line(linewidth = 0.7) + geom_point(size = 1.6) +
  geom_hline(yintercept = 1, linetype = "dashed") +
  scale_x_log10() + scale_y_log10() +
  labs(x = "number of stored keys (log scale)",
       y = "fusion tree time / baseline time (log scale)",
       title = sprintf("Relative cost%s", label),
       subtitle = "band: bootstrap 95% interval; below the dashed line the fusion tree wins") +
  theme_minimal(base_size = 11)
ggsave(sprintf("figures/ratio%s.png", tag), p2, width = 7, height = 4.5, dpi = 300)

# Linear model of the ratio against log2(n). State the model explicitly in
# the report; do not let a fitted line masquerade as a measurement. A
# crossover is only reported when the slope's 95% interval excludes zero.
cat("\nRatio model:  ratio = a + b * log2(n)\n")
for (b in baselines) {
  d   <- ratios[ratios$baseline == b, ]
  fit <- lm(ratio ~ log2(n), data = d)
  a0  <- coef(fit)[1]; b1 <- coef(fit)[2]
  ci  <- confint(fit)[2, ]
  cat(sprintf("  vs %-12s a = %.3f  b = %+.4f  95%% CI for b [%+.4f, %+.4f]  ratio range %.2f-%.2f\n",
              b, a0, b1, ci[1], ci[2], min(d$ratio), max(d$ratio)))
  if (ci[2] < 0) {
    x <- (1 - a0) / b1
    cat(sprintf("     slope is negative; extrapolated crossover at log2(n) = %.1f\n", x))
  } else {
    cat("     slope not credibly negative; this data implies no crossover.\n")
  }
}
cat("  (A 64-bit address space holds at most 2^61 eight-byte keys.)\n")

# ---------------------------------------------------------------------------
# 3. Sketch window width against the theoretical bound.
# ---------------------------------------------------------------------------
if (file.exists("results/spread.csv")) {
  s <- read.csv("results/spread.csv")
  s <- s[s$r > 0, ]
  p3 <- ggplot(s, aes(x = factor(r), y = spread)) +
    geom_boxplot(outlier.size = 0.5) +
    geom_hline(yintercept = 64, linetype = "dashed", colour = "red") +
    labs(x = "number of important bits r",
         y = "achieved sketch window width (bits)",
         title = "How wide the multiplication sketch really is",
         subtitle = "red line: one 64-bit machine word") +
    theme_minimal(base_size = 11)
  ggsave("figures/spread.png", p3, width = 7, height = 4.5, dpi = 300)

  cat("\nWindow width by r (median / max / theoretical r^4):\n")
  for (rr in sort(unique(s$r))) {
    v <- s$spread[s$r == rr]
    cat(sprintf("  r=%d  median=%3.0f  max=%3d  bound=%d\n",
                rr, median(v), max(v), rr^4))
  }
  # Whether K fields fit depends on the node size K, not on r, so report it
  # per K as well.
  cat("\nShare of key sets whose K fields fit in one word, by node size K:\n")
  has256 <- "fields_fit256" %in% names(s)
  for (k in sort(unique(s$k))) {
    cat(sprintf("  K=%2d  64-bit word %5.1f%%", k, 100 * mean(s$fields_fit[s$k == k])))
    if (has256) cat(sprintf("   256-bit word %5.1f%%", 100 * mean(s$fields_fit256[s$k == k])))
    cat(sprintf("   median window %3.0f bits\n", median(s$spread[s$k == k])))
  }
}

cat("\nDone. Figures written to figures/.\n")
