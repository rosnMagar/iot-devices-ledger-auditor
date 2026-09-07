// Turning a reading series into what a chart draws. Pure, so the awkward cases
// — nulls, vectors, mixed sensors — are testable without a canvas.

import type { Reading } from './sensors'

// uPlot's format: [xs, ...ys], x in seconds, y null for a gap.
export type UplotData = [number[], ...(number | null)[][]]

export interface Series {
  labels: string[]
  data: UplotData
}

function seconds(iso: string): number {
  return Date.parse(iso) / 1000
}

// A failed read is null, never 0 (ADR 0010): a chart must break the line, not
// plot a value the sensor never produced.
export function toSeries(readings: Reading[], sensorId: string): Series {
  const points = readings
    .filter((r) => r.sensor_id === sensorId)
    .slice()
    .sort((a, b) => seconds(a.at) - seconds(b.at))

  const xs = points.map((p) => seconds(p.at))
  const vectorWidth = points.reduce(
    (width, p) => (Array.isArray(p.value) ? Math.max(width, p.value.length) : width),
    0,
  )

  if (vectorWidth === 0) {
    const ys = points.map((p) => (typeof p.value === 'number' ? p.value : null))
    return { labels: [sensorId], data: [xs, ys] }
  }

  // A vector becomes one line per axis. A null vector nulls every axis, so the
  // gap is visible on all of them rather than only the first.
  const axes: (number | null)[][] = []
  for (let axis = 0; axis < vectorWidth; axis += 1) {
    axes.push(
      points.map((p) => (Array.isArray(p.value) ? (p.value[axis] ?? null) : null)),
    )
  }
  const names = ['x', 'y', 'z']
  return {
    labels: axes.map((_, i) => names[i] ?? `axis ${i}`),
    data: [xs, ...axes],
  }
}

export function isEmpty(series: Series): boolean {
  // All-null is not the same as no points: the sensor reported and every read
  // failed, which is worth showing rather than calling the chart empty.
  return series.data[0].length === 0
}

export function hasAnyValue(series: Series): boolean {
  return series.data.slice(1).some((axis) => axis.some((v) => v !== null))
}
