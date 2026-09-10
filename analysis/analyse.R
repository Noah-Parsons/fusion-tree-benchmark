#!/usr/bin/env Rscript
# analyse.R — turns the raw CSVs into the figures and tables for the report.
#
# Run from the project root:
#   Rscript analysis/analyse.R
#
# Inputs :  results/timing_raw.csv, results/spread.csv
# Outputs:  figures/*.png, results/summary.csv
#
# Only base R plus ggplot2 is used, so there is nothing exotic to install:
#   install.packages(c("ggplot2"))

suppressPackageStartupMessages(library(ggplot2))

dir.create("figures", showWarnings = FALSE)

# ---------------------------------------------------------------------------
# 1. Timing: median and interquartile range per (structure, n).
#
# The median is used rather than the mean because a single interrupted
# repetition can move a mean a long way and cannot move a median at all.
# The IQR is reported rather than a standard deviation for the same reason:
# it does not assume the runs are normally distributed, and they are not.
# ---------------------------------------------------------------------------
t <- read.csv("results/timing_raw.csv")

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
write.csv(summary_df, "results/summary.csv", row.names = FALSE)

p1 <- ggplot(summary_df, aes(x = n, y = median_ns, colour = structure)) +
  geom_line(linewidth = 0.7) +
  geom_point(size = 1.6) +
  geom_errorbar(aes(ymin = q25_ns, ymax = q75_ns), width = 0.06, alpha = 0.6) +
  scale_x_log10() +
  labs(x = "number of stored keys (log scale)",
       y = "nanoseconds per predecessor query",
       title = "Predecessor query cost",
       subtitle = "median of repetitions; bars show interquartile range") +
  theme_minimal(base_size = 11)
ggsave("figures/timing.png", p1, width = 7, height = 4.5, dpi = 300)

# ---------------------------------------------------------------------------
# 2. Ratio plot. This is the figure that carries the argument: if the ratio
#    is falling with n, extrapolating it tells you where a crossover would be.
# ---------------------------------------------------------------------------
wide <- reshape(summary_df[, c("structure", "n", "median_ns")],
                idvar = "n", timevar = "structure", direction = "wide")
names(wide) <- sub("median_ns\\.", "", names(wide))

if (all(c("fusion8", "btree8") %in% names(wide))) {
  wide$ratio <- wide$fusion8 / wide$btree8
  p2 <- ggplot(wide, aes(x = n, y = ratio)) +
    geom_line(linewidth = 0.7) + geom_point(size = 1.6) +
    geom_hline(yintercept = 1, linetype = "dashed") +
    scale_x_log10() +
    labs(x = "number of stored keys (log scale)",
         y = "fusion tree time / B-tree time",
         title = "Relative cost",
         subtitle = "the dashed line is the crossover; below it the fusion tree wins") +
    theme_minimal(base_size = 11)
  ggsave("figures/ratio.png", p2, width = 7, height = 4.5, dpi = 300)

  # Log-linear extrapolation of the ratio against log2(n).
  # State the model explicitly in the report; do not let a fitted line
  # masquerade as a measurement.
  fit <- lm(ratio ~ log2(n), data = wide)
  cat("\nRatio model:  ratio = a + b * log2(n)\n")
  print(summary(fit)$coefficients)
  a <- coef(fit)[1]; b <- coef(fit)[2]
  if (b < 0) {
    crossing_log2n <- (1 - a) / b
    cat(sprintf("\nExtrapolated crossover at log2(n) = %.1f  (n = 2^%.1f)\n",
                crossing_log2n, crossing_log2n))
    cat("A 64-bit address space holds at most 2^61 eight-byte keys.\n")
  } else {
    cat("\nRatio is not decreasing; no crossover is implied by this data.\n")
  }
}

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
}

cat("\nDone. Figures written to figures/.\n")
