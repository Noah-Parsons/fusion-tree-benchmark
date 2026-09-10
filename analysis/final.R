#!/usr/bin/env Rscript
# final.R — the final analysis: every session, both compilers, both modes,
# both run orders, pooled.
#
# Run from the project root:
#   Rscript analysis/final.R
#
# Inputs :  results/final/<compiler>_<mode>_<order>_s<session>.csv
#           results/node_lat*.csv, results/traffic.csv, results/spread.csv
# Outputs:  results/final_summary.csv   median per (config, structure, n):
#                                        median of session medians, min, max
#           results/final_ratios.csv    the comparisons that answer the question
#           figures/final_*.png
#
# Uncertainty is shown as the RANGE across sessions: the within-run
# interquartile range was found to understate session-to-session variation
# (work log, 10 September), so the session range is the honest error bar.

suppressPackageStartupMessages(library(ggplot2))
dir.create("figures", showWarnings = FALSE)

files <- list.files("results/final", pattern = "^[a-z]+_[a-z]+_[a-z]+_s[0-9]+\\.csv$", full.names = TRUE)
if (length(files) == 0) stop("no files in results/final")
parts <- do.call(rbind, strsplit(sub("\\.csv$", "", basename(files)), "_"))
d <- do.call(rbind, lapply(seq_along(files), function(i) {
  x <- read.csv(files[i])
  x$compiler <- parts[i, 1]; x$mode <- parts[i, 2]; x$order <- parts[i, 3]
  x$session <- as.integer(sub("s", "", parts[i, 4]))
  x
}))
cat(sprintf("%d files, %d rows, sessions: %s\n", length(files), nrow(d),
            paste(sort(unique(d$session)), collapse = ",")))

# ---------------------------------------------------------------------------
# 0. Checksum gate: within a file, every structure must agree at each n.
# ---------------------------------------------------------------------------
bad <- aggregate(checksum ~ compiler + mode + order + session + n, data = d,
                 FUN = function(v) length(unique(v)))
if (any(bad$checksum != 1)) stop("checksums disagree in some file")
cat("Checksums agree across structures in every file.\n")

# ---------------------------------------------------------------------------
# 1. Session medians, then the median and range across sessions.
# ---------------------------------------------------------------------------
sm <- aggregate(ns_per_query ~ compiler + mode + order + session + structure + n,
                data = d, FUN = median)
names(sm)[names(sm) == "ns_per_query"] <- "ns"
summ <- aggregate(ns ~ compiler + mode + order + structure + n, data = sm,
                  FUN = function(v) c(med = median(v), lo = min(v), hi = max(v), k = length(v)))
summ <- data.frame(summ[, 1:5], median_ns = summ$ns[, 1], min_ns = summ$ns[, 2],
                   max_ns = summ$ns[, 3], sessions = summ$ns[, 4])
write.csv(summ, "results/final_summary.csv", row.names = FALSE)

# ---------------------------------------------------------------------------
# 2. The comparisons. Each is a ratio of session medians within one session,
#    so machine drift between sessions cancels; then median and range.
# ---------------------------------------------------------------------------
comparisons <- list(
  c("fusion8",    "btree8_bl",      "original fusion vs branch-free B-tree"),
  c("fusion8_bf", "fusion8",        "branch-free rank vs original rank"),
  c("fusion8_c",  "fusion8_bf",     "compact 128-B node vs 184-B node"),
  c("fusion8_c",  "btree8_bl_a64",  "equal footprint: compact fusion vs aligned B-tree"),
  c("fusion16_w", "btree16_bl_a64", "Exp 4: 16-key fusion vs 16-key B-tree"),
  c("fusion16_w", "fusion8_bf",     "Exp 4: 16-key fusion vs 8-key fusion"),
  c("fusion16_w", "best_btree",     "Exp 4: 16-key fusion vs best B-tree"),
  c("fusion8",    "sorted_array",   "original fusion vs binary search")
)
btrees <- c("btree8", "btree8_bl", "btree8_a64", "btree8_bl_a64", "btree16_bl_a64")
key <- c("compiler", "mode", "order", "session", "n")
wide <- reshape(sm, idvar = key, timevar = "structure", direction = "wide")
names(wide) <- sub("^ns\\.", "", names(wide))
wide$best_btree <- apply(wide[, intersect(btrees, names(wide)), drop = FALSE], 1, min)

rows <- list()
for (cp in comparisons) {
  if (!all(cp[1:2] %in% names(wide))) next
  r <- wide[, key]
  r$comparison <- cp[3]; r$num <- cp[1]; r$den <- cp[2]
  r$ratio <- wide[[cp[1]]] / wide[[cp[2]]]
  rows[[length(rows) + 1]] <- r
}
rs <- do.call(rbind, rows)
rsum <- aggregate(ratio ~ compiler + mode + order + comparison + n, data = rs,
                  FUN = function(v) c(med = median(v), lo = min(v), hi = max(v)))
rsum <- data.frame(rsum[, 1:5], ratio = rsum$ratio[, 1], lo = rsum$ratio[, 2], hi = rsum$ratio[, 3])
write.csv(rsum, "results/final_ratios.csv", row.names = FALSE)

