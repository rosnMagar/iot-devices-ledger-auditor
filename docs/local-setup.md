# Local Development Setup

This doc covers what you need installed to work on each service locally.

---

## C++ (for storage-core)

storage-core builds with a C++20 compiler, CMake, and Make. On Debian/Ubuntu:

```bash
sudo apt install build-essential cmake libssl-dev
```

What this provides: `g++` + `make` (`build-essential`), `cmake` (drives the build), and OpenSSL's `libcrypto`/headers (`libssl-dev`, used for SHA-256).

Verify:

```bash
g++ --version     # g++ (...) 13.x
cmake --version   # cmake 3.2x.x
```

Header-only dependencies (`nlohmann/json`, `doctest`) are **vendored** under `storage-core/third_party/` — nothing to install.

### Building storage-core locally

```bash
cd storage-core
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build       # compile
ctest --test-dir build    # run all tests
./build/storage-core      # run locally on 0.0.0.0:8080
```

---

## Python (for backend-api)

Python 3.12+ recommended. Install via your OS package manager or [python.org](https://www.python.org/downloads/).

```bash
# Debian/Ubuntu
sudo apt install python3 python3-venv python3-pip

# macOS (Homebrew)
brew install python
```

Set up the virtual environment:

```bash
cd backend-api
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
```

Run locally:

```bash
uvicorn app.main:app --reload --port 8000
```

---

## Node (for frontend + auditor)

Node.js 20+ recommended. Install via [nodejs.org](https://nodejs.org/) or `nvm`:

```bash
# nvm (recommended for managing Node versions)
curl -o- https://raw.githubusercontent.com/nvm-sh/nvm/v0.39.7/install.sh | bash
source ~/.bashrc
nvm install 20
nvm use 20
```

Frontend:
```bash
cd frontend
npm install
npm run dev   # dev server on http://localhost:5173
```

Auditor Lambda (TypeScript):
```bash
cd auditor
npm install
npm run build
```

---

## AWS SAM CLI (for auditor Lambda local testing)

```bash
# pip install
pip install aws-sam-cli

# verify
sam --version
```

AWS credentials also needed (`aws configure` or env vars) — see [docs/deployment.md](deployment.md).

---

## Docker (for everything together)

Install Docker Engine + the Compose plugin. The easiest path on Ubuntu:

```bash
# official convenience script
curl -fsSL https://get.docker.com | sh
sudo usermod -aG docker $USER   # allow docker without sudo
# log out and back in to apply group membership
```

Then verify:

```bash
docker --version
docker compose version
```

From the repo root:

```bash
cp .env.example .env      # adjust if needed
docker compose up --build  # builds all images and starts stack
```

Services:
- `http://localhost:8080` — storage-core
- `http://localhost:8000` — backend-api
- `http://localhost` — frontend (port 80)
