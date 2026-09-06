// Client-side filtering. Wiring these to GET /devices query params is IOT-39,
// so the shape here matches the params that endpoint already accepts.

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

// AND across the three, so each one only ever narrows the result.
export function applyFilters(devices: Device[], filters: DeviceFilters): Device[] {
  return devices.filter((device) => {
    if (filters.status !== 'all' && device.status !== filters.status) return false
    if (filters.locationId !== null && device.location_id !== filters.locationId) {
      return false
    }
    if (filters.deviceType !== null && device.device_type !== filters.deviceType) {
      return false
    }
    return true
  })
}

// Options come from the *unfiltered* list. Deriving them from what is currently
// shown would let one choice empty another dropdown and strand the operator.
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
