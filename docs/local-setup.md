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
npm ci        # npm ci, not install: package-lock.json is committed
npm run dev   # dev server on http://localhost:5173
npm test      # vitest, once
npm run test:watch
```

## Simulated sensor readings (dev tool)

Firmware is blocked on hardware (IOT-30…33), so `backend-api/tools/simulate_readings.py`
stands in for the fleet. It registers devices/locations and posts `SENSOR_READING`
events matching [ADR 0009](decisions/0009-sensor-reading-payload.md).

It runs inside the backend-api container — the only place that can reach both the
devices database and storage-core — and is piped in over stdin, so it does not
need to be baked into the image:

```bash
docker compose exec -T -e SIM_CONFIRM=1 backend-api \
    python - < backend-api/tools/simulate_readings.py
```

It models multi-sensor boards: temperature, humidity, pressure, a 3-axis
accelerometer and a camera, each on **its own interval** — the accelerometer
reports every 2s while humidity waits 30s. Sensors fail independently, and a
camera emits `CAMERA_EVENT`s only; no frame ever enters a block (ADR 0011).

Tunables, all environment variables: `SIM_DEVICES` (4), `SIM_DURATION` (300s),
`SIM_TICK` (1s scheduler tick), `SIM_FAILURE_RATE` (0.02), `SIM_SEED`
(unset = random).

**`SIM_CONFIRM=1` is mandatory.** Every reading is appended to an immutable hash
chain and can never be removed — there is no undo. The script also refuses to run
when the target looks like production (a non-localhost `CORS_ORIGINS`) unless
`SIM_FORCE=1` is set as well.

Registration is idempotent, so re-running it will not duplicate devices.

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

## Simulated camera (dev tool)

`backend-api/tools/fake_camera.py` stands in for a camera module, so the live
view can be developed without hardware:

```bash
python3 backend-api/tools/fake_camera.py     # serves :8090/stream
SIM_CAMERA_URL=http://localhost:8090/stream  # tell the simulator to announce it
```

It serves `multipart/x-mixed-replace`, the same shape an ESP32-CAM's MJPEG
endpoint uses, so the frontend needs no special case for the fake. Frames are
PNG rather than JPEG because the standard library can encode PNG and cannot
encode JPEG; browsers render either in a multipart stream. A moving bar makes it
obvious at a glance that the stream is live rather than a still.

Nothing it serves is recorded, and none of it goes near the ledger (ADR 0011).
