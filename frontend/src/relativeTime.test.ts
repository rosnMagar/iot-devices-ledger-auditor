// IOT-40 — "2m ago" formatting, including the timezone trap.

import { describe, expect, it } from 'vitest'

import { relativeTime } from './relativeTime'

const NOW = new Date('2026-09-06T12:00:00Z')

describe('relativeTime', () => {
  it('separates never-reported from a real timestamp', () => {
    expect(relativeTime(null, NOW)).toBe('never')
  })

  it('does not throw on a malformed timestamp', () => {
    expect(relativeTime('not-a-timestamp', NOW)).toBe('unknown')
  })

  it.each([
    ['2026-09-06T11:59:30+00:00', '30s ago'],
    ['2026-09-06T11:59:00+00:00', '1m ago'],
    ['2026-09-06T11:58:00+00:00', '2m ago'],
    ['2026-09-06T09:00:00+00:00', '3h ago'],
    ['2026-09-04T12:00:00+00:00', '2d ago'],
  ])('formats %s as %s', (iso, expected) => {
    expect(relativeTime(iso, NOW)).toBe(expected)
  })

  it('clamps a future timestamp instead of saying "-5m ago"', () => {
    // The box clock running ahead of the browser's, not a reading from the future.
    expect(relativeTime('2026-09-06T12:05:00+00:00', NOW)).toBe('just now')
  })

  it.each([
    ['Z suffix', '2026-09-06T11:58:00Z'],
    ['+00:00 offset', '2026-09-06T11:58:00+00:00'],
    ['non-UTC offset', '2026-09-06T13:58:00+02:00'],
    // The regression guard: JS parses an offset-less ISO string as *local* time,
    // which in a UTC-5 browser renders a 2-minute-old reading as "just now".
    ['no offset at all', '2026-09-06T11:58:00'],
  ])('reads %s as UTC', (_label, iso) => {
    expect(relativeTime(iso, NOW)).toBe('2m ago')
  })
})
