# syntax=docker/dockerfile:1
FROM --platform=$BUILDPLATFORM gcc:13 AS builder
RUN apt-get update && apt-get install -y --no-install-recommends cmake \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /app
COPY CMakeLists.txt ./
COPY src ./src
COPY include ./include
COPY third_party ./third_party
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DSTORAGE_CORE_BUILD_TESTS=OFF && cmake --build build -j

FROM debian:bookworm-slim
RUN apt-get update && apt-get install -y --no-install-recommends ca-certificates \
    && rm -rf /var/lib/apt/lists/*
COPY --from=builder /app/build/storage-core /usr/local/bin/storage-core
EXPOSE 8080
CMD ["storage-core"]
