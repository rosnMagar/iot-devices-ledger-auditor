# Weekend 1 — Core Data Model

Status: In progress (IOT-9 done).

## Goal

Build `block.hpp` + `chain.hpp/.cpp`: the `Block`/`EventPayload` types, SHA-256 hashing, and an in-memory `Chain` with `append()`/`verify()`. Unit tests (doctest) for hash determinism, tamper detection, and multi-block integrity. Stretch: a small `main` demo printing a 5-block chain + verify result.

## Concepts you need (read/skim before starting)

- **Structs & aggregate initialization** — plain structs, brace-init, `std::string` / `std::uint64_t` fields
- **`nlohmann/json`** — `NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE`, `json::dump()`, and `nlohmann::json` as a dynamic value (for `metadata`)
- **OpenSSL `libcrypto`** — `SHA256()` (or the EVP API) over a byte buffer, then hex-encoding the 32-byte digest
- **`std::vector`** — `push_back`, `back()`, iteration (for `Chain`)
- **`std::optional`** — for `first_invalid_index` in the verify result
- **CMake basics** — `add_executable`, `target_include_directories`, linking `-lcrypto`, and `enable_testing()` / `add_test` for ctest

## What we built and why (fill in after)

- TBD

## Gotchas hit (fill in after)

- TBD
