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
from flask import Flask, jsonify, render_template, request, url_for

app = Flask(__name__)

BASE_DIR   = os.path.dirname(os.path.abspath(__file__))
TOY4       = os.path.join(BASE_DIR, "toy4")
STATIC_DIR = os.path.join(BASE_DIR, "static")

MAX_PRECISAO = 10          # espelha o max do slider
MAX_EXPOENTE = 30


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


def _valida_int(nome, valor, minimo, maximo):
    if not re.fullmatch(r"-?\d+", valor):
        raise ValueError(f"{nome} deve ser inteiro.")
    n = int(valor)
    if not (minimo <= n <= maximo):
        raise ValueError(f"{nome} deve estar entre {minimo} e {maximo}.")
    return n


def _executa(b, p, emin):
    """Roda toy4 nos modos dados e tabela. Devolve dict pronto para JSON."""
    tmp = tempfile.mkdtemp(prefix="toy4_")
    try:
        r = subprocess.run(
            [TOY4, str(b), str(p), str(emin), "dados", tmp],
            capture_output=True, text=True, timeout=30,
        )
        if r.returncode != 0:
            raise RuntimeError(r.stderr.strip() or "toy4 (dados) falhou")

        meta = _parse_meta(os.path.join(tmp, "meta.txt"))
        qtd_normais    = int(meta["qtd_normais"])
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
            raise RuntimeError(t.stderr.strip() or "toy4 (tabela) falhou")

        os.makedirs(STATIC_DIR, exist_ok=True)
        sufixo = uuid.uuid4().hex[:10]
        nome_normais  = f"plot_normais_{sufixo}.png"
        nome_completo = f"plot_completo_{sufixo}.png"

        _gera_grafico(
            os.path.join(tmp, "normais.dat"), "",
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

        return {
            "base": b,
            "precisao": p,
            "emin": emin,
            "emax": 1 - emin,
            "qtd_normais": qtd_normais,
            "qtd_subnormais": qtd_subnormais,
            "completos": completos,
            "tabela": t.stdout,
            "img_normais": url_for("static", filename=nome_normais),
            "img_completo": (url_for("static", filename=nome_completo)
                             if completos else None),
        }
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


@app.route("/", methods=["GET"])
def index():
    return render_template("index.html")


@app.route("/api/calculate", methods=["GET"])
def api_calculate():
    try:
        b    = _valida_int("base",            request.args.get("base", ""),            2, 36)
        p    = _valida_int("precisao",        request.args.get("precisao", ""),        1, MAX_PRECISAO)
        emin = _valida_int("expoente_minimo", request.args.get("expoente_minimo", ""), -MAX_EXPOENTE, 0)
        return jsonify(_executa(b, p, emin))
    except ValueError as e:
        return jsonify({"erro": str(e)}), 400
    except (RuntimeError, subprocess.TimeoutExpired) as e:
        return jsonify({"erro": str(e)}), 500


@app.route("/calculate", methods=["POST"])
def calculate():
    """Fallback HTML (sem JavaScript)."""
    try:
        b    = _valida_int("base",            request.form.get("base", ""),            2, 36)
        p    = _valida_int("precisao",        request.form.get("precisao", ""),        1, MAX_PRECISAO)
        emin = _valida_int("expoente_minimo", request.form.get("expoente_minimo", ""), -MAX_EXPOENTE, 0)
        resultado = _executa(b, p, emin)
        return render_template(
            "resultado.html",
            resultado=resultado, erro=None,
        )
    except ValueError as e:
        return render_template("resultado.html",
                               resultado=None, erro=str(e)), 400
    except (RuntimeError, subprocess.TimeoutExpired) as e:
        return render_template("resultado.html",
                               resultado=None, erro=str(e)), 500


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000, debug=True)
