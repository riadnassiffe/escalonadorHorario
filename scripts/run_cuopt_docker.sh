#!/usr/bin/env bash
# run_cuopt_docker.sh
# Script para execução do modelo via container oficial NVIDIA NGC cuOpt

set -e

MODEL_MPS="${1:-grade_cuopt_model.mps}"

if [ ! -f "$MODEL_MPS" ]; then
    echo "[-] Arquivo de modelo não encontrado: $MODEL_MPS"
    echo "[!] Execute o escalonador com a opção --cuopt para gerar o modelo MPS."
    exit 1
fi

echo "==========================================================="
echo "  Executando NVIDIA cuOpt via Container NVIDIA NGC"
echo "==========================================================="
echo "[*] Modelo: $MODEL_MPS"

docker run --rm --gpus all \
    -v "$(pwd)":/workspace \
    -w /workspace \
    nvcr.io/nvidia/cuopt:latest \
    python3 scripts/solve_cuopt.py "$MODEL_MPS"
