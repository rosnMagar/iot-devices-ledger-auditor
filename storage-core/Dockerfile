# syntax=docker/dockerfile:1
# NOT --platform=$BUILDPLATFORM: this stage compiles a native binary, so it must
# run on the *target* platform. Pinning it to the build platform produced an
# x86 binary inside an arm64 image — "exec format error" on the Graviton box.
FROM gcc:13 AS builder
RUN apt-get update && apt-get install -y --no-install-recommends cmake \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /app
COPY CMakeLists.txt ./
COPY src ./src
COPY include ./include
COPY third_party ./third_party
# -static-libstdc++/-static-libgcc: gcc:13 links against GCC 13's libstdc++, but
# the bookworm-slim runtime below only ships GCC 12's (GLIBCXX_3.4.30). Without
# this the binary builds fine and dies on startup with "GLIBCXX_3.4.32 not
# found". Baking the C++ runtime in costs ~2MB and decouples the two stages, so
# bumping either base image can't reintroduce the mismatch. Safe here because
# nothing else in the image is C++ — only C libraries are dynamically linked.
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DSTORAGE_CORE_BUILD_TESTS=OFF \
      -DCMAKE_EXE_LINKER_FLAGS="-static-libstdc++ -static-libgcc" \
    && cmake --build build -j

FROM debian:bookworm-slim
RUN apt-get update && apt-get install -y --no-install-recommends ca-certificates \
    && rm -rf /var/lib/apt/lists/*
COPY --from=builder /app/build/storage-core /usr/local/bin/storage-core
EXPOSE 8080
# WebSocket listener (IOT-28). A second port because cpp-httplib and
# IXWebSocket each own a listening socket and cannot share 8080.
EXPOSE 8081
CMD ["storage-core"]
