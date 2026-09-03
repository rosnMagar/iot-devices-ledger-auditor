# syntax=docker/dockerfile:1
# Single-stage image: --platform=$BUILDPLATFORM would pin the whole image to the
# x86 runner, and its pip wheels with it. Must be the target platform.
FROM python:3.12-slim
WORKDIR /app
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt
COPY app ./app

# The SQLite file lives here. Created in the image so the app starts even with
# no volume mounted; docker-compose.yml mounts a named volume over it so the
# data actually survives a redeploy (IOT-55).
RUN mkdir -p /app/data

EXPOSE 8000
CMD ["uvicorn", "app.main:app", "--host", "0.0.0.0", "--port", "8000"]
