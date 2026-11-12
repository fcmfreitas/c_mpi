#!/usr/bin/env python3
"""Resumo das medicoes do T3: le as saidas do Slurm (results/t3_*.out) e gera
results/resumo.md com as tabelas pedidas no enunciado.

    python3 scripts/analyze.py [arquivos .out ...]

Sem argumentos, usa o job mais recente de cada experimento em results/.
Cada execucao imprime uma linha "result chave=valor ..." (versoes sequencial e MPI)
e, com --rank-stats, uma linha "rank ..." por processo. As execucoes com stats=1
ficam fora das tabelas de tempo e alimentam as tabelas de carga por processo.

Speed-ups (secao 6): S = T1 / TP (contra o Bubble Sort do vetor inteiro) e
S^delta = T1^delta / TP (contra a mesma arvore em um so processo), E = S^delta / P
(ou S^delta / 32 na serie acima das 32 threads). T1 / T1^delta e o ganho de algoritmo.
"""
import glob
import os
import re
import statistics
import sys
from collections import defaultdict

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RESULTS = os.path.join(ROOT, "results")
EXPERIMENTS = ["t3_calibra", "t3_seq", "t3_orig", "t3_bal", "t3_over"]
HW_THREADS = 32  # 2 nos x 16 threads de hardware


def latest_outputs():
    files = []
    for exp in EXPERIMENTS:
        found = glob.glob(os.path.join(RESULTS, f"{exp}.*.out"))
        if found:
            files.append(max(found, key=lambda f: int(re.search(r"\.(\d+)\.out$", f).group(1))))
    return files


def parse_kv(line):
    return dict(item.split("=", 1) for item in line.split()[1:] if "=" in item)


def parse(files):
    """Devolve (execucoes, stats): execucoes e uma lista de dicts da linha "result";
    stats associa cada execucao com --rank-stats as suas linhas "rank"."""
    runs, stats = [], defaultdict(list)
    for path in files:
        current = None
        with open(path) as fh:
            for line in fh:
                if line.startswith("result "):
                    current = parse_kv(line)
                    current["file"] = os.path.basename(path)
                    runs.append(current)
                elif line.startswith("rank ") and current is not None:
                    stats[id(current)].append(parse_kv(line))
    return runs, stats


def summary(values):
    return statistics.median(values), min(values), max(values)


def fmt(seconds):
    return f"{seconds:.3f}"


def table(header, rows):
    out = ["| " + " | ".join(header) + " |", "| " + " | ".join(["---"] * len(header)) + " |"]
    out += ["| " + " | ".join(str(c) for c in row) + " |" for row in rows]
    return "\n".join(out) + "\n"


