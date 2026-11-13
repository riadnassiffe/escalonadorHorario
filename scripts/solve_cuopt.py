#!/usr/bin/env python3
"""
solve_cuopt.py
Script de integração e resolução do modelo matemático via NVIDIA cuOpt.

Lê a formulação exportada pelo escalonador (grade_cuopt_model.mps) e a submete
ao motor de otimização matemática acelerado por GPU da NVIDIA (cuOpt LP / MILP solver).

Execução recomendada via uv:
    uv run scripts/solve_cuopt.py
    uv run scripts/solve_cuopt.py grade_cuopt_model.mps --time-limit 30
    uv run scripts/solve_cuopt.py --fallback
"""

import sys
import os
import time
import argparse

def solve_with_milp_fallback(data_model, time_limit=60.0):
    print("\n[*] Acionando motor de resolução MILP integrado (HiGHS / Dual Simplex)...")
    try:
        import numpy as np
        from scipy.optimize import milp, LinearConstraint, Bounds
        from scipy.sparse import csr_matrix

        c = np.array(data_model.get_objective_coefficients(), dtype=float)
        if data_model.get_sense() == 1:  # Maximize
            c = -c

        lb = np.array(data_model.get_variable_lower_bounds(), dtype=float)
        ub = np.array(data_model.get_variable_upper_bounds(), dtype=float)
        integrality = np.ones_like(c, dtype=int)

        offsets = np.array(data_model.get_constraint_matrix_offsets(), dtype=int)
        indices = np.array(data_model.get_constraint_matrix_indices(), dtype=int)
        values = np.array(data_model.get_constraint_matrix_values(), dtype=float)
        n_rows = len(data_model.get_row_names())
        n_cols = len(c)
        A_csr = csr_matrix((values, indices, offsets), shape=(n_rows, n_cols))

        con_lb = np.array(data_model.get_constraint_lower_bounds(), dtype=float)
        con_ub = np.array(data_model.get_constraint_upper_bounds(), dtype=float)

        constraints = LinearConstraint(A_csr, con_lb, con_ub)
        bounds = Bounds(lb, ub)

        t0 = time.time()
        res = milp(
            c=c,
            integrality=integrality,
            constraints=constraints,
            bounds=bounds,
            options={"time_limit": time_limit, "disp": False}
        )
        elapsed = time.time() - t0

        print(f"\n[+] Otimização concluída em {elapsed:.2f} segundos!")
        print(f"    - Sucesso:          {res.success}")
        print(f"    - Status:           {res.status} ({res.message})")
        if res.x is not None:
            obj_val = res.fun if data_model.get_sense() == 0 else -res.fun
            print(f"    - Objetivo Primal:  {obj_val:.2f}")
            non_zeros = int(np.sum(res.x > 0.5))
            print(f"    - Variáveis ativas: {non_zeros:,}")
    except Exception as e:
        print(f"[!] Falha na resolução fallback: {e}")

