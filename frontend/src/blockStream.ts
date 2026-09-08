// Turning the /ws/blocks frames into what the dashboard needs. Pure, so the
// merge rules are testable without a socket.

import type { Reading } from './sensors'

export interface BlockFrame {
  type: 'block' | 'lagged'
  block?: {
    index: number
    timestamp: string
    event: {
      event_type: string
      actor: string
      location_id: string
      metadata?: Record<string, unknown> | null
    }
  }
  device?: { device_type: string; location_id: string | null; registered: boolean } | null
  dropped?: number
}

export interface StreamState {
  connected: boolean
  // device_id -> the newest timestamp seen on the stream.
  lastSeen: Record<string, string>
  // Newest last, bounded — a long-lived tab must not grow without limit.
  readings: Reading[]
  dropped: number
}

export const EMPTY_STREAM: StreamState = {
  connected: false,
  lastSeen: {},
  readings: [],
  dropped: 0,
}

// Matches the server's per-sensor history cap, so the chart cannot end up
// holding more than /readings would ever return.
export const MAX_STREAM_READINGS = 500

function readingFrom(frame: BlockFrame): Reading | null {
  const event = frame.block?.event
  // Only measurements. A CAMERA_EVENT has no value (ADR 0011), and a block from
  // before ADR 0010 conforms to nothing.
  if (event?.event_type !== 'SENSOR_READING') return null
  const metadata = event.metadata
  if (metadata === null || typeof metadata !== 'object') return null
  const sensorId = metadata['sensor_id']
  if (typeof sensorId !== 'string' || sensorId === '') return null

  return {
    sensor_id: sensorId,
    sensor_type: (metadata['sensor_type'] as string | null) ?? null,
    // null is meaningful: the sensor reported and the read failed.
    value: (metadata['value'] as Reading['value']) ?? null,
    unit: (metadata['unit'] as string | null) ?? null,
    seq: (metadata['seq'] as number | null) ?? null,
    at: frame.block!.timestamp,
  }
}

export function applyFrame(state: StreamState, frame: BlockFrame): StreamState {
  if (frame.type === 'lagged') {
    // Surfaced, not swallowed: blocks were missed, so the charts have a hole.
    return { ...state, dropped: state.dropped + (frame.dropped ?? 0) }
  }

  const actor = frame.block?.event?.actor
  const at = frame.block?.timestamp
  if (typeof actor !== 'string' || !actor || typeof at !== 'string') return state

  // max, not assign: out-of-order delivery must not rewind last_seen, matching
  // the rule the backend already applies.
  const previous = state.lastSeen[actor]
  const lastSeen =
    previous === undefined || at > previous
      ? { ...state.lastSeen, [actor]: at }
      : state.lastSeen

  const reading = readingFrom(frame)
  const readings =
    reading === null
      ? state.readings
      : [...state.readings, { ...reading, device_id: actor } as Reading & { device_id: string }]
          .slice(-MAX_STREAM_READINGS)

  return { ...state, lastSeen, readings }
}

// Status decays with time, not with events: a device going quiet produces no
// block, so this must be recomputed on a timer rather than only on arrival.
export function statusFrom(
  lastSeen: string | null,
  windowSeconds: number,
  now: Date,
): 'active' | 'inactive' {
  if (lastSeen === null) return 'inactive'
  const parsed = Date.parse(lastSeen)
  if (Number.isNaN(parsed)) return 'inactive'
  return now.getTime() - parsed <= windowSeconds * 1000 ? 'active' : 'inactive'
}

// http(s) -> ws(s), same host. Derived rather than configured separately, so
// the two cannot drift the way CORS_ORIGINS once did.
export function streamUrlFrom(apiBase: string): string {
  return `${apiBase.replace(/^http/, 'ws')}/ws/blocks`
}
