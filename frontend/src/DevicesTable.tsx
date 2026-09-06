// Presentational only: takes devices, renders them. No fetching, so it can be
// reused wherever a device list already exists.

import type { Device } from './api'
import { relativeTime } from './relativeTime'

const styles = {
  table: { borderCollapse: 'collapse', width: '100%' } as const,
  th: {
    textAlign: 'left',
    padding: '0.5rem',
    borderBottom: '2px solid #ddd',
    color: '#555',
  } as const,
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

export default function DevicesTable({
  devices,
  now,
  emptyMessage = 'No devices registered yet.',
}: {
  devices: Device[]
  now?: Date
  // An empty fleet and a filter that matches nothing are different problems, so
  // the caller says which one this is.
  emptyMessage?: string
}) {
  if (devices.length === 0) {
    return <p style={styles.empty}>{emptyMessage}</p>
  }

  return (
    <table style={styles.table}>
      <thead>
        <tr>
          <th style={styles.th}>Name</th>
          <th style={styles.th}>Type</th>
          <th style={styles.th}>Location</th>
          <th style={styles.th}>Status</th>
          <th style={styles.th}>Last seen</th>
        </tr>
      </thead>
      <tbody>
        {devices.map((device) => (
          <tr key={device.device_id}>
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
          </tr>
        ))}
      </tbody>
    </table>
  )
}