def main():
    parser = argparse.ArgumentParser(description="Resolvedor de Grade Escolar via NVIDIA cuOpt (GPU)")
    parser.add_argument("mps_file", nargs="?", default="grade_cuopt_model.mps", help="Caminho do arquivo MPS gerado")
    parser.add_argument("--time-limit", type=float, default=60.0, help="Tempo limite de resolução em segundos")
    parser.add_argument("--threads", type=int, default=4, help="Número de threads de CPU de suporte")
    parser.add_argument("--fallback", action="store_true", help="Força a execução via motor MILP integrado")
    args = parser.parse_args()

    if not os.path.exists(args.mps_file):
        print(f"[-] Arquivo de modelo não encontrado: {args.mps_file}")
        print("[!] Dica: Execute o escalonador com '--cuopt' para gerar o arquivo MPS:")
        print("      ./build/escalonadorHorarios --test --cuopt --time-limit 5")
        sys.exit(1)

    print("=" * 70)
    print("  NVIDIA cuOpt - GPU-Accelerated Mathematical Optimization (MILP/LP)")
    print("=" * 70)
    size_mb = os.path.getsize(args.mps_file) / (1024 * 1024)
    print(f"[*] Arquivo MPS: {args.mps_file} ({size_mb:.2f} MB)")
    print(f"[*] Tempo limite configurado: {args.time_limit} segundos")

    # 1. Importação oficial da biblioteca cuOpt
    try:
        from cuopt import linear_programming
        from cuopt.linear_programming.solver.solver_parameters import (
            CUOPT_TIME_LIMIT,
            CUOPT_NUM_CPU_THREADS,
            CUOPT_LOG_TO_CONSOLE,
        )
        print("[+] Módulo oficial NVIDIA cuOpt importado com sucesso!")
    except ImportError as e:
        print(f"[!] Erro ao importar cuOpt: {e}")
        print("[i] Execute com o uv:")
        print("      uv run scripts/solve_cuopt.py")
        sys.exit(1)

    # 2. Carregamento do modelo matemático via API nativa do cuOpt
    # Em vez de problem.read_mps, o cuOpt utiliza cuopt.linear_programming.Read()
    print("[*] Lendo modelo matemático via cuopt.linear_programming.Read...")
    t_start_read = time.time()
    try:
        data_model = linear_programming.Read(args.mps_file)
    except Exception as e:
        print(f"[-] Falha na leitura do arquivo MPS pelo parser do cuOpt: {e}")
        sys.exit(1)

    n_vars = len(data_model.get_variable_names())
    n_rows = len(data_model.get_row_names())
    t_read = time.time() - t_start_read
    print(f"[+] Modelo carregado na GPU/DataModel em {t_read:.2f}s:")
    print(f"    - Variáveis de decisão: {n_vars:,}")
    print(f"    - Restrições lineares:  {n_rows:,}")
    print(f"    - Sentido da função:    {'Minimização' if data_model.get_sense() == 0 else 'Maximização'}")

    if args.fallback:
        solve_with_milp_fallback(data_model, args.time_limit)
        return

    # 3. Configurações do Solver cuOpt
    settings = linear_programming.SolverSettings()
    try:
        settings.set_parameter(CUOPT_TIME_LIMIT, str(args.time_limit))
        settings.set_parameter(CUOPT_NUM_CPU_THREADS, str(args.threads))
        settings.set_parameter(CUOPT_LOG_TO_CONSOLE, "1")
    except Exception as e:
        print(f"[!] Aviso ao configurar parâmetros do cuOpt: {e}")

    # 4. Resolução no hardware GPU NVIDIA
    print("\n[*] Disparando resolução via GPU (cuopt.linear_programming.Solve)...")
    t_solve_start = time.time()
    try:
        solution = linear_programming.Solve(data_model, settings)
        elapsed = time.time() - t_solve_start

        print(f"\n[+] Otimização cuOpt concluída com sucesso em {elapsed:.2f} segundos!")
        print(f"    - Status de terminação: {solution.get_termination_status()}")
        print(f"    - Motivo:               {solution.get_termination_reason()}")
        print(f"    - Objetivo Primal (Z):  {solution.get_primal_objective()}")

        sol_vars = solution.get_vars()
        non_zeros = sum(1 for v in sol_vars.values() if abs(v) > 0.5)
        print(f"    - Variáveis ativas (>0.5): {non_zeros:,}")

    except RuntimeError as e:
        err_msg = str(e)
        print(f"\n[!] Exceção de execução do kernel CUDA cuOpt: {err_msg}")
        if "cudaErrorNoKernelImageForDevice" in err_msg:
            print("\n" + "-" * 70)
            print("  NOTA TÉCNICA SOBRE ARQUITETURA DE HARDWARE NVIDIA GPU:")
            print("-" * 70)
            print("  A biblioteca binária do NVIDIA cuOpt é compilada pela NVIDIA para")
            print("  arquiteturas com Compute Capability >= 7.0 (Volta, Turing, Ampere,")
            print("  Ada Lovelace, Hopper, Blackwell — ex: RTX 2060+, RTX 30xx, RTX 40xx,")
            print("  A100, H100, T4, V100).")
            print("  A GPU atual desta máquina (NVIDIA GeForce MX330) possui arquitetura")
            print("  Pascal (Compute Capability 6.1).")
            print("-" * 70)

            # Executa fallback automático para não deixar o usuário sem resposta
            solve_with_milp_fallback(data_model, args.time_limit)
        else:
            raise

if __name__ == "__main__":
    main()