def main():
    files = sys.argv[1:] or latest_outputs()
    if not files:
        sys.exit("nenhuma saida em results/ (rode os scripts sbatch primeiro)")
    runs, stats = parse(files)
    md = ["# Resumo das medicoes do T3\n", "Arquivos: " + ", ".join(os.path.basename(f) for f in files) + "\n"]

    # Equivalencia: toda execucao deve ter check=OK e o mesmo checksum para o mesmo n.
    bad = [r for r in runs if r.get("check") != "OK"]
    checksums = defaultdict(set)
    for r in runs:
        checksums[r["n"]].add(r["checksum"])
    md.append("## Verificacao\n")
    md.append(f"- {len(runs)} execucoes, {len(bad)} com check diferente de OK.\n")
    for n, sums in sorted(checksums.items(), key=lambda kv: int(kv[0])):
        md.append(f"- n={n}: checksum {'unico' if len(sums) == 1 else 'DIVERGENTE'} ({', '.join(sorted(sums))})\n")
    md.append("\n")

    timed = [r for r in runs if r.get("stats", "0") == "0"]
    calib = [r for r in timed if r["version"] == "seq" and r["mode"] == "bubble" and "t3_calibra" in r["file"]]
    timed = [r for r in timed if "t3_calibra" not in r["file"]]
    if calib:
        md.append("## Calibracao de N (T1, uma execucao)\n\n")
        md.append(table(["n", "T1 (s)"], [(r["n"], fmt(float(r["time"]))) for r in calib]) + "\n")

    def times(pred):
        return [float(r["time"]) for r in timed if pred(r)]

    seq = [r for r in timed if r["version"] == "seq"]
    n_values = sorted({r["n"] for r in timed}, key=int)
    for n in n_values:
        t1 = times(lambda r: r["version"] == "seq" and r["mode"] == "bubble" and r["n"] == n)
        if not t1:
            md.append(f"(n={n}: sem T1 medido, speed-ups omitidos)\n")
            continue
        t1_med = summary(t1)[0]
        md.append(f"## n = {n}\n\n### Referencias sequenciais\n\n")
        rows = [("T1 (Bubble Sort inteiro)", "-", "-", "-", *map(fmt, summary(t1)), "1.00")]
        tree_ref, bal_ref = {}, {}
        for r_mode, key, ref in (("tree", "delta", tree_ref), ("balanced", "procs", bal_ref)):
            for value in sorted({r[key] for r in seq if r["mode"] == r_mode and r["n"] == n}, key=int):
                sel = [r for r in seq if r["mode"] == r_mode and r["n"] == n and r[key] == value]
                med, lo, hi = summary([float(r["time"]) for r in sel])
                ref[value] = med
                shape = sel[0]
                label = f"arvore delta={value}" if r_mode == "tree" else f"arvore balanceada P={value}"
                rows.append((label, shape["leaves"], f"{shape['min_leaf']}..{shape['max_leaf']}",
                             shape["depth"], fmt(med), fmt(lo), fmt(hi), f"{t1_med / med:.2f}"))
        md.append(table(["referencia", "folhas", "tamanho das folhas", "niveis", "mediana (s)",
                         "min (s)", "max (s)", "T1/T1^delta"], rows) + "\n")

        def speedup_table(title, version, yield_value, ref_key, ref, over=False):
            # over=True: so P > 32, com eficiencia contra as 32 threads; senao so P <= 32.
            efficiency_base = HW_THREADS if over else None
            sel = [r for r in timed if r["version"] == version and r["n"] == n
                   and (yield_value is None or r["yield"] == yield_value)
                   and (int(r["procs"]) > HW_THREADS) == over]
            if not sel:
                return
            rows = []
            for p in sorted({r["procs"] for r in sel}, key=int):
                med, lo, hi = summary([float(r["time"]) for r in sel if r["procs"] == p])
                sample = next(r for r in sel if r["procs"] == p)
                t1d = ref.get(sample[ref_key]) if ref_key else None
                if ref_key == "delta" and p == "1":
                    t1d = t1_med  # P=1: a raiz conquista o vetor inteiro (delta = n)
                s = t1_med / med
                if t1d:
                    sd = t1d / med
                    sd_txt, e_txt = f"{sd:.2f}", f"{100 * sd / (efficiency_base or int(p)):.0f}%"
                else:
                    sd_txt = e_txt = "-"
                rows.append((p, sample["delta"] if version == "orig" else "-", fmt(med), fmt(lo), fmt(hi),
                             f"{s:.2f}", sd_txt, e_txt))
            e_label = f"E = S^delta/{efficiency_base}" if efficiency_base else "E = S^delta/P"
            md.append(f"### {title}\n\n")
            md.append(table(["P", "delta", "mediana (s)", "min (s)", "max (s)", "S = T1/TP",
                             "S^delta", e_label], rows) + "\n")

        speedup_table("Versao original (yield=0)", "orig", "0", "delta", tree_ref)
        speedup_table("Versao balanceada", "bal", None, "procs", bal_ref)
        speedup_table("Versao balanceada com MPI_Isend", "bal-isend", None, "procs", bal_ref)
        # A serie de 63 a 1023 processos: eficiencia contra as 32 threads de hardware.
        for y, label in (("0", "espera ativa"), ("1", "cede o nucleo")):
            speedup_table(f"Original com P > 32, yield={y} ({label})", "orig", y, "delta",
                          tree_ref, over=True)

    # Carga por processo (execucoes com --rank-stats).
    stat_runs = [r for r in runs if r.get("stats") == "1" and id(r) in stats and "t3_calibra" not in r["file"]]
    if stat_runs:
        md.append("## Carga por processo (--rank-stats)\n\n")
        md.append("Por nivel da arvore: media dos tempos dos processos do nivel. "
                  "envio = mandando metades aos filhos + devolvendo ao pai; parado = esperando o pai + esperando os filhos. Bubble Sort (elem.): menor..maior folha do nivel.\n\n")
    for r in stat_runs:
        ranks = stats[id(r)]
        title = f"{r['version']} P={r['procs']} n={r['n']} yield={r['yield']} (TP={fmt(float(r['time']))} s)"
        by_depth = defaultdict(list)
        for s in ranks:
            by_depth[int(s["depth"])].append(s)
        rows = []
        for depth in sorted(by_depth):
            group = by_depth[depth]
            avg = lambda k: statistics.mean(float(s[k]) for s in group)
            hosts = defaultdict(int)
            for s in group:
                hosts[s["host"].split(".")[0]] += 1
            bubbled = [int(s["bubbled"]) for s in group]
            rows.append((depth, len(group), ", ".join(f"{h}:{c}" for h, c in sorted(hosts.items())),
                         f"{statistics.mean(int(s['received']) for s in group):.0f}",
                         f"{min(bubbled)}..{max(bubbled)}",
                         fmt(avg("bubble")), fmt(avg("merge")), fmt(avg("send") + avg("send_up")),
                         fmt(avg("wait_parent") + avg("wait_children")), fmt(avg("total"))))
        md.append(f"### {title}\n\n")
        md.append(table(["nivel", "processos", "nos", "recebidos (media)", "Bubble Sort (elem.)",
                         "bubble (s)", "merge (s)", "envio (s)", "parado (s)", "total (s)"], rows) + "\n")

    out_path = os.path.join(RESULTS, "resumo.md")
    with open(out_path, "w") as fh:
        fh.write("".join(md))
    print(f"escrito {out_path}")


if __name__ == "__main__":
    main()
