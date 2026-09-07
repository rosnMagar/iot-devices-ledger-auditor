// Radial gauge for a scalar reading. Plain SVG — a single needle does not need
// a charting library, and uPlot is for series.

import { fraction, type Range } from './gaugeRange'

const SIZE = 168
const CENTRE = SIZE / 2
const RADIUS = 64
// A 240° sweep starting bottom-left, so the dial reads left-to-right.
const START = 150
const SWEEP = 240

function polar(angleDeg: number, radius: number): [number, number] {
  const rad = (angleDeg * Math.PI) / 180
  return [CENTRE + radius * Math.cos(rad), CENTRE + radius * Math.sin(rad)]
}

function arc(fromFrac: number, toFrac: number, radius: number): string {
  const [x1, y1] = polar(START + SWEEP * fromFrac, radius)
  const [x2, y2] = polar(START + SWEEP * toFrac, radius)
  const large = SWEEP * (toFrac - fromFrac) > 180 ? 1 : 0
  return `M ${x1} ${y1} A ${radius} ${radius} 0 ${large} 1 ${x2} ${y2}`
}

export default function Gauge({
  value,
  unit,
  range,
  label,
}: {
  value: number | null
  unit: string | null
  range: Range
  label: string
}) {
  const filled = value === null ? 0 : fraction(value, range)

  return (
    <svg
      width={SIZE}
      height={SIZE * 0.78}
      viewBox={`0 0 ${SIZE} ${SIZE * 0.78}`}
      role="img"
      aria-label={
        value === null
          ? `${label}: read failed`
          : `${label}: ${value}${unit ? ` ${unit}` : ''}`
      }
    >
      <path d={arc(0, 1, RADIUS)} fill="none" stroke="#eee" strokeWidth="13" />
      {value !== null && (
        <path
          d={arc(0, filled, RADIUS)}
          fill="none"
          stroke="#2b7a3d"
          strokeWidth="13"
          strokeLinecap="round"
        />
      )}
      <text x={CENTRE} y={CENTRE + 10} textAnchor="middle" fontSize="26" fill="#222">
        {/* A failed read is not zero — it must never look like a measurement. */}
        {value === null ? '—' : value}
      </text>
      <text x={CENTRE} y={CENTRE + 32} textAnchor="middle" fontSize="12" fill="#888">
        {value === null ? 'read failed' : (unit ?? '')}
      </text>
    </svg>
  )
}
