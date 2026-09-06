// IOT-40 — filter/sort state, and the URL it round-trips through.

import { describe, expect, it } from 'vitest'

import type { DeviceQuery } from './query'
import { DEFAULT_QUERY, fromSearchParams, toSearchParams, toggleSort } from './query'

describe('toSearchParams', () => {
  it('emits nothing at the defaults, so the common case is a bare URL', () => {
    expect(toSearchParams(DEFAULT_QUERY).toString()).toBe('')
  })

  it('uses the snake_case names the API expects', () => {
    // camelCase here would be silently ignored by the backend: no error, and
    // filtering would simply appear not to work.
    expect(
      toSearchParams({ ...DEFAULT_QUERY, locationId: 'a', deviceType: 'b' }).toString(),
    ).toBe('location_id=a&device_type=b')
  })

  it('emits every param when all are set', () => {
    expect(
      toSearchParams({
        status: 'inactive',
        locationId: 'cold-store',
        deviceType: 'DHT22',
        sort: 'name',
        order: 'asc',
      }).toString(),
    ).toBe('status=inactive&location_id=cold-store&device_type=DHT22&sort=name&order=asc')
  })

  it('encodes a value that needs it', () => {
    expect(toSearchParams({ ...DEFAULT_QUERY, locationId: 'cold store/1' }).toString()).toBe(
      'location_id=cold+store%2F1',
    )
  })
})

describe('fromSearchParams', () => {
  it.each(['', '?'])('treats %j as the defaults', (search) => {
    expect(fromSearchParams(search)).toEqual(DEFAULT_QUERY)
  })

  it('parses a full query', () => {
    expect(fromSearchParams('?status=active&sort=name&order=asc')).toEqual({
      ...DEFAULT_QUERY,
      status: 'active',
      sort: 'name',
      order: 'asc',
    })
  })

  // A hand-edited URL must not reach the backend, which 400s on these.
  it.each([
    ['?status=banana', 'status', 'all'],
    ['?sort=; DROP TABLE', 'sort', 'last_seen'],
    ['?order=sideways', 'order', 'desc'],
  ])('falls back to the default when %s is junk', (search, key, expected) => {
    expect(fromSearchParams(search)[key as 'status' | 'sort' | 'order']).toBe(expected)
  })

  it('treats an empty filter value as absent, not as a filter matching nothing', () => {
    expect(fromSearchParams('?location_id=').locationId).toBeNull()
  })

  it('decodes an encoded value', () => {
    expect(fromSearchParams('?location_id=cold+store').locationId).toBe('cold store')
  })
})

describe('URL round trip', () => {
  const cases: DeviceQuery[] = [
    DEFAULT_QUERY,
    { ...DEFAULT_QUERY, status: 'inactive' },
    { ...DEFAULT_QUERY, locationId: 'warehouse-a', deviceType: 'BME280' },
    {
      status: 'active',
      locationId: 'cold-store',
      deviceType: 'DHT22',
      sort: 'location',
      order: 'asc',
    },
  ]

  it.each(cases)('reloads identically from its own URL (%j)', (query) => {
    // What the address bar shows must restore exactly, or a shared link lies.
    expect(fromSearchParams(`?${toSearchParams(query).toString()}`)).toEqual(query)
  })
})

describe('toggleSort', () => {
  it('flips direction when the active column is clicked again', () => {
    expect(toggleSort(DEFAULT_QUERY, 'last_seen').order).toBe('asc')
    expect(toggleSort({ ...DEFAULT_QUERY, order: 'asc' }, 'last_seen').order).toBe('desc')
  })

  it('starts a text column ascending', () => {
    expect(toggleSort(DEFAULT_QUERY, 'name')).toEqual({
      ...DEFAULT_QUERY,
      sort: 'name',
      order: 'asc',
    })
  })

  it('starts last_seen descending, so the newest reading is first', () => {
    expect(toggleSort({ ...DEFAULT_QUERY, sort: 'name', order: 'asc' }, 'last_seen')).toEqual({
      ...DEFAULT_QUERY,
      sort: 'last_seen',
      order: 'desc',
    })
  })

  it('keeps the filters when the sort column changes', () => {
    const filtered = { ...DEFAULT_QUERY, status: 'active' as const, locationId: 'x' }
    expect(toggleSort(filtered, 'name')).toMatchObject({ status: 'active', locationId: 'x' })
  })
})
