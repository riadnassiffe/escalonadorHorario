#!/usr/bin/env python3
"""
solve_cvxpy.py
Script para leitura de um problema de otimização a partir de um arquivo MPS
e resolução utilizando a biblioteca de modelagem algébrica CVXPY.

Execução via uv:
    uv run scripts/solve_cvxpy.py
    uv run scripts/solve_cvxpy.py grade_cuopt_model.mps --solver HIGHS --time-limit 60
    uv run scripts/solve_cvxpy.py grade_cuopt_model.mps --solver CUOPT --time-limit 60
"""

import argparse
import os
import sys
import time
import numpy as np
import cvxpy as cp
from scipy.sparse import csr_matrix


def load_mps_to_matrices(mps_file: str):
    """
    Lê o arquivo MPS utilizando o parser nativo de alta performance (cuOpt)
    e retorna os vetores e a matriz esparsa CSR do problema.
    """
    from cuopt import linear_programming

    t0 = time.time()
    data_model = linear_programming.Read(mps_file)
    read_time = time.time() - t0

    c = np.array(data_model.get_objective_coefficients(), dtype=float)
    offset = float(data_model.get_objective_offset())
    maximize = bool(data_model.get_sense())

    var_lb = np.array(data_model.get_variable_lower_bounds(), dtype=float)
    var_ub = np.array(data_model.get_variable_upper_bounds(), dtype=float)
    var_types = np.array(data_model.get_variable_types())
    var_names = data_model.get_variable_names()

    offsets = np.array(data_model.get_constraint_matrix_offsets(), dtype=int)
    indices = np.array(data_model.get_constraint_matrix_indices(), dtype=int)
    values = np.array(data_model.get_constraint_matrix_values(), dtype=float)
    con_lb = np.array(data_model.get_constraint_lower_bounds(), dtype=float)
    con_ub = np.array(data_model.get_constraint_upper_bounds(), dtype=float)
    row_types = np.array(data_model.get_ascii_row_types(), dtype=np.int8)

    n_rows = len(con_lb)
    n_cols = len(c)
    A = csr_matrix((values, indices, offsets), shape=(n_rows, n_cols))

    return {
        "c": c,
        "offset": offset,
        "maximize": maximize,
        "var_lb": var_lb,
        "var_ub": var_ub,
        "var_types": var_types,
        "var_names": var_names,
        "A": A,
        "con_lb": con_lb,
        "con_ub": con_ub,
        "row_types": row_types,
        "read_time": read_time,
    }


def build_cvxpy_problem(data: dict, relax: bool = False):
    """
    Constrói o objeto cp.Problem do CVXPY a partir dos dados matriciais do MPS.
    """
    t0 = time.time()
    c = data["c"]
    offset = data["offset"]
    maximize = data["maximize"]
    var_lb = data["var_lb"]
    var_ub = data["var_ub"]
    var_types = data["var_types"]
    A = data["A"]
    con_lb = data["con_lb"]
    con_ub = data["con_ub"]
    row_types = data["row_types"]

    n_cols = len(c)

    # Definição da variável de decisão vetorial no CVXPY
    # Verifica se o modelo possui variáveis inteiras/binárias
    has_integers = np.any(var_types == "I") and not relax
    is_all_binary = (
        has_integers
        and np.all((var_lb == 0.0) | (var_lb == 1.0))
        and np.all((var_ub == 0.0) | (var_ub == 1.0))
        and np.all((var_types != "C") | (var_lb == var_ub))
    )

    if is_all_binary:
        x = cp.Variable(n_cols, boolean=True, name="x")
    elif has_integers:
        x = cp.Variable(n_cols, integer=True, name="x")
    else:
        x = cp.Variable(n_cols, name="x")

    constraints = []

    # 1. Restrições de igualdade ('E' = ASCII 69)
    mask_eq = row_types == 69
    if np.any(mask_eq):
        constraints.append(A[mask_eq] @ x == con_ub[mask_eq])

    # 2. Restrições de menor ou igual ('L' = ASCII 76)
    mask_le = row_types == 76
    if np.any(mask_le):
        constraints.append(A[mask_le] @ x <= con_ub[mask_le])
        # Caso existam linhas 'L' com limite inferior finito (RANGES)
        mask_le_ranged = mask_le & np.isfinite(con_lb) & (con_lb > -1e19)
        if np.any(mask_le_ranged):
            constraints.append(A[mask_le_ranged] @ x >= con_lb[mask_le_ranged])

    # 3. Restrições de maior ou igual ('G' = ASCII 71)
    mask_ge = row_types == 71
    if np.any(mask_ge):
        constraints.append(A[mask_ge] @ x >= con_lb[mask_ge])
        # Caso existam linhas 'G' com limite superior finito (RANGES)
        mask_ge_ranged = mask_ge & np.isfinite(con_ub) & (con_ub < 1e19)
        if np.any(mask_ge_ranged):
            constraints.append(A[mask_ge_ranged] @ x <= con_ub[mask_ge_ranged])

    # 4. Limites de caixa (Bounds) das variáveis
    if is_all_binary:
        # Variáveis boolean=True já possuem 0 <= x <= 1 nativamente;
        # adicionamos restrições apenas para variáveis fixadas (lb == ub)
        fixed_zero = np.where(var_ub == 0.0)[0]
        fixed_one = np.where(var_lb == 1.0)[0]
        if len(fixed_zero) > 0:
            constraints.append(x[fixed_zero] == 0.0)
        if len(fixed_one) > 0:
            constraints.append(x[fixed_one] == 1.0)
    else:
        finite_lb = np.isfinite(var_lb) & (var_lb > -1e19)
        finite_ub = np.isfinite(var_ub) & (var_ub < 1e19)
        if np.all(finite_lb):
            constraints.append(x >= var_lb)
        elif np.any(finite_lb):
            constraints.append(x[finite_lb] >= var_lb[finite_lb])

        if np.all(finite_ub):
            constraints.append(x <= var_ub)
        elif np.any(finite_ub):
            constraints.append(x[finite_ub] <= var_ub[finite_ub])

    # Função Objetivo
    obj_expr = c @ x + offset
    objective = cp.Maximize(obj_expr) if maximize else cp.Minimize(obj_expr)

    problem = cp.Problem(objective, constraints)
    build_time = time.time() - t0
    return problem, x, build_time


