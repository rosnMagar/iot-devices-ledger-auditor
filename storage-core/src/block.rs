#![allow(dead_code)] // wired up in chain.rs (IOT-10+)

use serde::{Deserialize, Serialize};

/// The domain content of an audit event — what `POST /events` accepts.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct EventPayload {
    pub event_type: String,
    pub location_id: String,
    pub actor: String,
    pub description: String,
    pub metadata: serde_json::Value,
}

/// One entry in the hash-chained ledger.
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Block {
    pub index: u64,
    pub timestamp: chrono::DateTime<chrono::Utc>,
    pub event: EventPayload,
    pub prev_hash: String,
    pub hash: String,
}
