mod block;

use axum::{routing::get, Json, Router};
use serde_json::{json, Value};
use std::net::SocketAddr;

#[tokio::main]
async fn main() {
    tracing_subscriber::fmt()
        .with_env_filter(tracing_subscriber::EnvFilter::from_default_env())
        .init();

    let app = Router::new()
        .route("/health", get(health))
        .route("/blocks", get(blocks))
        .route("/verify", get(verify));

    let bind_addr = std::env::var("BIND_ADDR").unwrap_or_else(|_| "0.0.0.0:8080".to_string());
    let addr: SocketAddr = bind_addr.parse().expect("invalid BIND_ADDR");

    tracing::info!("storage-core stub listening on {addr}");
    let listener = tokio::net::TcpListener::bind(addr).await.unwrap();
    axum::serve(listener, app).await.unwrap();
}

async fn health() -> Json<Value> {
    Json(json!({ "status": "ok" }))
}

// Stub data — Phase 1 replaces this with the real hash-chained ledger.
async fn blocks() -> Json<Value> {
    let genesis_hash = "0".repeat(64);
    Json(json!({
        "blocks": [
            {
                "index": 0,
                "timestamp": "2026-01-01T00:00:00Z",
                "event": {
                    "event_type": "GENESIS",
                    "location_id": "factory-1",
                    "actor": "system",
                    "description": "Genesis block",
                    "metadata": {}
                },
                "prev_hash": genesis_hash,
                "hash": "stub-hash-0"
            },
            {
                "index": 1,
                "timestamp": "2026-01-01T00:01:00Z",
                "event": {
                    "event_type": "VALVE_OPENED",
                    "location_id": "factory-1",
                    "actor": "operator-1",
                    "description": "Valve A opened",
                    "metadata": {}
                },
                "prev_hash": "stub-hash-0",
                "hash": "stub-hash-1"
            }
        ],
        "chain_length": 2
    }))
}

async fn verify() -> Json<Value> {
    Json(json!({
        "valid": true,
        "chain_length": 2,
        "checked_blocks": 2,
        "first_invalid_index": null,
        "verified_at": "2026-01-01T00:02:00Z"
    }))
}
