# Sistema Web — Escalonador Inteligente de Horários

## Arquitetura
- **Core Matemático (`build/escalonador_solver_cli`)**: binário C++20 sem Qt reutilizando `scheduler_core` (CP-SAT, HiGHS, SCIP, cuOpt, Greedy).
- **Backend (`web/backend`)**: Python 3.12 + FastAPI + SQLAlchemy 2.0 (async) + Alembic + PostgreSQL + Celery + Redis.
- **Frontend (`web/frontend`)**: TypeScript + React 19 + Vite + `@dnd-kit/core` + TanStack Query + Zustand + Tailwind CSS.

## Desenvolvimento Local

1. **Compilar o CLI C++:**
   ```bash
   cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=build/Release/generators/conan_toolchain.cmake -DCMAKE_BUILD_TYPE=Release
   cmake --build build -j$(nproc)
   ```

2. **Subir PostgreSQL e Redis (ou usar `docker compose up -d postgres redis`):**
   ```bash
   cd web/backend
   uv sync --python 3.12
   uv run alembic upgrade head
   uv run uvicorn app.main:app --reload --port 8000
   ```

3. **Worker Celery (em outro terminal):**
   ```bash
   cd web/backend
   uv run celery -A app.workers.celery_app worker --loglevel=info --concurrency=2 -Q optimization,default
   ```

4. **Frontend React:**
   ```bash
   cd web/frontend
   npm install
   npm run dev
   ```

## Produção / Docker Compose
```bash
docker compose up --build
# Opcional: monitoramento Celery Flower
docker compose --profile monitoring up --build
```
