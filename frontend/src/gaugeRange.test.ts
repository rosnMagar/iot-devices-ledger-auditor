// IOT-74 — a gauge needs a range; a reading only carries a value and a unit.

import { describe, expect, it } from 'vitest'

import { fraction, rangeFor } from './gaugeRange'

describe('rangeFor', () => {
  it('prefers the unit over the sensor type', () => {
    // The unit comes from the reading and is authoritative, so a probe swapped
    // to a different unit is not silently drawn on the old scale.
    expect(rangeFor('temperature', 'percent')).toEqual({ min: 0, max: 100 })
  })

  it('falls back to the sensor type when the unit is unknown', () => {
    expect(rangeFor('pressure', 'made-up')).toEqual({ min: 950, max: 1050 })
  })

  it('returns null when neither is known, so the caller can degrade', () => {
    expect(rangeFor('mystery', 'furlongs')).toBeNull()
    expect(rangeFor(null, null)).toBeNull()
  })
})

describe('fraction', () => {
  const range = { min: 0, max: 100 }

  it.each([
    [0, 0],
    [50, 0.5],
    [100, 1],
  ])('maps %d to %d', (value, expected) => {
    expect(fraction(value, range)).toBe(expected)
  })

  it('clamps rather than dropping an out-of-range reading', () => {
    // A value past the end is still a real measurement; it pins at the end.
    expect(fraction(-40, range)).toBe(0)
    expect(fraction(1000, range)).toBe(1)
  })

  it('does not divide by zero on a degenerate range', () => {
    expect(fraction(5, { min: 10, max: 10 })).toBe(0)
  })
})
