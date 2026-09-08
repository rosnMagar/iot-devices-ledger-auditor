import { useState } from 'react'

import DevicesTable from './DevicesTable'
import SensorPanel from './SensorPanel'
import FilterControls from './FilterControls'
import { isFiltered } from './filters'
import type { DeviceFilters } from './filters'
import type { SortKey } from './query'
import { toggleSort } from './query'
import { statusFrom } from './blockStream'
import { useBlockStream, useTicker } from './useBlockStream'
import { useDevices, useFleetOptions } from './useDevices'
import { useSensorPanel } from './useSensorPanel'
import { useUrlQuery } from './useUrlQuery'

// No localhost fallback. Vite inlines this at build time, so if it's missing the
// bundle is already wrong and every user's browser would silently call their own
// machine. Better to say so on the page than to look broken for no visible reason.
const API = (import.meta.env.VITE_API_BASE_URL as string | undefined)?.trim()

const page = { fontFamily: 'monospace', padding: '2rem' } as const
// Table left, panel right. The panel takes half the screen so charts are big
// enough to read; minmax(0, ...) lets a wide table scroll instead of forcing
// the grid wider than the viewport.
const columns = {
  display: 'grid',
  gridTemplateColumns: 'minmax(0, 1fr) minmax(0, 1fr)',
  gap: '2rem',
  alignItems: 'start',
} as const
// One column when nothing is selected, and on a narrow screen the media query
// below collapses to one regardless.
const singleColumn = { display: 'block' } as const
const tableColumn = { minWidth: 0, overflowX: 'auto' } as const

export default function App() {
  const [query, setQuery] = useUrlQuery()
  const [selectedId, setSelectedId] = useState<string | null>(null)
  const { data, loading, error } = useDevices(API, query)
  const options = useFleetOptions(API)
  const panel = useSensorPanel(API, selectedId)
  const stream = useBlockStream(API)
  // Status decays with time, so something must re-render even when no block
  // arrives — otherwise a quiet device stays "active" for ever.
  const now = useTicker(5000)

  // Selects and header clicks are discrete events, so there is nothing to
  // debounce — one change is one request.
  const patchFilters = (patch: Partial<DeviceFilters>) => setQuery({ ...query, ...patch })
  const sortBy = (key: SortKey) => setQuery(toggleSort(query, key))

  // Live values layered over the fetched rows. Positions are deliberately not
  // re-sorted: the sort is server-side (IOT-39), and rows jumping under the
  // cursor on every reading would be worse than a stale ordering. The next
  // fetch reorders.
  const devices = (data?.devices ?? []).map((device) => {
    const streamed = stream.lastSeen[device.device_id]
    const lastSeen =
      streamed !== undefined && (device.last_seen === null || streamed > device.last_seen)
        ? streamed
        : device.last_seen
    return {
      ...device,
      last_seen: lastSeen,
      status: data ? statusFrom(lastSeen, data.active_window_seconds, now) : device.status,
    }
  })

  // Readings that arrived over the socket for the selected device, appended to
  // the backfill from /readings. De-duplicated on (sensor, seq, timestamp),
  // because a reading can arrive on the socket that the backfill also returned.
  const streamedForDevice = stream.readings.filter(
    (r) => (r as { device_id?: string }).device_id === selectedId,
  )
  const seen = new Set(panel.readings.map((r) => `${r.sensor_id}|${r.seq}|${r.at}`))
  const panelReadings = [
    ...panel.readings,
    ...streamedForDevice.filter((r) => !seen.has(`${r.sensor_id}|${r.seq}|${r.at}`)),
  ]

  // The gauge and the "Xs ago" label read `latest`, which came from the fetch.
  // Without this the chart grows while the dial beside it stays frozen at the
  // value it had when the device was selected.
  const panelSensors = panel.sensors.map((sensor) => {
    const newest = panelReadings.reduce<typeof sensor.latest>((best, reading) => {
      if (reading.sensor_id !== sensor.sensor_id) return best
      return best === null || reading.at > best.at ? reading : best
    }, sensor.latest)
    return newest === sensor.latest ? sensor : { ...sensor, latest: newest }
  })

  if (!API) {
    return (
      <p style={{ ...page, color: 'red' }}>
        This build has no <code>VITE_API_BASE_URL</code>, so it doesn't know where
        the API is. It is baked in at image build time — pass it as a build arg
        and rebuild; restarting the container will not help.
      </p>
    )
  }

  return (
    <main style={page}>
      <h1>IoT Devices Ledger Auditor</h1>

      {error && <p style={{ color: 'red' }}>Error: {error}</p>}

      {/* The socket being down is not an outage — everything still works from
          the fetch — but a dashboard that has silently stopped updating while
          looking live is worse than one that says so. */}
      <p style={{ color: stream.connected ? '#2b7a3d' : '#888', fontSize: '0.85em' }}>
        {stream.connected ? '● live' : '○ not live — showing the last fetch'}
        {stream.dropped > 0 && ` · ${stream.dropped} blocks missed`}
      </p>

      {/* Status is derived from the ledger; if it is unreachable the column is
          stale, and saying nothing would show a dead fleet as healthy. */}
      {data && !data.ledger_reachable && (
        <p style={{ background: '#fff3cd', color: '#7a5c00', padding: '0.5rem' }}>
          The ledger is unreachable — status and last seen may be out of date.
        </p>
      )}

      <FilterControls
        filters={query}
        locationIds={options.locationIds}
        deviceTypes={options.deviceTypes}
        onChange={patchFilters}
      />

      <p style={{ color: '#888' }}>
        {/* Both numbers while filtering, so a narrow filter never reads as a
            fleet that has shrunk. The total comes from the unfiltered fetch. */}
        {data === null
          ? 'Loading devices…'
          : isFiltered(query)
            ? `${data.count} of ${options.total} devices`
            : `${data.count} device${data.count === 1 ? '' : 's'}`}
        {' · '}
        {/* Rows stay on screen while refetching, so this is the only signal
            that a filter change is still in flight. */}
        {loading && data !== null ? 'updating…' : null}
        {data !== null && !loading
          ? `active means seen in the last ${data.active_window_seconds}s`
          : null}
      </p>

      <div style={selectedId === null ? singleColumn : columns} className="panel-grid">
        <div style={tableColumn}>
          {data && (
            <DevicesTable
              devices={devices}
              sort={query.sort}
              order={query.order}
              onSort={sortBy}
              selectedId={selectedId}
              onSelect={setSelectedId}
              emptyMessage={
                isFiltered(query)
                  ? 'No devices match these filters.'
                  : 'No devices registered yet.'
              }
            />
          )}
        </div>

        {selectedId !== null && (
          <SensorPanel
            now={now}
            deviceId={selectedId}
            sensors={panelSensors}
            readings={panelReadings}
            loading={panel.loading}
            error={panel.error}
          />
        )}
      </div>

      {data && data.devices.length > 0 && selectedId === null && (
        <p style={{ color: '#888', marginTop: '1rem' }}>
          Select a device to see its sensors.
        </p>
      )}
    </main>
  )
}
