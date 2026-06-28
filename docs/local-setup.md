# Local Development Setup

This doc covers what you need installed to work on each service locally.

---

## Rust (for storage-core)

Install via `rustup` — the official Rust toolchain installer.

```bash
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
```

When prompted, choose option 1 ("Proceed with standard installation").

What gets installed: `rustc` (compiler), `cargo` (build tool + package manager), `rustfmt` (formatter), `clippy` (linter).

After the installer finishes, reload your shell:

```bash
source "$HOME/.cargo/env"
```

Verify:

```bash
rustc --version   # rustc 1.8x.x (...)
cargo --version   # cargo 1.8x.x (...)
```

To update Rust later:

```bash
rustup update
```

### Building storage-core locally

```bash
cd storage-core
cargo build           # debug build
cargo build --release # release build (used by Docker)
cargo test            # run all tests
cargo run             # run locally on 0.0.0.0:8080
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
