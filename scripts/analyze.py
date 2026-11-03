#!/usr/bin/env python3
"""Le as saidas do Slurm (results/*.out) e gera as tabelas e os graficos do relatorio.

Uso: python3 scripts/analyze.py [--results results] [--out relatorio] [--chunk N] [--dump]

Cada execucao na saida do Slurm comeca com uma marca "### exp=... P=... chunk=... height=... rep=..."
(funcao mark de scripts/lib.sh), seguida da linha de comando ("+ srun ..."), da linha
"frame=... time=... iterations=... checksum=..." e, com --rank-stats, de uma linha "rank=..."
por escravo; com --pingpong, de uma linha "pingpong rank=..." por escravo.
Quando um experimento aparece em mais de um job, vale o job mais recente (maior id).

Saidas: results/resumo.md (todas as tabelas, em markdown), <out>/tab_*.tex (fragmentos
LaTeX) e <out>/fig_*.pdf (graficos).
"""
import argparse
import glob
import math
import os
import re
import statistics
import sys
from collections import defaultdict

KV = re.compile(r"(\w+)=(\S+)")


def kv(text):
    return dict(KV.findall(text))


def parse_file(path):
    """Devolve a lista de execucoes (dicts) encontradas em um arquivo de saida do Slurm."""
    name = os.path.basename(path)
    m = re.match(r"(.+)\.(\d+)\.out$", name)
    job = int(m.group(2)) if m else 0
    runs, cur = [], None
    with open(path, errors="replace") as f:
        for line in f:
            line = line.rstrip("\n")
            if line.startswith("### "):
                d = kv(line[4:])
                cur = {"exp": d["exp"], "P": int(d["P"]), "chunk": int(d["chunk"]), "height": int(d["height"]),
                       "rep": int(d.get("rep", 1)), "file": name, "job": job, "cmd": None, "frame": None,
                       "workers": [], "pingpong": [], "master_host": None}
                runs.append(cur)
            elif cur is None:
                continue
            elif line.startswith("+ "):
                cur["cmd"] = line[2:]
            elif line.startswith("frame="):
                d = kv(line)
                cur["frame"] = {"time": float(d["time"]), "iterations": int(d["iterations"]), "checksum": d["checksum"],
                                "procs": int(d["procs"]), "width": int(d["width"]), "height": int(d["height"]),
                                "max_iter": int(d["max_iter"]), "mode": d["mode"], "chunk": int(d["chunk"])}
            elif line.lstrip().startswith("rank=") and "tasks=" in line:
                d = kv(line)
                cur["workers"].append({"rank": int(d["rank"]), "host": d["host"], "tasks": int(d["tasks"]),
                                       "iterations": int(d["iterations"]), "busy": float(d["busy"]),
                                       "per_task": float(d["per_task"]), "done_at": float(d["done_at"])})
            elif line.lstrip().startswith("rank=0") and "role=master" in line:
                cur["master_host"] = kv(line)["host"]
            elif line.startswith("pingpong "):
                d = kv(line)
                cur["pingpong"].append({"rank": int(d["rank"]), "host": d["host"], "master_host": d["master_host"],
                                        "chunk": int(d["chunk"]), "bytes": int(d["bytes"]),
                                        "round_trip": float(d["round_trip"])})
    return runs


def load(results_dir):
    runs = []
    for path in sorted(glob.glob(os.path.join(results_dir, "*.out"))):
        runs.extend(parse_file(path))
    # Por experimento, fica o job mais recente.
    latest = {}
    for r in runs:
        latest[r["exp"]] = max(latest.get(r["exp"], 0), r["job"])
    kept = [r for r in runs if r["job"] == latest[r["exp"]] and r["frame"] is not None]
    pp = [r for r in runs if r["job"] == latest[r["exp"]] and r["pingpong"]]
    return kept, pp, latest


def by(runs, exp):
    return [r for r in runs if r["exp"] == exp]


def med(xs):
    return statistics.median(xs) if xs else float("nan")


def fmt(x, nd=2, dec=","):
    if x is None or (isinstance(x, float) and math.isnan(x)):
        return "--"
    s = f"{x:.{nd}f}"
    return s.replace(".", dec) if dec != "." else s


