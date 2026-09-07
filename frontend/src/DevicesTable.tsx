// Presentational only: takes devices, renders them. No fetching, so it can be
// reused wherever a device list already exists.

import type { Device } from './api'
import type { SortKey, SortOrder } from './query'
import { relativeTime } from './relativeTime'

const styles = {
  table: { borderCollapse: 'collapse', width: '100%' } as const,
  th: {
    textAlign: 'left',
    padding: '0.5rem',
    borderBottom: '2px solid #ddd',
    color: '#555',
  } as const,
  sortButton: {
    font: 'inherit',
    color: 'inherit',
    background: 'none',
    border: 'none',
    padding: 0,
    cursor: 'pointer',
    display: 'inline-flex',
    gap: '0.35rem',
  } as const,
  arrow: { color: '#888' } as const,
  td: { padding: '0.5rem', borderBottom: '1px solid #eee' } as const,
  badge: {
    padding: '0.15rem 0.5rem',
    borderRadius: '999px',
    fontSize: '0.85em',
  } as const,
  empty: { color: '#888', padding: '1rem 0' } as const,
  unset: { color: '#bbb' } as const,
}

// Colour alone would exclude colourblind users, so the word carries the meaning.
function StatusBadge({ status }: { status: Device['status'] }) {
  const active = status === 'active'
  return (
    <span
      style={{
        ...styles.badge,
        background: active ? '#d8f5dd' : '#eee',
        color: active ? '#136c28' : '#666',
      }}
    >
      {status}
    </span>
  )
}

// Only last_seen, name and location are sortable — those are the keys the
// backend accepts. Type and Status render as plain headers rather than dead
// buttons, so nothing invites a click that would do nothing.
function SortableHeader({
  label,
  sortKey,
  sort,
  order,
  onSort,
}: {
  label: string
  sortKey: SortKey
  sort: SortKey
  order: SortOrder
  onSort: (key: SortKey) => void
}) {
  const activeSort = sort === sortKey
  return (
    <th
      style={styles.th}
      aria-sort={activeSort ? (order === 'asc' ? 'ascending' : 'descending') : 'none'}
    >
      <button type="button" style={styles.sortButton} onClick={() => onSort(sortKey)}>
        {label}
        <span style={styles.arrow} aria-hidden="true">
          {activeSort ? (order === 'asc' ? '▲' : '▼') : '↕'}
        </span>
      </button>
    </th>
  )
}

export default function DevicesTable({
  devices,
  now,
  emptyMessage = 'No devices registered yet.',
  sort,
  order,
  onSort,
  selectedId = null,
  onSelect,
}: {
  devices: Device[]
  now?: Date
  // An empty fleet and a filter that matches nothing are different problems, so
  // the caller says which one this is.
  emptyMessage?: string
  sort: SortKey
  order: SortOrder
  onSort: (key: SortKey) => void
  selectedId?: string | null
  onSelect?: (deviceId: string) => void
}) {
  if (devices.length === 0) {
    return <p style={styles.empty}>{emptyMessage}</p>
  }

  const headerProps = { sort, order, onSort }

  return (
    <table style={styles.table}>
      <thead>
        <tr>
          <SortableHeader label="Name" sortKey="name" {...headerProps} />
          <th style={styles.th}>Type</th>
          <SortableHeader label="Location" sortKey="location" {...headerProps} />
          <th style={styles.th}>Status</th>
          <SortableHeader label="Last seen" sortKey="last_seen" {...headerProps} />
          <th style={styles.th}>Sensors</th>
        </tr>
      </thead>
      <tbody>
        {devices.map((device) => (
          <tr
            key={device.device_id}
            onClick={() => onSelect?.(device.device_id)}
            // The row is the control, so it needs to be reachable and announce
            // its state without a mouse.
            tabIndex={onSelect ? 0 : undefined}
            role={onSelect ? 'button' : undefined}
            aria-pressed={onSelect ? device.device_id === selectedId : undefined}
            onKeyDown={(e) => {
              if (onSelect && (e.key === 'Enter' || e.key === ' ')) {
                e.preventDefault()
                onSelect(device.device_id)
              }
            }}
            style={{
              cursor: onSelect ? 'pointer' : undefined,
              background: device.device_id === selectedId ? '#eef5ff' : undefined,
            }}
          >
            {/* Devices have no separate name column; device_id is the name. */}
            <td style={styles.td}>{device.device_id}</td>
            <td style={styles.td}>{device.device_type}</td>
            <td style={styles.td}>
              {device.location_id ?? <span style={styles.unset}>unassigned</span>}
            </td>
            <td style={styles.td}>
              <StatusBadge status={device.status} />
            </td>
            <td style={styles.td} title={device.last_seen ?? 'never reported'}>
              {relativeTime(device.last_seen, now)}
            </td>
            <td style={styles.td} title={(device.sensor_types ?? []).join(', ')}>
              {device.sensor_count ?? 0}
            </td>
          </tr>
        ))}
      </tbody>
    </table>
  )
}
