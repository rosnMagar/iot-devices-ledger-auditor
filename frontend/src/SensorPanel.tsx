// The right-hand panel: one visualisation per sensor, chosen by sensor type.
// Adding a type means adding a renderer here, not editing the panel — which is
// how the camera pane (IOT-75) and the accelerometer 3D view (IOT-76) land.

import CameraPane from './CameraPane'
import Gauge from './Gauge'
import SeriesChart from './SeriesChart'
import { hasAnyValue, isEmpty, toSeries } from './chartData'
import { rangeFor } from './gaugeRange'
import { relativeTime } from './relativeTime'
import type { Reading, Sensor } from './sensors'

const styles = {
  panel: { borderLeft: '1px solid #ddd', paddingLeft: '2rem', minWidth: 0 } as const,
  card: { borderBottom: '1px solid #eee', padding: '1.25rem 0' } as const,
  head: { display: 'flex', justifyContent: 'space-between', alignItems: 'baseline' } as const,
  name: { fontWeight: 700, fontSize: '1.05rem' } as const,
  meta: { color: '#888', fontSize: '0.85em' } as const,
  note: { color: '#888', padding: '0.5rem 0' } as const,
  warn: { background: '#fff3cd', color: '#7a5c00', padding: '0.4rem', fontSize: '0.85em' } as const,
  raw: { fontSize: '2rem' } as const,
  row: { display: 'flex', gap: '1.5rem', alignItems: 'center' } as const,
  grow: { flex: '1 1 0', minWidth: 0 } as const,
} as const

const GAUGE_TYPES = new Set(['temperature', 'pressure'])

function latestScalar(latest: Reading | null): number | null {
  return typeof latest?.value === 'number' ? latest.value : null
}

function SensorBody({
  sensor,
  readings,
  now,
}: {
  sensor: Sensor
  readings: Reading[]
  now?: Date
}) {
  const { sensor_type: type, latest } = sensor

  // Checked before `latest`: a camera reports events, not readings, so it would
  // otherwise be reported as having never sent anything.
  if (type === 'camera') {
    return <CameraPane camera={sensor.camera} now={now} />
  }

  // Never reported at all — different from reporting and failing.
  if (latest === null) {
    return <p style={styles.note}>No readings yet.</p>
  }

  const unit = latest.unit ?? sensor.unit
  const range = rangeFor(type, unit)
  const series = toSeries(readings, sensor.sensor_id)

  if (GAUGE_TYPES.has(type ?? '') && range !== null) {
    return (
      <div style={styles.row}>
        <Gauge
          value={latestScalar(latest)}
          unit={unit}
          range={range}
          label={sensor.sensor_id}
        />
        {!isEmpty(series) && hasAnyValue(series) && (
          <div style={styles.grow}>
            <SeriesChart series={series} unit={unit} height={150} />
          </div>
        )}
      </div>
    )
  }

  if (!isEmpty(series) && hasAnyValue(series)) {
    return <SeriesChart series={series} unit={unit} />
  }

  // Reported, but nothing plottable — an unknown type, or every read failed.
  // Showing the raw latest value beats an empty box.
  return (
    <p style={styles.raw}>
      {latest.value === null ? '— read failed' : JSON.stringify(latest.value)}{' '}
      <span style={styles.meta}>{unit}</span>
    </p>
  )
}

export default function SensorPanel({
  deviceId,
  sensors,
  readings,
  loading,
  error,
  now,
}: {
  deviceId: string
  sensors: Sensor[]
  readings: Reading[]
  loading: boolean
  error: string | null
  now?: Date
}) {
  return (
    <aside style={styles.panel}>
      <h2 style={{ marginTop: 0 }}>{deviceId}</h2>

      {loading && <p style={styles.note}>Loading sensors…</p>}
      {error && <p style={{ color: 'red' }}>Error: {error}</p>}

      {!loading && !error && sensors.length === 0 && (
        <p style={styles.note}>This device has no sensors registered.</p>
      )}

      {sensors.map((sensor) => (
        <section key={sensor.sensor_id} style={styles.card}>
          <div style={styles.head}>
            <span style={styles.name}>{sensor.sensor_id}</span>
            <span style={styles.meta}>
              {sensor.sensor_type ?? 'unknown type'}
              {sensor.latest ? ` · ${relativeTime(sensor.latest.at, now)}` : ''}
            </span>
          </div>

          {!sensor.registered && (
            <p style={styles.warn}>
              Reporting but not registered — likely a typo in the device's config.
            </p>
          )}

          <SensorBody sensor={sensor} readings={readings} now={now} />
        </section>
      ))}
    </aside>
  )
}
