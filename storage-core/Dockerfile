# syntax=docker/dockerfile:1
FROM --platform=$BUILDPLATFORM rust:1-slim AS builder
WORKDIR /app
COPY Cargo.toml Cargo.lock* ./
COPY src ./src
RUN cargo build --release

FROM debian:bookworm-slim
RUN apt-get update && apt-get install -y --no-install-recommends ca-certificates \
    && rm -rf /var/lib/apt/lists/*
COPY --from=builder /app/target/release/storage-core /usr/local/bin/storage-core
EXPOSE 8080
CMD ["storage-core"]
