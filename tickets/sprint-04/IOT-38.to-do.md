# IOT-38: frontend — filter controls (status/location/type)

**Sprint:** sprint-04
**Story points:** 3
**Status:** To Do
**Depends on:** IOT-37

## Story
As an operator, I want to filter the devices table so that I can focus on currently active devices.

## Acceptance criteria
- [ ] Status filter: active / inactive / all
- [ ] Location filter and device-type filter
- [ ] Filters combine (AND) and reflect in the result set
- [ ] Clearing filters returns to the default view

## Implementation notes
- Controlled inputs; filter state lifted to the page container.
- Wiring filters to backend query params is IOT-39.
