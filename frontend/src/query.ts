// One source of truth for filter + sort state. Both the GET /devices request and
// the URL are derived from this, so they can never disagree.

import type { DeviceFilters, StatusFilter } from './filters'
import { NO_FILTERS } from './filters'

export type SortKey = 'last_seen' | 'name' | 'location'
export type SortOrder = 'asc' | 'desc'

export interface DeviceQuery extends DeviceFilters {
  sort: SortKey
  order: SortOrder
}

// Must match the backend's defaults, or the first render disagrees with the URL.
export const DEFAULT_QUERY: DeviceQuery = {
  ...NO_FILTERS,
  sort: 'last_seen',
  order: 'desc',
}

const STATUSES: StatusFilter[] = ['all', 'active', 'inactive']
const SORT_KEYS: SortKey[] = ['last_seen', 'name', 'location']
const ORDERS: SortOrder[] = ['asc', 'desc']

// The backend 400s on an unknown value, so a hand-edited URL would break the
// page. Anything unrecognised falls back to the default instead.
function oneOf<T extends string>(raw: string | null, allowed: T[], fallback: T): T {
  return allowed.includes(raw as T) ? (raw as T) : fallback
}

// Defaults are omitted so the common case stays a bare URL.
export function toSearchParams(query: DeviceQuery): URLSearchParams {
  const params = new URLSearchParams()
  if (query.status !== DEFAULT_QUERY.status) params.set('status', query.status)
  if (query.locationId !== null) params.set('location_id', query.locationId)
  if (query.deviceType !== null) params.set('device_type', query.deviceType)
  if (query.sort !== DEFAULT_QUERY.sort) params.set('sort', query.sort)
  if (query.order !== DEFAULT_QUERY.order) params.set('order', query.order)
  return params
}

export function fromSearchParams(search: string): DeviceQuery {
  const params = new URLSearchParams(search)
  const locationId = params.get('location_id')
  const deviceType = params.get('device_type')
  return {
    status: oneOf(params.get('status'), STATUSES, DEFAULT_QUERY.status),
    // An empty string is not a real filter, and the backend would match nothing.
    locationId: locationId ? locationId : null,
    deviceType: deviceType ? deviceType : null,
    sort: oneOf(params.get('sort'), SORT_KEYS, DEFAULT_QUERY.sort),
    order: oneOf(params.get('order'), ORDERS, DEFAULT_QUERY.order),
  }
}

// Clicking the active column flips direction; a new column starts descending
// for last_seen (most recent first) and ascending for the text columns.
export function toggleSort(query: DeviceQuery, key: SortKey): DeviceQuery {
  if (query.sort === key) {
    return { ...query, order: query.order === 'asc' ? 'desc' : 'asc' }
  }
  return { ...query, sort: key, order: key === 'last_seen' ? 'desc' : 'asc' }
}
