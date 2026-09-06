// Shapes returned by backend-api GET /devices. See docs/backend-api.md.

export interface Device {
  device_id: string
  location_id: string | null
  device_type: string
  registered_at: string
  // null = never reported. A timestamp = reported once, then possibly went quiet.
  last_seen: string | null
  status: 'active' | 'inactive'
}

export interface DevicesResponse {
  devices: Device[]
  count: number
  active_window_seconds: number
  // false = storage-core was unreachable, so status/last_seen may be stale.
  ledger_reachable: boolean
}

export async function fetchDevices(
  apiBase: string,
  signal?: AbortSignal,
): Promise<DevicesResponse> {
  const response = await fetch(`${apiBase}/devices`, { signal })
  if (!response.ok) {
    throw new Error(`GET /devices returned ${response.status} ${response.statusText}`)
  }
  return (await response.json()) as DevicesResponse
}