def main():
    parser = argparse.ArgumentParser(
        description="Lê um arquivo MPS e resolve o problema de otimização utilizando CVXPY."
    )
    parser.add_argument(
        "mps_file",
        nargs="?",
        default="grade_cuopt_model.mps",
        help="Caminho para o arquivo MPS (padrão: grade_cuopt_model.mps)",
    )
    parser.add_argument(
        "--solver",
        type=str,
        default="HIGHS",
        choices=["HIGHS", "CUOPT", "SCIPY", "CLARABEL", "OSQP", "SCS"],
        help="Solver utilizado pelo CVXPY (padrão: HIGHS)",
    )
    parser.add_argument(
        "--time-limit",
        type=float,
        default=60.0,
        help="Tempo limite de execução em segundos (padrão: 60.0)",
    )
    parser.add_argument(
        "--relax",
        action="store_true",
        help="Resolve a relaxação linear (LP) ignorando a integralidade das variáveis",
    )
    parser.add_argument(
        "--verbose",
        action="store_true",
        help="Exibe o log detalhado do solver durante a otimização",
    )
    args = parser.parse_args()

    if not os.path.exists(args.mps_file):
        print(f"[-] Erro: Arquivo MPS não encontrado: {args.mps_file}")
        sys.exit(1)

    print("=" * 72)
    print("  Resolução de Modelo MPS via CVXPY")
    print("=" * 72)
    size_mb = os.path.getsize(args.mps_file) / (1024 * 1024)
    print(f"[*] Arquivo MPS:       {args.mps_file} ({size_mb:.2f} MB)")
    print(f"[*] Solver CVXPY:      {args.solver}")
    print(f"[*] Solvers ativos:    {cp.installed_solvers()}")
    print(f"[*] Tempo limite:      {args.time_limit} s")

    # 1. Leitura do arquivo MPS
    print("\n[*] Carregando estruturas matriciais do arquivo MPS...")
    data = load_mps_to_matrices(args.mps_file)
    n_vars = len(data["c"])
    n_cons = data["A"].shape[0]
    nnz = data["A"].nnz
    print(f"[+] MPS lido em {data['read_time']:.2f}s:")
    print(f"    - Variáveis:       {n_vars:,}")
    print(f"    - Restrições:      {n_cons:,}")
    print(f"    - Elementos (NNZ): {nnz:,}")
    print(f"    - Sentido:         {'Maximização' if data['maximize'] else 'Minimização'}")

    # 2. Construção do modelo no CVXPY
    print("\n[*] Construindo cp.Problem no CVXPY...")
    problem, x, build_time = build_cvxpy_problem(data, relax=args.relax)
    print(f"[+] Modelo CVXPY construído em {build_time:.2f}s!")

    # 3. Configuração de parâmetros específicos do solver escolhido
    solver_kwargs = {"verbose": args.verbose}
    solver_name = args.solver.upper()

    if solver_name == "HIGHS":
        solver_kwargs["time_limit"] = args.time_limit
    elif solver_name == "SCIPY":
        solver_kwargs["scipy_options"] = {"time_limit": args.time_limit}
    elif solver_name == "CUOPT":
        solver_kwargs["time_limit"] = args.time_limit

    # 4. Resolução via problem.solve()
    print(f"\n[*] Resolvendo via CVXPY (solver={solver_name})...")
    t_solve = time.time()
    try:
        problem.solve(solver=getattr(cp, solver_name), **solver_kwargs)
    except Exception as e:
        print(f"\n[!] Erro ao executar o solver {solver_name}: {e}")
        if solver_name == "CUOPT":
            print("[*] Alternando automaticamente para o solver HIGHS no CVXPY...")
            problem.solve(solver=cp.HIGHS, verbose=args.verbose, time_limit=args.time_limit)
        else:
            sys.exit(1)
    solve_time = time.time() - t_solve

    # 5. Exibição dos resultados
    print(f"\n[+] Finalizado em {solve_time:.2f}s!")
    print(f"    - Status CVXPY:    {problem.status}")
    if problem.value is not None:
        print(f"    - Valor Objetivo:  {problem.value:.4f}")

    if x.value is not None:
        active_indices = np.where(x.value > 0.5)[0]
        print(f"    - Variáveis > 0.5: {len(active_indices):,}")
        if len(active_indices) > 0:
            sample_names = [data["var_names"][i] for i in active_indices[:10]]
            print(f"    - Amostra ativas:  {', '.join(sample_names)} ...")


if __name__ == "__main__":
    main()
