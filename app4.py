# app4.py
import os
import re
import shutil
import subprocess
import tempfile
import uuid

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from flask import Flask, render_template, request, url_for

app = Flask(__name__)

TOY4 = os.path.join(os.path.dirname(os.path.abspath(__file__)), "toy4")
STATIC_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "static")


def _parse_meta(caminho):
    meta = {}
    with open(caminho) as f:
        for linha in f:
            if "=" in linha:
                k, v = linha.strip().split("=", 1)
                meta[k] = v
    return meta


def _gera_grafico(normais_path, subnormais_path, destino, titulo, xmax):
    plt.figure(figsize=(10, 3.2))

    if normais_path and os.path.getsize(normais_path) > 0:
        n = np.loadtxt(normais_path)
        if n.ndim == 1:
            n = n.reshape(1, -1)
        plt.vlines(n[:, 0], 0, 0.19, colors="#2459a6",
                   linewidth=1.4, label="Normais")

    if subnormais_path and os.path.getsize(subnormais_path) > 0:
        s = np.loadtxt(subnormais_path)
        if s.ndim == 1:
            s = s.reshape(1, -1)
        plt.vlines(s[:, 0], 0, 0.19, colors="#df7b19",
                   linewidth=1.4, label="Subnormais")

    plt.axvline(0, color="black", linewidth=1.5)
    plt.xlim(0, 1.06 * xmax)
    plt.ylim(0, 0.30)
    plt.yticks([])
    plt.xlabel("Reta numérica (x >= 0)")
    plt.title(titulo)
    plt.legend(loc="upper right")
    plt.tight_layout()
    plt.savefig(destino, dpi=110)
    plt.close()


@app.route("/", methods=["GET"])
def index():
    return render_template("index.html", resultado=None, erro=None)


@app.route("/calculate", methods=["POST"])
def calculate():
    bruto = {
        "base": request.form.get("base", "").strip(),
        "precisao": request.form.get("precisao", "").strip(),
        "expoente_minimo": request.form.get("expoente_minimo", "").strip(),
    }

    if not re.fullmatch(r"\d+", bruto["base"]) or \
       not re.fullmatch(r"\d+", bruto["precisao"]) or \
       not re.fullmatch(r"-?\d+", bruto["expoente_minimo"]):
        return render_template(
            "index.html", resultado=None,
            erro="Preencha base, precisão e expoente mínimo com inteiros."
        ), 400

    b = int(bruto["base"])
    p = int(bruto["precisao"])
    emin = int(bruto["expoente_minimo"])

    if not (2 <= b <= 36):
        return render_template("index.html", resultado=None,
                               erro="Base deve estar entre 2 e 36."), 400
    if p < 1:
        return render_template("index.html", resultado=None,
                               erro="Precisão deve ser >= 1."), 400
    if not (-30 <= emin <= 0):
        return render_template("index.html", resultado=None,
                               erro="Expoente mínimo deve estar entre -30 e 0."), 400

    tmp = tempfile.mkdtemp(prefix="toy4_")
    try:
        r = subprocess.run(
            [TOY4, str(b), str(p), str(emin), "dados", tmp],
            capture_output=True, text=True, timeout=30,
        )
        if r.returncode != 0:
            return render_template(
                "index.html", resultado=None,
                erro=f"toy4 falhou: {r.stderr.strip() or 'erro desconhecido'}"
            ), 500

        meta = _parse_meta(os.path.join(tmp, "meta.txt"))
        qtd_normais = int(meta["qtd_normais"])
        qtd_subnormais = int(meta["qtd_subnormais"])
        completos = (
            qtd_subnormais > 0
            and qtd_subnormais <= 2048
            and qtd_normais <= 2048
            and qtd_subnormais <= 2048 - qtd_normais
        )

        maior_normal = float(meta["maior_normal"])

        t = subprocess.run(
            [TOY4, str(b), str(p), str(emin), "tabela"],
            capture_output=True, text=True, timeout=60,
        )
        if t.returncode != 0:
            return render_template(
                "index.html", resultado=None,
                erro=f"toy4 (tabela) falhou: {t.stderr.strip()}"
            ), 500

        os.makedirs(STATIC_DIR, exist_ok=True)
        sufixo = uuid.uuid4().hex[:10]

        nome_normais = f"plot_normais_{sufixo}.png"
        nome_completo = f"plot_completo_{sufixo}.png"

        _gera_grafico(
            os.path.join(tmp, "normais.dat"),
            "",
            os.path.join(STATIC_DIR, nome_normais),
            titulo=f"F({b}, {p}, {emin}, {1-emin}) - só normais",
            xmax=maior_normal,
        )

        if completos:
            _gera_grafico(
                os.path.join(tmp, "normais.dat"),
                os.path.join(tmp, "subnormais.dat"),
                os.path.join(STATIC_DIR, nome_completo),
                titulo=f"F({b}, {p}, {emin}, {1-emin}) - completo",
                xmax=maior_normal,
            )

        resultado = {
            "base": b,
            "precisao": p,
            "emin": emin,
            "emax": 1 - emin,
            "qtd_normais": qtd_normais,
            "qtd_subnormais": qtd_subnormais,
            "completos": completos,
            "tabela": t.stdout,
            "img_normais": url_for("static", filename=nome_normais),
            "img_completo": url_for("static", filename=nome_completo)
                            if completos else None,
        }
        return render_template("index.html", resultado=resultado, erro=None)

    finally:
        shutil.rmtree(tmp, ignore_errors=True)


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000, debug=True)
