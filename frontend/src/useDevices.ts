// Fetching, kept out of the rendering components.

import { useEffect, useState } from 'react'

import { fetchDevices, type DevicesResponse } from './api'

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

export function useDevices(apiBase: string | undefined): DevicesState {
  const [state, setState] = useState<DevicesState>({
    data: null,
    loading: apiBase !== undefined,
    error: null,
  })

  useEffect(() => {
    if (!apiBase) return

    // Abort on unmount so StrictMode's double-invoke can't apply a stale response.
    const controller = new AbortController()
    setState((previous) => ({ ...previous, loading: true }))

    fetchDevices(apiBase, controller.signal)
      .then((data) => setState({ data, loading: false, error: null }))
      .catch((error: unknown) => {
        if (controller.signal.aborted) return
        setState({ data: null, loading: false, error: describe(error, apiBase) })
      })

    return () => controller.abort()
  }, [apiBase])

  return state
}
