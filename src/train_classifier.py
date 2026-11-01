#!/usr/bin/env python3
"""
train_classifier.py

Entrena un clasificador (Gradient Boosting) para separar señal de background
usando los CSV generados por extract_diphoton, y guarda el modelo en ../model/.

Uso:
    python train_classifier.py ../data/signal_vars.csv ../data/bkg_vars.csv
    python train_classifier.py signal.csv bkg.csv --drop m_gg

Dependencias:
    pip install numpy pandas scikit-learn joblib
"""

import argparse
from pathlib import Path

import joblib
import numpy as np
import pandas as pd
import sklearn
from sklearn.ensemble import HistGradientBoostingClassifier
from sklearn.model_selection import train_test_split
from sklearn.utils.class_weight import compute_sample_weight

FILL = -999.0  # valor de relleno usado por el programa en C++
WEIGHT_COL = "event_weight"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("signal_csv")
    ap.add_argument("background_csv")
    ap.add_argument("--out", default="../model", help="directorio de salida del modelo")
    ap.add_argument("--drop", nargs="*", default=[],
                    help="features a excluir (ej: --drop m_gg)")
    ap.add_argument("--test-size", type=float, default=0.2)
    ap.add_argument("--seed", type=int, default=42)
    args = ap.parse_args()

    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)

    # ---- Datos -----------------------------------------------------------
    sig = pd.read_csv(args.signal_csv)
    bkg = pd.read_csv(args.background_csv)
    sig["label"] = 1
    bkg["label"] = 0
    df = pd.concat([sig, bkg], ignore_index=True)
    df = df.replace(FILL, np.nan)  # HistGradientBoosting maneja NaN de forma nativa

    features = [c for c in df.columns
                if c not in ("label", WEIGHT_COL) and c not in args.drop]
    X, y = df[features], df["label"]

    print(f"Signal events: {len(sig)} | Background events: {len(bkg)}")
    print(f"Features ({len(features)}): {features}")

    # Se separa un conjunto de test que NO se usa aquí (queda reproducible con
    # la misma semilla y test_size, guardados junto al modelo).
    X_train, _, y_train, _ = train_test_split(
        X, y, test_size=args.test_size, stratify=y, random_state=args.seed)

    # Balancea las clases durante el entrenamiento
    w_train = compute_sample_weight("balanced", y_train)

    # ---- Entrenamiento ---------------------------------------------------
    clf = HistGradientBoostingClassifier(
        max_iter=500,
        learning_rate=0.05,
        max_leaf_nodes=31,
        early_stopping=True,
        validation_fraction=0.1,
        n_iter_no_change=20,
        random_state=args.seed,
    )
    clf.fit(X_train, y_train, sample_weight=w_train)
    print(f"Used iterations (early stopping): {clf.n_iter_}")

    # ---- Guardar ---------------------------------------------------------
    model_path = out / "classifier_model.joblib"
    joblib.dump({
        "model": clf,
        "features": features,
        "seed": args.seed,
        "test_size": args.test_size,
        "sklearn_version": sklearn.__version__,
    }, model_path)
    print(f"Model saved at: {model_path}")


if __name__ == "__main__":
    main()