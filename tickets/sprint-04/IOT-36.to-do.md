# IOT-36: backend-api — `GET /devices` filters + `sort`/`order`

**Sprint:** sprint-04
**Story points:** 3
**Status:** To Do
**Depends on:** IOT-35

## Story
As a dashboard, I want to query devices with filtering and sorting so that operators can find active devices fast.

## Acceptance criteria
- [ ] `GET /devices` supports filters: `status` (active/inactive/all), `location_id`, `device_type`
- [ ] Supports `sort` (`last_seen`/`name`/`location`) + `order` (`asc`/`desc`)
- [ ] Defaults: `status=all`, `sort=last_seen`, `order=desc`
- [ ] Invalid `sort`/`order`/`status` → 400 with a clear message

## Implementation notes
- Apply filters at the query level where possible; `status` may need post-filtering on derived `last_seen`.
- Document params in `docs/backend-api.md`.
