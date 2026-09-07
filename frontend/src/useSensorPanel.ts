// Loads what the panel needs for one device: its sensors (with latest values)
// and enough history to draw. Two requests, not one per sensor.

import { useEffect, useState } from 'react'

import { fetchReadings, fetchSensors, type Reading, type Sensor } from './sensors'

export interface PanelState {
  sensors: Sensor[]
  readings: Reading[]
  loading: boolean
  error: string | null
}

const EMPTY: PanelState = { sensors: [], readings: [], loading: false, error: null }

export function useSensorPanel(
  apiBase: string | undefined,
  deviceId: string | null,
): PanelState {
  const [state, setState] = useState<PanelState>(EMPTY)

  useEffect(() => {
    if (!apiBase || deviceId === null) {
      setState(EMPTY)
      return
    }

    // Abort on a new selection, so a slow response for the previous device
    // cannot land in the panel of the current one.
    const controller = new AbortController()
    setState({ ...EMPTY, loading: true })

    Promise.all([
      fetchSensors(apiBase, deviceId, controller.signal),
      fetchReadings(apiBase, deviceId, controller.signal),
    ])
      .then(([sensors, readings]) =>
        setState({
          sensors: sensors.sensors,
          readings: readings.series,
          loading: false,
          error: null,
        }),
      )
      .catch((error: unknown) => {
        if (controller.signal.aborted) return
        setState({
          ...EMPTY,
          error: error instanceof Error ? error.message : String(error),
        })
      })

    return () => controller.abort()
  }, [apiBase, deviceId])

  return state
}
