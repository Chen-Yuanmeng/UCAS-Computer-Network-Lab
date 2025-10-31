
SHELL := /bin/bash

# directory containing this Makefile (absolute, ends with /)
BASEDIR := $(dir $(abspath $(firstword $(MAKEFILE_LIST))))

# Default qlen (can override with `make reproduce qlen=xxx`)
qlen ?= 10

PY := python3
REPRO_PY := reproduce_bufferbloat.py
EXTRACT_PY := extract_results.py
PLOT_PY := plot_bufferbloat.py
COMPARE_PY := compare_algos.py

OUTDIR := outputs/qlen-$(qlen)

.PHONY: reproduce extract plot clean help

help:
	@echo "Usage: make reproduce qlen=XXX"
	@echo "Targets:"
	@echo "  reproduce  - run experiment (sudo), extract CSV and plot (default qlen=$(qlen))"
	@echo "  extract    - extract CSV only for qlen=$(qlen)"
	@echo "  plot       - plot only (reads CSV from $(OUTDIR))"
	@echo "  clean      - remove outputs/"

reproduce:  ## Run experiment, extract results and plot (may require sudo for Mininet)
	@echo "[1/3] Running reproduce_bufferbloat.py with maxq=$(qlen) (sudo may be requested)"
	sudo $(PY) $(BASEDIR)$(REPRO_PY) --maxq $(qlen)
	@echo "[2/3] Extracting CSVs"
	$(PY) $(BASEDIR)$(EXTRACT_PY) qlen-$(qlen)
	@echo "[3/3] Plotting"
	$(PY) $(BASEDIR)$(PLOT_PY) -d $(OUTDIR) -o $(OUTDIR)/plot.png
	@echo "Finished. Results (CSV + plot) are in $(OUTDIR)"

extract:  ## Extract CSV from a run
	@echo "Extracting CSVs for qlen=$(qlen)"
	$(PY) $(EXTRACT_PY) qlen-$(qlen)

plot:  ## Plot CSVs (expects csvs in $(OUTDIR))
	@echo "Plotting results in $(OUTDIR)"
	$(PY) $(PLOT_PY) -d $(OUTDIR) -o $(OUTDIR)/plot.png

clean:  ## Remove generated outputs
	@echo "Removing outputs/"
	rm -rf outputs


## ------------------------------------------------------------------
# algos: run extract+plot for the algorithms directories
# Usage: make algos
## ------------------------------------------------------------------
.PHONY: algos

ALGOS := taildrop codel red

algos:  ## Extract and plot RTT for taildrop, codel, red
	@echo "Processing algos: $(ALGOS)"
	@for a in $(ALGOS); do \
		 echo "--- $$a ---"; \
			 $(PY) $(BASEDIR)$(EXTRACT_PY) $$a || true; \
			 $(PY) $(BASEDIR)$(PLOT_PY) -d outputs/$$a -o outputs/$$a/rtt_plot.png || true; \
		echo "Saved outputs/$$a/rtt_plot.png" || true; \
	done


.PHONY: mitigate
mitigate:  ## Run algos extraction then generate combined log-scale RTT comparison
	@echo "Running mitigate: extract and combine RTT for $(ALGOS)"
	@for a in $(ALGOS); do \
		 echo "extracting $$a"; \
		 $(PY) $(BASEDIR)$(EXTRACT_PY) $$a || true; \
	done
	@echo "Generating combined log-scale RTT plot..."
	$(PY) $(BASEDIR)$(COMPARE_PY) $(addprefix outputs/,$(ALGOS)) -o outputs/mitigate_rtt.png
	@echo "Saved outputs/mitigate_rtt.png"