def ceil_div(a, b):
    return -(-a // b)


# ----------------------------------------------------------------------------------------------
# Metricas de uma execucao MPI com --rank-stats.
def run_metrics(r, t1=None):
    T = r["frame"]["time"]
    w = r["workers"]
    m = {"T": T}
    if w:
        busy = sum(x["busy"] for x in w)
        n = len(w)
        m["busy_sum"] = busy
        m["util"] = busy / (n * T)                      # fracao do tempo em que os escravos calculam
        m["tail"] = T - min(x["done_at"] for x in w)    # janela entre o 1o escravo liberado e o fim
        m["busy_max"] = max(x["busy"] for x in w)
        m["busy_min"] = min(x["busy"] for x in w)
        m["tasks_max"] = max(x["tasks"] for x in w)
        m["tasks_min"] = min(x["tasks"] for x in w)
        if t1:
            m["inflation"] = busy / t1                  # tempo de CPU dos escravos / T1 (>1: calculo mais lento por nucleo)
    return m


def agg(runs, key, t1=None):
    vals = [run_metrics(r, t1).get(key) for r in runs]
    vals = [v for v in vals if v is not None]
    return med(vals)


# ----------------------------------------------------------------------------------------------
class Tables:
    def __init__(self, out_dir):
        self.md = []
        self.out_dir = out_dir

    def section(self, title):
        self.md.append(f"\n## {title}\n")

    def note(self, text):
        self.md.append(text + "\n")

    def table(self, name, header, rows, caption=None, align=None):
        """Grava a tabela em markdown (no resumo) e em LaTeX (fragmento <out>/tab_<name>.tex)."""
        self.md.append("| " + " | ".join(header) + " |")
        self.md.append("|" + "|".join("---" for _ in header) + "|")
        for row in rows:
            self.md.append("| " + " | ".join(str(c) for c in row) + " |")
        self.md.append("")
        cols = align or ("l" + "r" * (len(header) - 1))
        tex = ["\\begin{tabular}{" + cols + "}", "\\toprule",
               " & ".join(header) + " \\\\", "\\midrule"]
        for row in rows:
            tex.append(" & ".join(str(c).replace("%", "\\%") for c in row) + " \\\\")
        tex += ["\\bottomrule", "\\end{tabular}"]
        with open(os.path.join(self.out_dir, f"tab_{name}.tex"), "w") as f:
            f.write("\n".join(tex) + "\n")

    def write_md(self, path):
        with open(path, "w") as f:
            f.write("# Resumo das medicoes (gerado por scripts/analyze.py)\n")
            f.write("\n".join(self.md) + "\n")


# ----------------------------------------------------------------------------------------------
def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--results", default="results")
    ap.add_argument("--out", default="relatorio")
    ap.add_argument("--chunk", type=int, default=None, help="granularidade escolhida (padrao: a do experimento forte)")
    ap.add_argument("--dump", action="store_true", help="so lista as execucoes encontradas")
    args = ap.parse_args()
    os.makedirs(args.out, exist_ok=True)

    runs, pp_runs, latest = load(args.results)
    if args.dump:
        for r in runs:
            f = r["frame"]
            print(f"{r['file']}: {r['exp']} P={r['P']} chunk={r['chunk']} height={r['height']} rep={r['rep']} "
                  f"time={f['time']:.4f} iter={f['iterations']} sum={f['checksum']} workers={len(r['workers'])}")
        for r in pp_runs:
            print(f"{r['file']}: {r['exp']} chunk={r['chunk']} pingpong={len(r['pingpong'])} linhas")
        return
    print("jobs usados:", ", ".join(f"{e}={j}" for e, j in sorted(latest.items())))

    tb = Tables(args.out)
    seq = by(runs, "seq")
    forte = by(runs, "forte")
    chosen = args.chunk or (forte[0]["chunk"] if forte else None)
    procs = sorted({r["P"] for r in forte})
    t1 = med([r["frame"]["time"] for r in seq]) if seq else None
    f0 = (seq or forte or runs)[0]["frame"]
    tb.note(f"Entrada de referencia: {f0['width']}x{f0['height']}, max_iter={f0['max_iter']}; "
            f"granularidade escolhida: {chosen} linha(s) por tarefa.")

    # ---- T1 e equivalencia ------------------------------------------------------------------
    tb.section("T1 (sequencial) e equivalencia")
    checks = defaultdict(lambda: defaultdict(set))   # (height, max_iter) -> mode -> checksums
    iters = {}
    for r in runs:
        f = r["frame"]
        checks[(f["height"], f["max_iter"])][f["mode"]].add(f["checksum"])
        iters[(f["height"], f["max_iter"])] = f["iterations"]
    rows = []
    for (h, mi), modes in sorted(checks.items()):
        s = modes.get("seq", set())
        m = modes.get("mpi", set())
        ok = "ok" if len(s | m) == 1 else ("so mpi" if not s and len(m) == 1 else "DIFERE")
        rows.append([h, mi, iters[(h, mi)], ", ".join(sorted(s | m)), len(s), len(m), ok])
    tb.table("equiv", ["linhas", "max\\_iter", "iteracoes", "checksum", "exec. seq", "exec. MPI", "equiv."], rows,
             align="rrrlrrl")
    if seq:
        ts = [r["frame"]["time"] for r in seq]
        tb.note(f"T1 = mediana {t1:.3f} s (min {min(ts):.3f}, max {max(ts):.3f}) em {len(ts)} execucoes.")
        tb.table("t1", ["versao", "execucoes", "mediana (s)", "min (s)", "max (s)"],
                 [["sequencial", len(ts), fmt(t1, 3), fmt(min(ts), 3), fmt(max(ts), 3)]])

    # ---- forte -------------------------------------------------------------------------------
    strong = {}
    if forte and t1:
        tb.section("Escalabilidade forte")
        rows = []
        for P in procs + ["64p"]:
            if P == "64p":
                rs = by(runs, "forte_poll")
                if not rs:
                    continue
                label, Pn = "64 (polling)", 64
            else:
                rs = [r for r in forte if r["P"] == P]
                label, Pn = str(P), P
            ts = [r["frame"]["time"] for r in rs]
            T = med(ts)
            S = t1 / T
            strong[label] = {"P": Pn, "T": T, "min": min(ts), "max": max(ts), "S": S, "E": S / Pn,
                             "S_min": t1 / max(ts), "S_max": t1 / min(ts),
                             "util": agg(rs, "util"), "infl": agg(rs, "inflation", t1), "tail": agg(rs, "tail")}
            st = strong[label]
            rows.append([label, fmt(T, 2), fmt(st["min"], 2), fmt(st["max"], 2), fmt(S, 2), fmt(st["E"] * 100, 0) + "%",
                         fmt(st["infl"], 2), fmt(st["util"] * 100, 0) + "%", fmt(st["tail"], 2)])
        tb.table("forte", ["P", "mediana (s)", "min (s)", "max (s)", "S", "E", "CPU/T1", "util.", "cauda (s)"], rows)
        tb.note("CPU/T1: soma do tempo ocupado dos escravos dividida por T1 (>1: cada nucleo calcula mais devagar "
                "que na sequencial); util.: fracao do tempo em que os escravos estao calculando; cauda: tempo entre "
                "o primeiro escravo liberado e o fim.")

    # ---- fraca -------------------------------------------------------------------------------
    fraca = by(runs, "fraca")
    weak = {}
    if fraca and t1:
        tb.section("Escalabilidade fraca")
        valida = {r["frame"]["height"]: r["frame"] for r in by(runs, "valida")}
        base_iter = seq[0]["frame"]["iterations"]
        rows = []
        for P in sorted({r["P"] for r in fraca}):
            rs = [r for r in fraca if r["P"] == P]
            ts = [r["frame"]["time"] for r in rs]
            T = med(ts)
            f = rs[0]["frame"]
            weak[P] = {"T": T, "min": min(ts), "max": max(ts), "E": t1 / T, "E_min": t1 / max(ts), "E_max": t1 / min(ts),
                       "util": agg(rs, "util"), "infl": agg(rs, "inflation", t1 * P), "tail": agg(rs, "tail")}
            v = valida.get(f["height"])
            equiv = "ok" if v and v["checksum"] == f["checksum"] and v["iterations"] == f["iterations"] else "--"
            rows.append([P, f["height"], ceil_div(f["height"], rs[0]["chunk"]), fmt(f["iterations"] / base_iter, 2),
                         fmt(T, 2), fmt(min(ts), 2), fmt(max(ts), 2), fmt(weak[P]["E"] * 100, 0) + "%",
                         fmt(weak[P]["infl"], 2), fmt(weak[P]["util"] * 100, 0) + "%", equiv])
        tb.table("fraca", ["P", "linhas", "tarefas", "iter./iter.\\textsubscript{1}", "mediana (s)", "min (s)", "max (s)",
                           "E\\textsubscript{fraca}", "CPU/(P$\\cdot$T1)", "util.", "equiv. seq"], rows)

    # ---- granularidade -----------------------------------------------------------------------
    gran = by(runs, "gran")
    pings = {}
    for r in pp_runs:
        intra = [p["round_trip"] for p in r["pingpong"] if p["host"] == p["master_host"]]
        inter = [p["round_trip"] for p in r["pingpong"] if p["host"] != p["master_host"]]
        pings[r["chunk"]] = {"intra": med(intra), "inter": med(inter), "bytes": r["pingpong"][0]["bytes"]}
    gran_rows = {}
    if gran:
        tb.section("Granularidade (P=32)")
        rows = []
        for c in sorted({r["chunk"] for r in gran}):
            rs = [r for r in gran if r["chunk"] == c]
            ts = [r["frame"]["time"] for r in rs]
            T = med(ts)
            tasks_total = sum(w["tasks"] for r in rs for w in r["workers"])
            busy_total = sum(w["busy"] for r in rs for w in r["workers"])
            per_task = busy_total / tasks_total if tasks_total else float("nan")
            p = pings.get(c, {})
            g = gran_rows[c] = {"T": T, "min": min(ts), "max": max(ts), "per_task": per_task,
                                "intra": p.get("intra"), "inter": p.get("inter"), "bytes": p.get("bytes"),
                                "util": agg(rs, "util"), "tail": agg(rs, "tail"),
                                "tasks": ceil_div(rs[0]["frame"]["height"], c)}
            ratio = per_task / g["inter"] if g["inter"] else float("nan")
            rows.append([c, g["tasks"], fmt(T, 2), fmt(g["min"], 2), fmt(g["max"], 2), fmt(per_task * 1e3, 1),
                         fmt(g["intra"] * 1e6, 0) if g["intra"] else "--", fmt(g["inter"] * 1e6, 0) if g["inter"] else "--",
                         fmt(ratio, 0), fmt(g["util"] * 100, 0) + "%", fmt(g["tail"], 2)])
        tb.table("gran", ["linhas/tarefa", "tarefas", "mediana (s)", "min (s)", "max (s)", "tarefa (ms)",
                          "ida e volta intra ($\\mu$s)", "inter ($\\mu$s)", "tarefa/inter", "util.", "cauda (s)"], rows)
        tb.note("tarefa: tempo medio de processar uma tarefa (tempo ocupado / tarefas, todos os escravos e repeticoes); "
                "ida e volta: mediana do --pingpong entre o mestre e os escravos do mesmo no (intra) e do outro no (inter).")

    # ---- balanceamento -----------------------------------------------------------------------
    bal_src = [r for r in gran if r["chunk"] == chosen] or [r for r in forte if r["P"] == 32]
    balance = []
    if bal_src:
        tb.section(f"Balanceamento por escravo (P=32, {chosen} linha(s) por tarefa, mediana de {len(bal_src)} execucoes)")
        per_rank = defaultdict(list)
        for r in bal_src:
            for w in r["workers"]:
                per_rank[w["rank"]].append(w)
        T = med([r["frame"]["time"] for r in bal_src])
        rows = []
        for rank in sorted(per_rank):
            ws = per_rank[rank]
            b = {"rank": rank, "host": ws[0]["host"], "tasks": med([w["tasks"] for w in ws]),
                 "busy": med([w["busy"] for w in ws]), "done_at": med([w["done_at"] for w in ws]),
                 "iterations": med([w["iterations"] for w in ws])}
            balance.append(b)
            rows.append([rank, b["host"], fmt(b["tasks"], 0), fmt(b["iterations"] / 1e6, 0), fmt(b["busy"], 3),
                         fmt(b["done_at"], 3)])
        tb.table("balance", ["rank", "no", "tarefas", "iter. (M)", "ocupado (s)", "liberado em (s)"], rows,
                 align="rlrrrr")
        # Resumo por no
        rows = []
        for host in sorted({b["host"] for b in balance}):
            bs = [b for b in balance if b["host"] == host]
            rows.append([host, len(bs), f"{min(b['tasks'] for b in bs):.0f}--{max(b['tasks'] for b in bs):.0f}",
                         fmt(min(b["busy"] for b in bs), 2) + "--" + fmt(max(b["busy"] for b in bs), 2),
                         fmt(min(b["done_at"] for b in bs), 2) + "--" + fmt(max(b["done_at"] for b in bs), 2)])
        tb.table("balance_resumo", ["no", "escravos", "tarefas", "ocupado (s)", "liberado em (s)"], rows, align="lrrrr")
        tb.note(f"T (mediana) = {T:.3f} s.")

    tb.write_md(os.path.join(args.results, "resumo.md"))
    print(open(os.path.join(args.results, "resumo.md")).read())

    # ---- graficos ----------------------------------------------------------------------------
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except ImportError:
        print("matplotlib nao encontrado: graficos nao gerados", file=sys.stderr)
        return
    plots(plt, args.out, strong, weak, gran_rows, balance, chosen, procs)


# ----------------------------------------------------------------------------------------------
BLUE, ORANGE, AQUA, YELLOW, VIOLET = "#2a78d6", "#eb6834", "#1baf7a", "#eda100", "#6250d6"
INK, MUTED, GRID = "#0b0b0b", "#52514e", "#e4e3df"


def style(ax, xlabel, ylabel, logx=True):
    ax.set_xlabel(xlabel, color=INK)
    ax.set_ylabel(ylabel, color=INK)
    ax.grid(True, color=GRID, linewidth=0.6)
    ax.set_axisbelow(True)
    for s in ("top", "right"):
        ax.spines[s].set_visible(False)
    for s in ("left", "bottom"):
        ax.spines[s].set_color(MUTED)
    ax.tick_params(colors=MUTED, labelcolor=INK, width=0.6)
    if logx:
        ax.set_xscale("log", base=2)
        ax.xaxis.set_major_formatter(__import__("matplotlib").ticker.FuncFormatter(lambda v, _: f"{v:g}"))


def plots(plt, out, strong, weak, gran, balance, chosen, procs):
    plt.rcParams.update({"font.size": 8, "font.family": "sans-serif", "pdf.fonttype": 42, "axes.titlesize": 8,
                         "legend.fontsize": 7, "legend.frameon": False})
    base = {k: v for k, v in strong.items() if "polling" not in k}
    if base:
        Ps = [v["P"] for v in base.values()]
        S = [v["S"] for v in base.values()]
        err = [[v["S"] - v["S_min"] for v in base.values()], [v["S_max"] - v["S"] for v in base.values()]]
        fig, ax = plt.subplots(figsize=(3.3, 2.5))
        lim = max(Ps)
        ax.plot([1, lim], [1, lim], color=MUTED, linewidth=1, linestyle="--", label="ideal $S=P$")
        ax.plot([2, lim], [1, lim - 1], color=MUTED, linewidth=0.8, linestyle=":", label="$S=P-1$ (mestre sem calcular)")
        ax.errorbar(Ps, S, yerr=err, color=BLUE, linewidth=1.5, marker="o", markersize=4, capsize=2, label="medido (mediana, min--max)")
        for p, s in zip(Ps, S):
            ax.annotate(f"{s:.1f}", (p, s), textcoords="offset points", xytext=(4, -9), fontsize=6.5, color=INK)
        style(ax, "processos (P)", "speed-up", logx=False)
        ax.set_xticks(Ps)
        ax.set_xlim(0, lim + 3)
        ax.set_ylim(0, lim + 3)
        ax.legend(loc="upper left")
        fig.tight_layout()
        fig.savefig(os.path.join(out, "fig_speedup.pdf"))
        plt.close(fig)

        fig, ax = plt.subplots(figsize=(3.3, 2.5))
        E = [v["E"] * 100 for v in base.values()]
        ax.plot([p for p in Ps], [(p - 1) / p * 100 for p in Ps], color=MUTED, linewidth=0.8, linestyle=":",
                label="limite $(P-1)/P$")
        ax.plot(Ps, E, color=BLUE, linewidth=1.5, marker="o", markersize=4, label="forte")
        if weak:
            Pw = sorted(weak)
            ax.plot(Pw, [weak[p]["E"] * 100 for p in Pw], color=ORANGE, linewidth=1.5, marker="s", markersize=4, label="fraca")
        style(ax, "processos (P)", "eficiência (%)")
        ax.set_xticks(Ps)
        ax.set_ylim(0, 105)
        ax.legend(loc="lower left")
        fig.tight_layout()
        fig.savefig(os.path.join(out, "fig_eficiencia.pdf"))
        plt.close(fig)

    if gran:
        cs = sorted(gran)
        fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(6.6, 2.4))
        T = [gran[c]["T"] for c in cs]
        ax1.fill_between(cs, [gran[c]["min"] for c in cs], [gran[c]["max"] for c in cs], color=BLUE, alpha=0.15, linewidth=0)
        ax1.plot(cs, T, color=BLUE, linewidth=1.5, marker="o", markersize=4, label="tempo (mediana, min--max)")
        ax1.plot(cs, [gran[c]["tail"] for c in cs], color=ORANGE, linewidth=1.2, marker="s", markersize=3.5,
                 label="cauda (1º escravo liberado → fim)")
        if chosen in gran:
            ax1.axvline(chosen, color=MUTED, linewidth=0.8, linestyle="--")
            ax1.annotate("escolhida", (chosen, max(T)), textcoords="offset points", xytext=(3, -2), fontsize=6.5, color=MUTED)
        style(ax1, "linhas por tarefa", "segundos")
        ax1.set_xticks(cs)
        ax1.legend(loc="upper left")
        ax2.plot(cs, [gran[c]["per_task"] * 1e3 for c in cs], color=BLUE, linewidth=1.5, marker="o", markersize=4, label="processar 1 tarefa")
        if any(gran[c]["inter"] for c in cs):
            ax2.plot(cs, [gran[c]["inter"] * 1e3 if gran[c]["inter"] else float("nan") for c in cs], color=ORANGE, linewidth=1.5,
                     marker="s", markersize=4, label="ida e volta entre nós")
            ax2.plot(cs, [gran[c]["intra"] * 1e3 if gran[c]["intra"] else float("nan") for c in cs], color=AQUA, linewidth=1.5,
                     marker="^", markersize=4, label="ida e volta no mesmo nó")
        style(ax2, "linhas por tarefa", "milissegundos")
        ax2.set_yscale("log")
        ax2.set_xticks(cs)
        ax2.legend(loc="upper left")
        fig.tight_layout()
        fig.savefig(os.path.join(out, "fig_gran.pdf"))
        plt.close(fig)

    if balance:
        ranks = [b["rank"] for b in balance]
        hosts = sorted({b["host"] for b in balance})
        colors = {h: c for h, c in zip(hosts, [BLUE, ORANGE, AQUA, YELLOW])}
        fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(6.6, 2.8), sharex=True)
        ax1.bar(ranks, [b["tasks"] for b in balance], color=[colors[b["host"]] for b in balance], width=0.7)
        style(ax1, "", "tarefas", logx=False)
        ax2.bar(ranks, [b["busy"] for b in balance], color=[colors[b["host"]] for b in balance], width=0.7, label="ocupado")
        ax2.plot(ranks, [b["done_at"] for b in balance], color=INK, linewidth=0, marker="_", markersize=7, label="liberado em")
        style(ax2, "escravo (rank)", "segundos", logx=False)
        ax2.set_xticks(ranks)
        ax2.tick_params(axis="x", labelsize=6)
        handles = [plt.Rectangle((0, 0), 1, 1, color=colors[h]) for h in hosts] + [plt.Line2D([], [], color=INK, marker="_", linewidth=0)]
        ax2.legend(handles, hosts + ["liberado em (fim do saco)"], loc="lower right", ncol=3)
        fig.tight_layout()
        fig.savefig(os.path.join(out, "fig_balance.pdf"))
        plt.close(fig)
    print("graficos em", out)


if __name__ == "__main__":
    main()
