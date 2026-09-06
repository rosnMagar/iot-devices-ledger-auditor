// Filter state. The filtering itself is done by the backend (IOT-39) — these
// names match the GET /devices query params so the two cannot drift.

import type { Device } from './api'

export type StatusFilter = 'all' | 'active' | 'inactive'

export interface DeviceFilters {
  status: StatusFilter
  locationId: string | null
  deviceType: string | null
}

export const NO_FILTERS: DeviceFilters = {
  status: 'all',
  locationId: null,
  deviceType: null,
}

export function isFiltered(filters: DeviceFilters): boolean {
  return (
    filters.status !== 'all' ||
    filters.locationId !== null ||
    filters.deviceType !== null
  )
}

// Fed from an unfiltered fetch — see useFleetOptions. Building these from a
// filtered response would let one choice empty another dropdown.
export function optionsFrom(devices: Device[]): {
  locationIds: string[]
  deviceTypes: string[]
} {
  const locationIds = new Set<string>()
  const deviceTypes = new Set<string>()
  for (const device of devices) {
    if (device.location_id !== null) locationIds.add(device.location_id)
    deviceTypes.add(device.device_type)
  }
  return {
    locationIds: [...locationIds].sort(),
    deviceTypes: [...deviceTypes].sort(),
  }
}
