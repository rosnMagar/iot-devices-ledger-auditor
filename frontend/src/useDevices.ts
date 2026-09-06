// Fetching, kept out of the rendering components.

import { useEffect, useState } from 'react'

import { fetchDevices, type DevicesResponse } from './api'
import { optionsFrom } from './filters'
import type { DeviceQuery } from './query'
import { toSearchParams } from './query'

export interface DevicesState {
  data: DevicesResponse | null
  loading: boolean
  error: string | null
}

function describe(error: unknown, apiBase: string): string {
  const message = error instanceof Error ? error.message : String(error)
  // The browser reports a blocked CORS request and a genuinely dead server with
  // the same opaque string; the real reason is only in the devtools console.
  if (message === 'Failed to fetch' || message === 'NetworkError when attempting to fetch resource.') {
    return `Could not reach the API at ${apiBase} — it may be down, or the browser may have blocked the request (CORS_ORIGINS).`
  }
  return message
}

// Refetches whenever the query changes. Previous rows stay on screen while the
// next page loads, so changing a filter does not blank the table.
export function useDevices(apiBase: string | undefined, query: DeviceQuery): DevicesState {
  const [state, setState] = useState<DevicesState>({
    data: null,
    loading: apiBase !== undefined,
    error: null,
  })

  // A string, not the object: a fresh object every render would refetch forever.
  const search = toSearchParams(query).toString()

  useEffect(() => {
    if (!apiBase) return

    // Abort on unmount and on a superseded query, so a slow early response
    // cannot overwrite a newer one.
    const controller = new AbortController()
    setState((previous) => ({ ...previous, loading: true, error: null }))

    fetchDevices(apiBase, new URLSearchParams(search), controller.signal)
      .then((data) => setState({ data, loading: false, error: null }))
      .catch((error: unknown) => {
        if (controller.signal.aborted) return
        setState({ data: null, loading: false, error: describe(error, apiBase) })
      })

    return () => controller.abort()
  }, [apiBase, search])

  return state
}

export interface FleetOptions {
  locationIds: string[]
  deviceTypes: string[]
  total: number
}

// The filtered response only contains matching devices, so the dropdowns cannot
// be built from it — one narrow filter would empty the others and strand the
// operator. This is a separate unfiltered read, which also gives the true fleet
// total for the "3 of 12" count.
export function useFleetOptions(apiBase: string | undefined): FleetOptions {
  const [options, setOptions] = useState<FleetOptions>({
    locationIds: [],
    deviceTypes: [],
    total: 0,
  })

  useEffect(() => {
    if (!apiBase) return
    const controller = new AbortController()

    fetchDevices(apiBase, undefined, controller.signal)
      .then((data) =>
        setOptions({ ...optionsFrom(data.devices), total: data.count }),
      )
      // Silent: the table and its error message are the primary view, and a
      // second error banner for the dropdowns would only add noise.
      .catch(() => undefined)

    return () => controller.abort()
  }, [apiBase])

  return options
}
