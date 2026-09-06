// Controlled inputs only. The filter state lives in the page container so
// IOT-39 can derive the query params and the URL from that one source.

import type { DeviceFilters, StatusFilter } from './filters'
import { isFiltered, NO_FILTERS } from './filters'

const styles = {
  bar: {
    display: 'flex',
    gap: '1rem',
    alignItems: 'flex-end',
    flexWrap: 'wrap',
    margin: '1rem 0',
  } as const,
  field: { display: 'flex', flexDirection: 'column', gap: '0.25rem' } as const,
  label: { color: '#555', fontSize: '0.85em' } as const,
  select: { fontFamily: 'inherit', padding: '0.25rem', minWidth: '10rem' } as const,
  clear: { fontFamily: 'inherit', padding: '0.3rem 0.75rem', cursor: 'pointer' } as const,
}

// '' is the "no constraint" option; the state uses null so it matches the
// query param IOT-39 will send (an absent param, not an empty string).
function toValue(selected: string): string | null {
  return selected === '' ? null : selected
}

export default function FilterControls({
  filters,
  locationIds,
  deviceTypes,
  onChange,
}: {
  filters: DeviceFilters
  locationIds: string[]
  deviceTypes: string[]
  onChange: (next: DeviceFilters) => void
}) {
  return (
    <div style={styles.bar}>
      <div style={styles.field}>
        <label style={styles.label} htmlFor="filter-status">
          Status
        </label>
        <select
          id="filter-status"
          style={styles.select}
          value={filters.status}
          onChange={(e) =>
            onChange({ ...filters, status: e.target.value as StatusFilter })
          }
        >
          <option value="all">All</option>
          <option value="active">Active</option>
          <option value="inactive">Inactive</option>
        </select>
      </div>

      <div style={styles.field}>
        <label style={styles.label} htmlFor="filter-location">
          Location
        </label>
        <select
          id="filter-location"
          style={styles.select}
          value={filters.locationId ?? ''}
          onChange={(e) => onChange({ ...filters, locationId: toValue(e.target.value) })}
        >
          <option value="">All locations</option>
          {locationIds.map((id) => (
            <option key={id} value={id}>
              {id}
            </option>
          ))}
        </select>
      </div>

      <div style={styles.field}>
        <label style={styles.label} htmlFor="filter-type">
          Type
        </label>
        <select
          id="filter-type"
          style={styles.select}
          value={filters.deviceType ?? ''}
          onChange={(e) => onChange({ ...filters, deviceType: toValue(e.target.value) })}
        >
          <option value="">All types</option>
          {deviceTypes.map((type) => (
            <option key={type} value={type}>
              {type}
            </option>
          ))}
        </select>
      </div>

      {/* Only offered when it would do something, so it never reads as broken. */}
      {isFiltered(filters) && (
        <button type="button" style={styles.clear} onClick={() => onChange(NO_FILTERS)}>
          Clear filters
        </button>
      )}
    </div>
  )
}
