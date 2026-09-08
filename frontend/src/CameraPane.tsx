// Live view for a camera sensor (IOT-75).
//
// The stream is fetched straight from the device's own endpoint — it never
// passes through storage-core or the ledger. No frame is ever read from or
// written to a block (ADR 0011), so nothing here is recorded and there is
// nothing to retain, expire or erase.

import { useEffect, useState } from 'react'

import { relativeTime } from './relativeTime'
import type { CameraState } from './sensors'

const styles = {
  frame: {
    width: '100%',
    maxWidth: '420px',
    background: '#111',
    borderRadius: '4px',
    display: 'block',
  } as const,
  placeholder: {
    width: '100%',
    maxWidth: '420px',
    aspectRatio: '4 / 3',
    background: '#f4f4f4',
    border: '1px dashed #ccc',
    borderRadius: '4px',
    display: 'flex',
    alignItems: 'center',
    justifyContent: 'center',
    color: '#888',
    textAlign: 'center',
    padding: '1rem',
  } as const,
  note: { color: '#888', fontSize: '0.85em', marginTop: '0.4rem' } as const,
  live: { color: '#2b7a3d', fontSize: '0.85em', marginTop: '0.4rem' } as const,
} as const

// A multipart stream that dies mid-flight leaves the last frame on screen and
// fires no error — the image already loaded. Reconnecting periodically turns
// "frozen for ever, still captioned live" into "stale for at most this long".
const WATCHDOG_MS = 30_000

export default function CameraPane({
  camera,
  now,
  watchdogMs = WATCHDOG_MS,
}: {
  camera: CameraState | undefined
  now?: Date
  watchdogMs?: number
}) {
  const url = camera?.stream_url
  const [failed, setFailed] = useState(false)
  const [attempt, setAttempt] = useState(0)

  // A new URL deserves a fresh attempt; otherwise one failure hides a camera
  // that has since come back on a different address.
  useEffect(() => {
    setFailed(false)
    setAttempt(0)
  }, [url])

  useEffect(() => {
    if (url === undefined || failed) return
    const id = setInterval(() => setAttempt((n) => n + 1), watchdogMs)
    return () => clearInterval(id)
  }, [url, failed, watchdogMs])

  if (camera === undefined) {
    return (
      <div style={styles.placeholder}>
        No camera events yet — the device has not announced a stream.
      </div>
    )
  }

  const event = camera.event ?? 'unknown'
  const announcedOffline = event === 'stream_offline'
  const lastEvent = (
    <p style={styles.note}>
      Last event: {event.replace(/_/g, ' ')} · {relativeTime(camera.at, now)}
    </p>
  )

  if (announcedOffline) {
    // The device said so itself, which is more trustworthy than any probe.
    return (
      <>
        <div style={styles.placeholder}>
          The device reported its stream went offline.
        </div>
        {lastEvent}
      </>
    )
  }

  if (url === undefined) {
    // The ledger knows the camera exists but no stream_online has carried an
    // address. Distinct from the stream being down.
    return (
      <>
        <div style={styles.placeholder}>
          The camera is reporting, but has not announced a stream address.
        </div>
        {lastEvent}
      </>
    )
  }

  if (failed) {
    // Deliberately distinct from a ledger problem: the events above may be
    // arriving perfectly while the video source itself is unreachable.
    return (
      <>
        <div style={styles.placeholder}>
          Stream unreachable at <code>{url}</code>. The ledger feed is unaffected.
        </div>
        {lastEvent}
      </>
    )
  }

  return (
    <>
      {/* An <img> against multipart/x-mixed-replace is how an ESP32-CAM MJPEG
          endpoint is normally consumed; no player library is needed. */}
      <img
        // Remounted by the watchdog, so a stream that died is discovered rather
        // than displayed for ever as a still frame.
        key={attempt}
        src={attempt === 0 ? url : `${url}${url.includes('?') ? '&' : '?'}_=${attempt}`}
        alt="Live camera view"
        style={styles.frame}
        onError={() => setFailed(true)}
      />
      <p style={styles.live}>● streaming from {url} · not recorded</p>
      {lastEvent}
    </>
  )
}
