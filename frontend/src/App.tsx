import DevicesTable from './DevicesTable'
import FilterControls from './FilterControls'
import { isFiltered } from './filters'
import type { DeviceFilters } from './filters'
import type { SortKey } from './query'
import { toggleSort } from './query'
import { useDevices, useFleetOptions } from './useDevices'
import { useUrlQuery } from './useUrlQuery'

// No localhost fallback. Vite inlines this at build time, so if it's missing the
// bundle is already wrong and every user's browser would silently call their own
// machine. Better to say so on the page than to look broken for no visible reason.
const API = (import.meta.env.VITE_API_BASE_URL as string | undefined)?.trim()

const page = { fontFamily: 'monospace', padding: '2rem', maxWidth: '900px' } as const

export default function App() {
  const [query, setQuery] = useUrlQuery()
  const { data, loading, error } = useDevices(API, query)
  const options = useFleetOptions(API)

  // Selects and header clicks are discrete events, so there is nothing to
  // debounce — one change is one request.
  const patchFilters = (patch: Partial<DeviceFilters>) => setQuery({ ...query, ...patch })
  const sortBy = (key: SortKey) => setQuery(toggleSort(query, key))

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

      {error && <p style={{ color: 'red' }}>Error: {error}</p>}

      {/* Status is derived from the ledger; if it is unreachable the column is
          stale, and saying nothing would show a dead fleet as healthy. */}
      {data && !data.ledger_reachable && (
        <p style={{ background: '#fff3cd', color: '#7a5c00', padding: '0.5rem' }}>
          The ledger is unreachable — status and last seen may be out of date.
        </p>
      )}

      <FilterControls
        filters={query}
        locationIds={options.locationIds}
        deviceTypes={options.deviceTypes}
        onChange={patchFilters}
      />

      <p style={{ color: '#888' }}>
        {/* Both numbers while filtering, so a narrow filter never reads as a
            fleet that has shrunk. The total comes from the unfiltered fetch. */}
        {data === null
          ? 'Loading devices…'
          : isFiltered(query)
            ? `${data.count} of ${options.total} devices`
            : `${data.count} device${data.count === 1 ? '' : 's'}`}
        {' · '}
        {/* Rows stay on screen while refetching, so this is the only signal
            that a filter change is still in flight. */}
        {loading && data !== null ? 'updating…' : null}
        {data !== null && !loading
          ? `active means seen in the last ${data.active_window_seconds}s`
          : null}
      </p>

      {data && (
        <DevicesTable
          devices={data.devices}
          sort={query.sort}
          order={query.order}
          onSort={sortBy}
          emptyMessage={
            isFiltered(query)
              ? 'No devices match these filters.'
              : 'No devices registered yet.'
          }
        />
      )}
    </main>
  )
}
