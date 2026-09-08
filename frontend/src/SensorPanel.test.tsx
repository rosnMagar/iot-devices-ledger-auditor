// IOT-74 — the panel picks a renderer per sensor type, and distinguishes the
// states that a naive implementation would collapse into "empty".

import { render, screen, within } from '@testing-library/react'
import { describe, expect, it, vi } from 'vitest'

import SensorPanel from './SensorPanel'
import type { Reading, Sensor } from './sensors'

// uPlot draws to a canvas, which jsdom does not implement. The chart's data
// shaping is covered in chartData.test.ts; here we only need to know that a
// chart was rendered rather than a fallback.
vi.mock('./SeriesChart', () => ({
  default: ({ unit }: { unit: string | null }) => (
    <div data-testid="series-chart" data-unit={unit ?? ''} />
  ),
}))

const AT = '2026-09-07T12:00:00+00:00'
const NOW = new Date('2026-09-07T12:01:00Z')

const reading = (
  sensorId: string,
  value: Reading['value'],
  unit: string | null,
  type: string,
  seq = 1,
): Reading => ({ sensor_id: sensorId, sensor_type: type, value, unit, seq, at: AT })

const sensor = (over: Partial<Sensor> & { sensor_id: string }): Sensor => ({
  sensor_type: 'temperature',
  unit: 'celsius',
  registered: true,
  latest: null,
  ...over,
})

function panel(sensors: Sensor[], readings: Reading[] = []) {
  render(
    <SensorPanel
      deviceId="esp32-03"
      sensors={sensors}
      readings={readings}
      loading={false}
      error={null}
      now={NOW}
    />,
  )
}

const card = (sensorId: string) =>
  within(screen.getByText(sensorId).closest('section')!)

describe('renderer selection', () => {
  it('draws a gauge for a temperature sensor', () => {
    panel([
      sensor({ sensor_id: 'temp-0', latest: reading('temp-0', 21.4, 'celsius', 'temperature') }),
    ])
    expect(card('temp-0').getByRole('img', { name: /temp-0: 21.4 celsius/ })).toBeInTheDocument()
  })

  it('draws a gauge for a pressure sensor', () => {
    panel([
      sensor({
        sensor_id: 'pres-0', sensor_type: 'pressure', unit: 'hpa',
        latest: reading('pres-0', 1013.2, 'hpa', 'pressure'),
      }),
    ])
    expect(card('pres-0').getByRole('img', { name: /1013.2 hpa/ })).toBeInTheDocument()
  })

  it('draws a line chart for a non-gauge series', () => {
    panel(
      [sensor({
        sensor_id: 'hum-0', sensor_type: 'humidity', unit: 'percent',
        latest: reading('hum-0', 44, 'percent', 'humidity'),
      })],
      [reading('hum-0', 43, 'percent', 'humidity', 1), reading('hum-0', 44, 'percent', 'humidity', 2)],
    )
    expect(card('hum-0').getByTestId('series-chart')).toHaveAttribute('data-unit', 'percent')
  })

  it('charts an accelerometer vector', () => {
    panel(
      [sensor({
        sensor_id: 'acc-0', sensor_type: 'accelerometer', unit: 'm_s2',
        latest: reading('acc-0', [1, 2, 9.8], 'm_s2', 'accelerometer'),
      })],
      [reading('acc-0', [1, 2, 9.8], 'm_s2', 'accelerometer')],
    )
    expect(card('acc-0').getByTestId('series-chart')).toBeInTheDocument()
  })

  it('renders a live view for a camera, not a chart', () => {
    panel([
      sensor({
        sensor_id: 'cam-0', sensor_type: 'camera', unit: 'stream',
        latest: null,
        camera: {
          sensor_id: 'cam-0', sensor_type: 'camera', event: 'stream_online',
          stream_url: 'http://device.local:8090/stream', at: AT,
        },
      }),
    ])
    const body = card('cam-0')

    expect(body.getByRole('img', { name: /live camera/i })).toBeInTheDocument()
    expect(body.queryByTestId('series-chart')).not.toBeInTheDocument()
  })

  it('does not report a camera as having never sent anything', () => {
    // A camera reports events, not readings, so `latest` is legitimately null.
    // The generic "No readings yet" branch would be wrong here.
    panel([
      sensor({ sensor_id: 'cam-0', sensor_type: 'camera', unit: 'stream', latest: null }),
    ])
    const body = card('cam-0')

    expect(body.queryByText(/no readings yet/i)).not.toBeInTheDocument()
    expect(body.getByText(/has not announced a stream/i)).toBeInTheDocument()
  })

  it('falls back to the raw value for an unknown sensor type', () => {
    // Must never be an empty box — an unrecognised type still has a reading.
    panel([
      sensor({
        sensor_id: 'lux-0', sensor_type: 'illuminance', unit: 'lux',
        latest: reading('lux-0', 812, 'lux', 'illuminance'),
      }),
    ])
    const body = card('lux-0')
    expect(body.getByText('812')).toBeInTheDocument()
    expect(body.queryByTestId('series-chart')).not.toBeInTheDocument()
  })
})

describe('states a naive panel would collapse', () => {
  it('separates "never reported" from "read failed"', () => {
    panel([
      sensor({ sensor_id: 'quiet-0', latest: null }),
      sensor({
        sensor_id: 'broken-0',
        latest: reading('broken-0', null, 'celsius', 'temperature'),
      }),
    ])

    expect(card('quiet-0').getByText(/no readings yet/i)).toBeInTheDocument()
    // A failed read is a dash, never a zero that looks like a measurement.
    expect(card('broken-0').getByRole('img', { name: /read failed/ })).toBeInTheDocument()
  })

  it('flags a sensor that reports but was never registered', () => {
    // A typo in the device's config. Surfacing it is how it gets found.
    panel([
      sensor({
        sensor_id: 'tpm-0', registered: false,
        latest: reading('tpm-0', 21, 'celsius', 'temperature'),
      }),
    ])
    expect(card('tpm-0').getByText(/not registered/i)).toBeInTheDocument()
  })

  it('says a device has no sensors rather than rendering nothing', () => {
    panel([])
    expect(screen.getByText(/no sensors registered/i)).toBeInTheDocument()
  })
})

describe('layout', () => {
  it.each([1, 5])('renders %i sensor(s)', (count) => {
    panel(
      Array.from({ length: count }, (_, i) =>
        sensor({
          sensor_id: `temp-${i}`,
          latest: reading(`temp-${i}`, 20 + i, 'celsius', 'temperature'),
        }),
      ),
    )
    for (let i = 0; i < count; i += 1) {
      expect(screen.getByText(`temp-${i}`)).toBeInTheDocument()
    }
  })

  it('shows loading and error states', () => {
    render(
      <SensorPanel deviceId="esp32-01" sensors={[]} readings={[]} loading error={null} />,
    )
    expect(screen.getByText(/loading sensors/i)).toBeInTheDocument()

    render(
      <SensorPanel
        deviceId="esp32-01" sensors={[]} readings={[]} loading={false} error="boom"
      />,
    )
    expect(screen.getByText(/boom/)).toBeInTheDocument()
  })
})
