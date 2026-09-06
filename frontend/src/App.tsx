import { useMemo, useState } from 'react'

import DevicesTable from './DevicesTable'
import FilterControls from './FilterControls'
import { applyFilters, isFiltered, NO_FILTERS, optionsFrom } from './filters'
import { useDevices } from './useDevices'

// No localhost fallback. Vite inlines this at build time, so if it's missing the
// bundle is already wrong and every user's browser would silently call their own
// machine. Better to say so on the page than to look broken for no visible reason.
const API = (import.meta.env.VITE_API_BASE_URL as string | undefined)?.trim()

const page = { fontFamily: 'monospace', padding: '2rem', maxWidth: '900px' } as const

export default function App() {
  const { data, loading, error } = useDevices(API)
  const [filters, setFilters] = useState(NO_FILTERS)

  const all = data?.devices
  // Options track the fetched fleet, not the filtered view — see filters.ts.
  const options = useMemo(() => optionsFrom(all ?? []), [all])
  const shown = useMemo(() => applyFilters(all ?? [], filters), [all, filters])

  if (!API) {
    return (
      <p style={{ ...page, color: 'red' }}>
        This build has no <code>VITE_API_BASE_URL</code>, so it doesn't know where
        the API is. It is baked in at image build time — pass it as a build arg
        and rebuild; restarting the container will not help.
      </p>
    )
  }

  return (
    <main style={page}>
      <h1>IoT Devices Ledger Auditor</h1>

      {loading && <p style={{ color: '#888' }}>Loading devices…</p>}
      {error && <p style={{ color: 'red' }}>Error: {error}</p>}

      {data && (
        <>
          {/* Status is derived from the ledger; if it is unreachable the column
              is stale, and saying nothing would show a dead fleet as healthy. */}
          {!data.ledger_reachable && (
            <p style={{ background: '#fff3cd', color: '#7a5c00', padding: '0.5rem' }}>
              The ledger is unreachable — status and last seen may be out of date.
            </p>
          )}

          <FilterControls
            filters={filters}
            locationIds={options.locationIds}
            deviceTypes={options.deviceTypes}
            onChange={setFilters}
          />

          {/* Both numbers while filtering, so a narrow filter never reads as a
              fleet that has shrunk. */}
          <p style={{ color: '#888' }}>
            {isFiltered(filters)
              ? `${shown.length} of ${data.count} devices`
              : `${data.count} device${data.count === 1 ? '' : 's'}`}{' '}
            · active means seen in the last {data.active_window_seconds}s
          </p>

          <DevicesTable
            devices={shown}
            emptyMessage={
              isFiltered(filters)
                ? 'No devices match these filters.'
                : 'No devices registered yet.'
            }
          />
        </>
      )}
    </main>
  )
}
