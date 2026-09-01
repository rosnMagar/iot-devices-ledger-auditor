# IOT-7: Verify walking skeleton end-to-end

**Sprint:** sprint-01
**Story points:** 1
**Status:** Done
**Depends on:** IOT-6

## Story
As a developer, I want to confirm the deployed stack works end-to-end so that the walking skeleton is proven before building real logic.

## Acceptance criteria
- [x] `curl http://<EC2-IP>:8080/health` → `{"status":"ok"}`
- [x] `curl http://<EC2-IP>:8000/blocks` → blocks proxied from storage-core
- [x] `http://<EC2-IP>` renders the frontend showing blocks + users
- [x] Result recorded; `docs/roadmap.md` Phase 0 checkboxes ticked

## Implementation notes
- Follow `docs/deployment.md` "Verify the deployment".
- A failure here points at inter-container networking or the `VITE_API_BASE_URL` build arg.
