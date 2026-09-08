// IOT-64 — merging live blocks into the dashboard. The merge rules are pure, so
// the awkward cases are testable without a socket.

import { describe, expect, it } from 'vitest'

import {
  applyFrame,
  EMPTY_STREAM,
  MAX_STREAM_READINGS,
  statusFrom,
  streamUrlFrom,
  type BlockFrame,
} from './blockStream'

const NOW = new Date('2026-09-07T12:00:00Z')

function frame(
  actor: string,
  at: string,
  metadata: Record<string, unknown> | null = {
    sensor_id: 'temp-0',
    sensor_type: 'temperature',
    value: 21,
    unit: 'celsius',
    seq: 1,
  },
  eventType = 'SENSOR_READING',
): BlockFrame {
  return {
    type: 'block',
    block: {
      index: 1,
      timestamp: at,
      event: { event_type: eventType, actor, location_id: 'warehouse-a', metadata },
    },
    device: { device_type: 'ESP32', location_id: 'warehouse-a', registered: true },
  }
}

describe('applyFrame', () => {
  it('records last_seen per device', () => {
    const state = applyFrame(EMPTY_STREAM, frame('esp32-01', '2026-09-07T12:00:00Z'))
    expect(state.lastSeen['esp32-01']).toBe('2026-09-07T12:00:00Z')
  })

  it('never rewinds last_seen on out-of-order delivery', () => {
    // Matches the rule the backend already applies.
    let state = applyFrame(EMPTY_STREAM, frame('esp32-01', '2026-09-07T12:00:05Z'))
    state = applyFrame(state, frame('esp32-01', '2026-09-07T12:00:01Z'))
    expect(state.lastSeen['esp32-01']).toBe('2026-09-07T12:00:05Z')
  })

  it('appends readings in arrival order', () => {
    let state = applyFrame(EMPTY_STREAM, frame('a', '2026-09-07T12:00:00Z'))
    state = applyFrame(state, frame('a', '2026-09-07T12:00:01Z'))
    expect(state.readings.map((r) => r.at)).toEqual([
      '2026-09-07T12:00:00Z',
      '2026-09-07T12:00:01Z',
    ])
  })

  it('keeps a failed read as null rather than dropping it', () => {
    const state = applyFrame(
      EMPTY_STREAM,
      frame('a', '2026-09-07T12:00:00Z', {
        sensor_id: 'temp-0', sensor_type: 'temperature', value: null, unit: 'celsius', seq: 3,
      }),
    )
    expect(state.readings[0].value).toBeNull()
  })

  it('keeps a vector reading intact', () => {
    const state = applyFrame(
      EMPTY_STREAM,
      frame('a', '2026-09-07T12:00:00Z', {
        sensor_id: 'acc-0', sensor_type: 'accelerometer', value: [1, 2, 9.8],
        unit: 'm_s2', seq: 1,
      }),
    )
    expect(state.readings[0].value).toEqual([1, 2, 9.8])
  })

  it('ignores camera events as readings but still updates last_seen', () => {
    // ADR 0011: a CAMERA_EVENT has no value and must not enter a chart.
    const state = applyFrame(
      EMPTY_STREAM,
      frame('cam-board', '2026-09-07T12:00:00Z',
        { sensor_id: 'cam-0', sensor_type: 'camera', event: 'motion_detected', seq: 1 },
        'CAMERA_EVENT'),
    )
    expect(state.readings).toHaveLength(0)
    expect(state.lastSeen['cam-board']).toBe('2026-09-07T12:00:00Z')
  })

  it.each([
    ['null metadata', null],
    ['metadata with no sensor_id', { sensor_type: 'temperature', value: 1 }],
    ['an empty sensor_id', { sensor_id: '', value: 1 }],
  ] as const)('skips a block with %s', (_label, metadata) => {
    const state = applyFrame(
      EMPTY_STREAM,
      frame('a', '2026-09-07T12:00:00Z', metadata as Record<string, unknown> | null),
    )
    expect(state.readings).toHaveLength(0)
    // Still counts as activity — the device reported something.
    expect(state.lastSeen['a']).toBe('2026-09-07T12:00:00Z')
  })

  it('skips a block with no metadata key at all', () => {
    // Passing undefined through the helper would hit its default, so the frame
    // is built directly.
    const bare: BlockFrame = {
      type: 'block',
      block: {
        index: 1,
        timestamp: '2026-09-07T12:00:00Z',
        event: { event_type: 'SENSOR_READING', actor: 'a', location_id: 'w' },
      },
    }
    const state = applyFrame(EMPTY_STREAM, bare)
    expect(state.readings).toHaveLength(0)
    expect(state.lastSeen['a']).toBe('2026-09-07T12:00:00Z')
  })

  it('bounds the reading buffer so a long-lived tab does not grow forever', () => {
    let state = EMPTY_STREAM
    for (let i = 0; i < MAX_STREAM_READINGS + 50; i += 1) {
      state = applyFrame(state, frame('a', `2026-09-07T12:00:00.${String(i).padStart(3, '0')}Z`))
    }
    expect(state.readings).toHaveLength(MAX_STREAM_READINGS)
  })

  it('surfaces a lag notice rather than swallowing it', () => {
    // Blocks were missed, so the charts have a hole the operator should know of.
    const state = applyFrame(EMPTY_STREAM, { type: 'lagged', dropped: 12 })
    expect(state.dropped).toBe(12)
    expect(state.readings).toHaveLength(0)
  })

  it('ignores a frame with no actor', () => {
    const broken = { type: 'block', block: { index: 1, timestamp: 'x', event: {} } }
    expect(applyFrame(EMPTY_STREAM, broken as unknown as BlockFrame)).toBe(EMPTY_STREAM)
  })
})

describe('statusFrom', () => {
  it('is active inside the window and inactive outside it', () => {
    expect(statusFrom('2026-09-07T11:59:00Z', 300, NOW)).toBe('active')
    expect(statusFrom('2026-09-07T11:50:00Z', 300, NOW)).toBe('inactive')
  })

  it('treats the window edge as active', () => {
    expect(statusFrom('2026-09-07T11:55:00Z', 300, NOW)).toBe('active')
  })

  it('honours a different window, so it is not hardcoded to 300', () => {
    expect(statusFrom('2026-09-07T11:59:00Z', 30, NOW)).toBe('inactive')
    expect(statusFrom('2026-09-07T11:59:00Z', 3600, NOW)).toBe('active')
  })

  it.each([null, 'not-a-timestamp'])('is inactive for %j', (value) => {
    expect(statusFrom(value, 300, NOW)).toBe('inactive')
  })
})

describe('streamUrlFrom', () => {
  it.each([
    ['http://localhost:8000', 'ws://localhost:8000/ws/blocks'],
    ['https://example.com', 'wss://example.com/ws/blocks'],
  ])('derives %s -> %s', (api, expected) => {
    // Derived rather than configured separately, so the two cannot drift.
    expect(streamUrlFrom(api)).toBe(expected)
  })
})
