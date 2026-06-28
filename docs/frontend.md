# frontend — Status: Not Started

## Overview (TBD)

Phase 0: stub page fetching backend-api's `/blocks` and `/users` and rendering them. Real dashboard lands in Phase 3.

## Stack / Decisions

- React, Vite, TypeScript
- Multi-stage Dockerfile, served via nginx
- Live event feed via WebSocket (backend-api relay of storage-core's `/ws/blocks`)

## Open Questions

- Auth/session handling (depends on backend-api Phase 2 design)
- Views needed: locations, users, settings, devices, live feed, audit reports
