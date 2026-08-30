# Deployment — AWS Setup & Redeploy Runbook

One-time manual setup before the first `prod` deploy. Do this before setting GitHub secrets.

---

## Live environment

| Item | Value |
|---|---|
| Region | `us-east-2` (Ohio) |
| Instance | `t4g.micro`, Ubuntu 24.04 ARM64 |
| Public IP | `52.15.229.12` |
| SSH | `ssh -i <key>.pem ubuntu@52.15.229.12` |

> Ports open: 22 (my IP), 80, 8000, 8080.

---

## 1. Launch EC2 instance

In the AWS console (or `aws ec2 run-instances`):

- **AMI**: Ubuntu 24.04 LTS, ARM64 (search "ubuntu-24.04-arm64")
- **Instance type**: `t4g.micro` (free tier eligible, Graviton/ARM)
- **Key pair**: create a new one, download the `.pem` file — you'll need it for SSH and for the GitHub `EC2_SSH_KEY` secret
- **Storage**: 20 GB gp3 is fine

**Security group** — inbound rules:

| Port | Protocol | Source | Purpose |
|---|---|---|---|
| 22 | TCP | Your IP only | SSH access |
| 80 | TCP | 0.0.0.0/0 | Frontend |
| 8000 | TCP | 0.0.0.0/0 | backend-api |
| 8080 | TCP | 0.0.0.0/0 | storage-core (Lambda auditor + ESP32) |

> Ports 8000/8080 stay open for Phase 0 demo; narrow them in Phase 5 once a reverse proxy is in place.

---

## 1b. Attach an Elastic IP

**Do this before anything else references the address.** A stopped-and-started
instance gets a *new* auto-assigned public IP, and that address is baked into
four places:

| Where | What breaks |
|---|---|
| `EC2_HOST` secret → `appleboy/ssh-action` | deploy fails with `dial tcp ***:22: i/o timeout` |
| `EC2_HOST` → `FRONTEND_API_URL` → `VITE_API_BASE_URL` | frontend calls the old IP; **baked in at image build time**, so it needs a rebuild, not a restart |
| `EC2_HOST` → Lambda `StorageCoreUrl` | auditor can't reach `/verify` |
| ESP32 firmware `secrets.h` (Phase 1.5) | devices post to nothing |

An Elastic IP is a fixed address you own, so a restart changes nothing.

```bash
# 1. Allocate
aws ec2 allocate-address --domain vpc --region us-east-2 \
  --tag-specifications 'ResourceType=elastic-ip,Tags=[{Key=Name,Value=iot-ledger}]'

# 2. Associate it with the instance (use the AllocationId printed above)
aws ec2 associate-address --region us-east-2 \
  --instance-id i-XXXXXXXX --allocation-id eipalloc-XXXXXXXX

# 3. Confirm
aws ec2 describe-addresses --region us-east-2 \
  --query 'Addresses[].[PublicIp,InstanceId,AllocationId]' --output table
```

Then point the secret at it and rebuild, so the frontend image picks up the new
URL:

```bash
gh secret set EC2_HOST --body "<elastic-ip>"
gh workflow run deploy.yml --ref prod
```

> **Cost.** AWS bills for public IPv4 addresses (including in-use Elastic IPs) —
> currently around $0.005/hour, roughly $3.60/month. The instance is already
> paying that for its auto-assigned address, so attaching an Elastic IP is close
> to cost-neutral while it stays associated. An Elastic IP that is allocated but
> **not** associated is billed at a higher rate — release it if you terminate the
> instance.

---

## 2. Install Docker on the EC2 instance

SSH in: `ssh -i your-key.pem ubuntu@<EC2-IP>`

```bash
curl -fsSL https://get.docker.com | sh
sudo usermod -aG docker ubuntu
# log out and back in so the group takes effect
exit
ssh -i your-key.pem ubuntu@<EC2-IP>
docker --version   # should work without sudo
```

---

## 3. Clone the repo

```bash
sudo mkdir -p /opt/audit-ledger
sudo chown ubuntu:ubuntu /opt/audit-ledger
cd /opt/audit-ledger
git clone https://github.com/rosnMagar/iot-devices-ledger-auditor.git .
git checkout prod
```

---

## 4. Create the `.env` file on EC2

```bash
cp .env.example .env
nano .env
```

Set these values (the rest can stay as defaults):

```
TAG=prod
VITE_API_BASE_URL=http://<EC2-IP>:8000
```

`TAG=prod` tells `docker compose pull` to pull the `:prod`-tagged images from GHCR.

`TAG` ships blank in `.env.example` on purpose. Leave it blank and `docker
compose` stops with `TAG is not set` instead of guessing — it used to default to
`dev`, which meant a box with a missing `.env` pulled development images, started
cleanly, and gave no sign anything was wrong.

The automated deploy (`.github/workflows/deploy.yml`) exports `TAG=prod` itself,
so a wrong `.env` on the box can no longer send the wrong images to production.
Setting it here still matters for anything you run by hand over SSH.

---

## 5. Create an IAM user for Lambda deploys

In the AWS IAM console:

1. Create user `iot-ledger-deployer`
2. Attach these policies (or a custom policy scoped to these):
   - `AWSLambda_FullAccess`
   - `AWSCloudFormationFullAccess`
   - `AmazonS3FullAccess` (SAM uses S3 for deployment artifacts; scope to a specific bucket in Phase 5)
3. Create an access key → save **Access Key ID** and **Secret Access Key**

---

## 6. Set GitHub secrets & variables

In the repo → Settings → Secrets and variables → Actions:

**Secrets:**

| Name | Value |
|---|---|
| `EC2_HOST` | EC2 public IP |
| `EC2_USER` | `ubuntu` |
| `EC2_SSH_KEY` | Contents of the `.pem` key file (`cat your-key.pem`) |
| `AWS_ACCESS_KEY_ID` | IAM user access key |
| `AWS_SECRET_ACCESS_KEY` | IAM user secret key |

**Variables (non-secret):**

| Name | Value |
|---|---|
| `AWS_REGION` | `us-east-2` (the region the EC2 box lives in) |

---

## 7. Make GHCR packages public (optional but simplest)

By default GHCR packages are private. For `docker compose pull` on EC2 without login, set the packages to public:

- Go to your GitHub profile → Packages → each service image → Package settings → Change visibility → Public

Or add a `docker login ghcr.io` step to the EC2 setup (see [docs/branching-and-cicd.md](branching-and-cicd.md)).

---

## Redeploy runbook (after first setup)

To redeploy manually without pushing to `prod`:

```bash
ssh -i your-key.pem ubuntu@<EC2-IP>
cd /opt/audit-ledger
git pull origin prod
docker compose pull
docker compose up -d
docker compose ps   # confirm all 3 containers are up
```

To check logs: `docker compose logs -f storage-core`

---

## Verify the deployment

```bash
curl http://<EC2-IP>:8080/health   # {"status":"ok"}
curl http://<EC2-IP>:8000/blocks   # proxied blocks from storage-core
# visit http://<EC2-IP> in a browser — should show the frontend stub
```

Lambda:
```bash
aws lambda invoke \
  --function-name iot-ledger-auditor-AuditorFunction \
  --region us-east-2 \
  /tmp/out.json && cat /tmp/out.json
# should show the /verify response from storage-core
```
