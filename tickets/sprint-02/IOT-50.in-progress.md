# IOT-50: Attach an Elastic IP to the EC2 instance

**Sprint:** sprint-02
**Story points:** 1
**Status:** In Progress
**Depends on:** —

## Story
As an operator, I want the EC2 instance to keep a fixed public address so that restarting it doesn't break deploys, the frontend and the Lambda at once.

## Acceptance criteria
- [x] `docs/deployment.md` documents allocating and associating an Elastic IP, before any step that references the address
- [x] The four places the IP is baked into are listed, so the blast radius is obvious
- [x] Cost behaviour noted (billed while associated; billed more while idle)
- [ ] **Elastic IP actually allocated and associated in AWS** — console/CLI work, not repo work
- [ ] `EC2_HOST` secret updated to the Elastic IP
- [ ] A prod deploy re-run so the frontend image is rebuilt against the new address

## Why
The auto-assigned public IPv4 changes whenever the instance is stopped and
started. That happened this sprint and produced
`dial tcp ***:22: i/o timeout` in `deploy-ec2` — which reads as a network or
firewall fault, not as a stale secret, so it costs real time to diagnose.

The address is not in one place. It is in four:

| Where | Failure mode |
|---|---|
| `EC2_HOST` → `appleboy/ssh-action` (`deploy.yml:110`) | SSH deploy times out |
| `EC2_HOST` → `FRONTEND_API_URL` → `VITE_API_BASE_URL` (`deploy.yml:25`) | frontend calls the old IP — **baked in at build time**, so a restart doesn't fix it, only a rebuild does |
| `EC2_HOST` → Lambda `StorageCoreUrl` (`deploy.yml:174`) | auditor can't reach `/verify` |
| ESP32 `secrets.h` (Phase 1.5, ADR 0007) | devices post into the void |

The frontend one is the nastiest: the image is rebuilt from a stale value only
when the workflow runs, so the symptom outlives the fix unless a deploy follows.

## Remaining work is not in this repo
The doc change is done and committed. Allocating the address is AWS console/CLI
work:

```bash
aws ec2 allocate-address --domain vpc --region us-east-2 \
  --tag-specifications 'ResourceType=elastic-ip,Tags=[{Key=Name,Value=iot-ledger}]'

aws ec2 associate-address --region us-east-2 \
  --instance-id i-XXXXXXXX --allocation-id eipalloc-XXXXXXXX

gh secret set EC2_HOST --body "<elastic-ip>"
gh workflow run deploy.yml --ref prod
```

Ticket stays `.in-progress` until those three boxes are ticked — the
documentation alone doesn't stop the outage recurring.

## Follow-ups
- With a stable address, Phase 5's TLS/reverse-proxy work becomes possible: a
  certificate needs a name that doesn't move, and a DNS record pointing at a
  rotating IP is its own outage.
- Consider a DNS name in front of the Elastic IP so even the address becomes an
  implementation detail — then `EC2_HOST` never changes again.
