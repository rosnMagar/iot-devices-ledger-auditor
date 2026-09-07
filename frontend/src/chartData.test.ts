// IOT-74 — turning readings into chart data. The awkward cases are nulls,
// vectors and mixed sensors, all testable without a canvas.

import { describe, expect, it } from 'vitest'

import { hasAnyValue, isEmpty, toSeries } from './chartData'
import type { Reading } from './sensors'

const at = (seconds: number) =>
  new Date(Date.UTC(2026, 8, 7, 12, 0, seconds)).toISOString()

const reading = (sensorId: string, value: Reading['value'], s: number): Reading => ({
  sensor_id: sensorId,
  sensor_type: 'temperature',
  value,
  unit: 'celsius',
  seq: s,
  at: at(s),
})

describe('toSeries', () => {
  it('produces x values in seconds and one y line for a scalar sensor', () => {
    const series = toSeries(
      [reading('temp-0', 21, 1), reading('temp-0', 22, 2)],
      'temp-0',
    )

    expect(series.labels).toEqual(['temp-0'])
    expect(series.data[1]).toEqual([21, 22])
    expect(series.data[0][1] - series.data[0][0]).toBe(1)
  })

  it('keeps only the requested sensor', () => {
    const series = toSeries(
      [reading('temp-0', 21, 1), reading('hum-0', 44, 2), reading('temp-0', 23, 3)],
      'temp-0',
    )
    expect(series.data[1]).toEqual([21, 23])
  })

  it('sorts by time regardless of input order', () => {
    const series = toSeries(
      [reading('temp-0', 30, 3), reading('temp-0', 10, 1), reading('temp-0', 20, 2)],
      'temp-0',
    )
    expect(series.data[1]).toEqual([10, 20, 30])
  })

  it('carries a failed read through as null, not zero', () => {
    // ADR 0010: 0 is a real temperature. Plotting it would invent a measurement.
    const series = toSeries(
      [reading('temp-0', 21, 1), reading('temp-0', null, 2), reading('temp-0', 23, 3)],
      'temp-0',
    )
    expect(series.data[1]).toEqual([21, null, 23])
  })

  it('splits a vector into one line per axis', () => {
    const series = toSeries(
      [reading('acc-0', [1, 2, 9.8], 1), reading('acc-0', [1.1, 2.1, 9.9], 2)],
      'acc-0',
    )

    expect(series.labels).toEqual(['x', 'y', 'z'])
    expect(series.data[1]).toEqual([1, 1.1])
    expect(series.data[3]).toEqual([9.8, 9.9])
  })

  it('nulls every axis when a vector read fails', () => {
    // Otherwise the gap shows on x only and y/z look like they kept reporting.
    const series = toSeries(
      [reading('acc-0', [1, 2, 9.8], 1), reading('acc-0', null, 2)],
      'acc-0',
    )
    expect(series.data[1]).toEqual([1, null])
    expect(series.data[2]).toEqual([2, null])
    expect(series.data[3]).toEqual([9.8, null])
  })

  it('handles a sensor with no readings', () => {
    const series = toSeries([], 'temp-0')
    expect(isEmpty(series)).toBe(true)
  })
})

describe('emptiness', () => {
  it('separates "no points" from "every read failed"', () => {
    // Different problems: one sensor never reported, the other is broken.
    const none = toSeries([], 'temp-0')
    const allFailed = toSeries(
      [reading('temp-0', null, 1), reading('temp-0', null, 2)],
      'temp-0',
    )

    expect(isEmpty(none)).toBe(true)
    expect(isEmpty(allFailed)).toBe(false)
    expect(hasAnyValue(allFailed)).toBe(false)
    expect(hasAnyValue(toSeries([reading('temp-0', 21, 1)], 'temp-0'))).toBe(true)
  })
})