# ---------------------------------------------------------------------------
# 3. Trend and crossover, per comparison and configuration. The slope is
#    fitted to every session's ratio, so its interval reflects session
#    variation. A crossover is extrapolated only if the whole 95% interval
#    of the slope points towards 1.0.
# ---------------------------------------------------------------------------
cat("\nratio = a + b * log2(n), fitted over all sessions\n")
trend <- list()
for (cfg in split(rs, list(rs$compiler, rs$mode, rs$order, rs$comparison), drop = TRUE)) {
  fit <- lm(ratio ~ log2(n), data = cfg)
  a0 <- coef(fit)[1]; b1 <- coef(fit)[2]; ci <- confint(fit)[2, ]
  last <- cfg[cfg$n == max(cfg$n), "ratio"]
  towards <- (mean(last) > 1 && ci[2] < 0) || (mean(last) < 1 && ci[1] > 0)
  cross <- if (towards) (1 - a0) / b1 else NA
  below <- aggregate(ratio ~ n, data = cfg, FUN = max)
  first_win <- if (any(below$ratio < 1)) min(below$n[below$ratio < 1]) else NA
  trend[[length(trend) + 1]] <- data.frame(
    compiler = cfg$compiler[1], mode = cfg$mode[1], order = cfg$order[1],
    comparison = cfg$comparison[1], a = a0, b = b1, b_lo = ci[1], b_hi = ci[2],
    ratio_at_max_n = mean(last), crossover_log2n = cross,
    first_n_below_1_every_session = first_win)
}
trend <- do.call(rbind, trend)
rownames(trend) <- NULL
write.csv(trend, "results/final_trends.csv", row.names = FALSE)
print(trend[order(trend$comparison, trend$compiler, trend$mode, trend$order),
            c("comparison", "compiler", "mode", "order", "b", "b_lo", "b_hi",
              "ratio_at_max_n", "crossover_log2n", "first_n_below_1_every_session")],
      digits = 3, row.names = FALSE)

# ---------------------------------------------------------------------------
# 4. Figures.
# ---------------------------------------------------------------------------
g <- summ[summ$compiler == "gcc" & summ$order == "shuffled", ]
p1 <- ggplot(g, aes(n, median_ns, colour = structure)) +
  geom_ribbon(aes(ymin = min_ns, ymax = max_ns, fill = structure), alpha = 0.15, colour = NA) +
  geom_line(linewidth = 0.6) + geom_point(size = 1) +
  facet_wrap(~ mode, labeller = labeller(mode = c(tput = "throughput", lat = "latency"))) +
  scale_x_log10() + scale_y_log10() +
  labs(x = "stored keys (log scale)", y = "ns per query (log scale)",
       title = "All ten structures (g++, shuffled order)",
       subtitle = "line: median of 3 sessions; band: range across sessions") +
  theme_minimal(base_size = 10)
ggsave("figures/final_timing.png", p1, width = 9, height = 4.8, dpi = 250)

main_cmp <- c("original fusion vs branch-free B-tree",
              "equal footprint: compact fusion vs aligned B-tree",
              "Exp 4: 16-key fusion vs best B-tree",
              "original fusion vs binary search")
g2 <- rsum[rsum$compiler == "gcc" & rsum$comparison %in% main_cmp, ]
p2 <- ggplot(g2, aes(n, ratio, colour = comparison, fill = comparison)) +
  geom_ribbon(aes(ymin = lo, ymax = hi), alpha = 0.2, colour = NA) +
  geom_line(linewidth = 0.6) + geom_point(size = 1) +
  geom_hline(yintercept = 1, linetype = "dashed") +
  facet_grid(order ~ mode, labeller = labeller(mode = c(tput = "throughput", lat = "latency"))) +
  scale_x_log10() + scale_y_log10() +
  labs(x = "stored keys (log scale)", y = "time ratio (log scale)",
       title = "The comparisons that answer the question (g++)",
       subtitle = "below the dashed line the fusion variant wins; band: range across 3 sessions") +
  theme_minimal(base_size = 10) + theme(legend.position = "bottom", legend.direction = "vertical")
ggsave("figures/final_ratios.png", p2, width = 9, height = 7, dpi = 250)

abl <- c("branch-free rank vs original rank", "compact 128-B node vs 184-B node",
         "Exp 4: 16-key fusion vs 8-key fusion")
g3 <- rsum[rsum$comparison %in% abl & rsum$order == "shuffled", ]
p3 <- ggplot(g3, aes(n, ratio, colour = compiler, fill = compiler)) +
  geom_ribbon(aes(ymin = lo, ymax = hi), alpha = 0.2, colour = NA) +
  geom_line(linewidth = 0.6) + geom_point(size = 1) +
  geom_hline(yintercept = 1, linetype = "dashed") +
  facet_grid(comparison ~ mode, labeller = labeller(mode = c(tput = "throughput", lat = "latency"))) +
  scale_x_log10() +
  labs(x = "stored keys (log scale)", y = "time ratio",
       title = "Ablations: what each change to the fusion node buys",
       subtitle = "below 1: the change helps; shuffled order; band: range across sessions") +
  theme_minimal(base_size = 10)
ggsave("figures/final_ablations.png", p3, width = 9, height = 7, dpi = 250)

cat("\nDone. Tables in results/final_*.csv, figures in figures/final_*.png\n")
