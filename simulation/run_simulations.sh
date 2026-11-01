#!/usr/bin/env bash
# Uso: ./run_simulations.sh [n_eventos]

set -euo pipefail

NEVENTS="${1:-}"
if [[ -z "$NEVENTS" ]]; then
    echo "[INFO ]: Using default nEvents = 10000"
    NEVENTS=10000
fi

MG5="$HOME/MG5_aMC_v3_5_11/bin/mg5_aMC"

mkdir -p logs

run_mg5() {
    local name="$1"
    local outdir="$2"
    local root_file="$outdir/Events/run_01/tag_1_delphes_events.root"

    echo "Running: $name"

    if ! "$MG5" > "logs/${name}.log" 2>&1; then
        echo "[ERROR]: Error while running $name"
        echo "Check logs/${name}.log"
        tail -n 30 "logs/${name}.log"
        return 1
    fi

    # Verifico que exista el ROOT de delphes
    if [[ ! -f "$root_file" ]]; then
        echo "[ERROR]: $name finished but $root_file was not found"
        echo "Check logs/${name}.log"
        tail -n 30 "logs/${name}.log"
        return 1
    fi

    echo "[DONE ]: $name completed successfully -> $root_file"
}

echo "================= |Generating SIGNAL|"

run_mg5 signal signal_pp_H_aa <<EOF
import model heft
generate p p > h > a a
output signal_pp_H_aa -f
launch signal_pp_H_aa
set nevents $NEVENTS
shower=Pythia8
detector=Delphes
done
EOF

echo "================= |Generating BACKGROUND|"

run_mg5 background bkg_pp_aa <<EOF
import model sm
generate p p > a a / h
output bkg_pp_aa -f
launch bkg_pp_aa
set nevents $NEVENTS
shower=Pythia8
detector=Delphes
done
EOF

echo "[DONE ]: Simulation pipeline finished"