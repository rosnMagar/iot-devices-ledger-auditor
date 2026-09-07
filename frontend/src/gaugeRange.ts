// A radial gauge needs a range; a reading only carries a value and a unit.
// These are display defaults, deliberately generous — a needle pinned at the
// end still shows a real reading, but a range that clips would hide one.

export interface Range {
  min: number
  max: number
}

const BY_UNIT: Record<string, Range> = {
  celsius: { min: -20, max: 50 },
  percent: { min: 0, max: 100 },
  hpa: { min: 950, max: 1050 },
}

const BY_TYPE: Record<string, Range> = {
  temperature: { min: -20, max: 50 },
  humidity: { min: 0, max: 100 },
  pressure: { min: 950, max: 1050 },
}

export function rangeFor(sensorType: string | null, unit: string | null): Range | null {
  // Unit first: it is the authoritative one from the reading, so a probe
  // swapped from °C to °F is not silently drawn on a celsius scale.
  if (unit && BY_UNIT[unit]) return BY_UNIT[unit]
  if (sensorType && BY_TYPE[sensorType]) return BY_TYPE[sensorType]
  return null
}

// Position on the dial, clamped. A value outside the range is still real, so it
// pins at the end rather than disappearing.
export function fraction(value: number, range: Range): number {
  if (range.max <= range.min) return 0
  return Math.min(1, Math.max(0, (value - range.min) / (range.max - range.min)))
}
