# IOT-38: frontend — filter controls (status/location/type)

**Sprint:** sprint-04
**Story points:** 3
**Status:** Review
**Depends on:** IOT-37

## Story
As an operator, I want to filter the devices table so that I can focus on currently active devices.

## Acceptance criteria
- [x] Status filter: active / inactive / all
- [x] Location filter and device-type filter
- [x] Filters combine (AND) and reflect in the result set
- [x] Clearing filters returns to the default view

## Implementation notes
- Controlled inputs; filter state lifted to the page container.
- Wiring filters to backend query params is IOT-39.

## What was built
- `src/filters.ts` — `DeviceFilters`, `applyFilters`, `isFiltered`, `optionsFrom`.
- `src/FilterControls.tsx` — three controlled selects plus a Clear button.
- `App.tsx` — owns the filter state and applies it.
- `DevicesTable` gained an `emptyMessage` prop.

## Notes for review
- Filtering is client-side on the already-fetched list, per the ticket. The
  `DeviceFilters` shape deliberately matches the `GET /devices` params so IOT-39
  can send it without reshaping.
- Dropdown options come from the **unfiltered** fleet. Deriving them from the
  visible rows would let one choice empty another dropdown and strand you with
  no way back.
- Two different empty states: "No devices registered yet." vs "No devices match
  these filters." Collapsing them makes a bad filter look like an empty fleet.
- The count reads "2 of 5 devices" while filtering, so a narrow filter never
  reads as a fleet that has shrunk.
- Clear only appears when something is filtered, so it never looks broken.
- `location_id` is nullable and shows as "unassigned"; there is no option for it
  because the backend param cannot express "is null". Such devices appear only
  under "All locations". Worth a follow-up if operators need it.
