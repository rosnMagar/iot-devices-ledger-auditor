// Keeps the filter/sort state and the address bar in step, so a filtered view
// can be shared as a link and the back button actually goes back.

import { useCallback, useEffect, useState } from 'react'

import type { DeviceQuery } from './query'
import { fromSearchParams, toSearchParams } from './query'

export function useUrlQuery(): [DeviceQuery, (next: DeviceQuery) => void] {
  // Read on first render, so a link with filters already in it loads filtered.
  const [query, setQuery] = useState(() => fromSearchParams(window.location.search))

  useEffect(() => {
    // Without this the URL changes on back/forward but the table does not.
    const onPopState = () => setQuery(fromSearchParams(window.location.search))
    window.addEventListener('popstate', onPopState)
    return () => window.removeEventListener('popstate', onPopState)
  }, [])

  const update = useCallback((next: DeviceQuery) => {
    const search = toSearchParams(next).toString()
    // Drop the "?" entirely at defaults, rather than leaving a bare one behind.
    window.history.pushState(null, '', search ? `?${search}` : window.location.pathname)
    setQuery(next)
  }, [])

  return [query, update]
}
