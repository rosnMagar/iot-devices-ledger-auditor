// Shapes returned by GET /devices/{id}/sensors and GET /readings (ADR 0010).

export type SensorValue = number | number[] | null

export interface Reading {
  sensor_id: string
  sensor_type: string | null
  // null = the sensor reported and the read failed. Distinct from no reading.
  value: SensorValue
  unit: string | null
  seq: number | null
  at: string
}

export interface CameraState {
  sensor_id: string
  sensor_type: 'camera'
  event: string | null
  // Announced by the device when its stream came online. The ledger records the
  // address, never the media (ADR 0011).
  stream_url?: string
  at: string
}

export interface Sensor {
  sensor_id: string
  sensor_type: string | null
  unit: string | null
  // false = the ledger reports it but nothing registered it — a firmware typo.
  registered: boolean
  latest: Reading | null
  camera?: CameraState
}

export interface SensorsResponse {
  device_id: string
  sensors: Sensor[]
  count: number
  unregistered_count: number
  ledger_reachable: boolean
}

export interface ReadingsResponse {
  device_id: string
  sensor_id: string | null
  series: Reading[]
  count: number
  limit: number
  history_limit: number
  truncated: boolean
  ledger_reachable: boolean
}

async function getJson<T>(url: string, signal?: AbortSignal): Promise<T> {
  const response = await fetch(url, { signal })
  if (!response.ok) {
    throw new Error(`${url} returned ${response.status} ${response.statusText}`)
  }
  return (await response.json()) as T
}

export function fetchSensors(
  apiBase: string,
  deviceId: string,
  signal?: AbortSignal,
): Promise<SensorsResponse> {
  return getJson(`${apiBase}/devices/${encodeURIComponent(deviceId)}/sensors`, signal)
}

export function fetchReadings(
  apiBase: string,
  deviceId: string,
  signal?: AbortSignal,
): Promise<ReadingsResponse> {
  // encodeURIComponent on every value: an unencoded "+" in a timestamp decodes
  // to a space server-side, which is a confusing 400 for a correct value.
  const params = new URLSearchParams({ device_id: deviceId })
  return getJson(`${apiBase}/readings?${params.toString()}`, signal)
}
