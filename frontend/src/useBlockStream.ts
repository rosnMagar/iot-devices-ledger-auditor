// The browser end of the live feed. Reconnects on its own, because a dashboard
// left open overnight will outlive at least one backend restart.

import { useEffect, useRef, useState } from 'react'

import { applyFrame, EMPTY_STREAM, streamUrlFrom, type BlockFrame, type StreamState } from './blockStream'

const INITIAL_BACKOFF_MS = 500
const MAX_BACKOFF_MS = 15_000

export function useBlockStream(apiBase: string | undefined): StreamState {
  const [state, setState] = useState<StreamState>(EMPTY_STREAM)
  const socket = useRef<WebSocket | null>(null)

  useEffect(() => {
    if (!apiBase || typeof WebSocket === 'undefined') return

    let closed = false
    let backoff = INITIAL_BACKOFF_MS
    let retry: ReturnType<typeof setTimeout> | undefined

    const open = () => {
      if (closed) return
      const ws = new WebSocket(streamUrlFrom(apiBase))
      socket.current = ws

      ws.onopen = () => {
        // Reset only on a successful open; resetting per attempt would turn a
        // connect/drop loop into a hot loop.
        backoff = INITIAL_BACKOFF_MS
        setState((previous) => ({ ...previous, connected: true }))
      }
      ws.onmessage = (event) => {
        try {
          setState((previous) => applyFrame(previous, JSON.parse(event.data) as BlockFrame))
        } catch {
          // A malformed frame must not kill the stream.
        }
      }
      ws.onclose = () => {
        setState((previous) => ({ ...previous, connected: false }))
        if (closed) return
        retry = setTimeout(open, backoff)
        backoff = Math.min(backoff * 2, MAX_BACKOFF_MS)
      }
      // onclose fires after onerror, so reconnection is handled in one place.
      ws.onerror = () => ws.close()
    }

    open()
    return () => {
      closed = true
      if (retry !== undefined) clearTimeout(retry)
      socket.current?.close()
      socket.current = null
    }
  }, [apiBase])

  return state
}

// Status decays with time, so something has to re-render even when no block
// arrives — otherwise a quiet device stays "active" forever.
export function useTicker(everyMs = 5000): Date {
  const [now, setNow] = useState(() => new Date())
  useEffect(() => {
    const id = setInterval(() => setNow(new Date()), everyMs)
    return () => clearInterval(id)
  }, [everyMs])
  return now
}
