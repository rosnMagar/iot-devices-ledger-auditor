# syntax=docker/dockerfile:1
# Single-stage image: --platform=$BUILDPLATFORM would pin the whole image to the
# x86 runner, and its pip wheels with it. Must be the target platform.
FROM python:3.12-slim
WORKDIR /app
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt
COPY app ./app
EXPOSE 8000
CMD ["uvicorn", "app.main:app", "--host", "0.0.0.0", "--port", "8000"]
