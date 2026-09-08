// IOT-75 — the camera live view. The states that matter are the ones a naive
// implementation would merge into one grey box.

import { render, screen } from '@testing-library/react'
import { fireEvent } from '@testing-library/dom'
import { act } from 'react'
import { describe, expect, it, vi } from 'vitest'

import CameraPane from './CameraPane'
import type { CameraState } from './sensors'

const NOW = new Date('2026-09-07T12:01:00Z')

const camera = (over: Partial<CameraState> = {}): CameraState => ({
  sensor_id: 'cam-0',
  sensor_type: 'camera',
  event: 'stream_online',
  stream_url: 'http://device.local:8090/stream',
  at: '2026-09-07T12:00:00+00:00',
  ...over,
})

describe('live view', () => {
  it('renders the stream straight from the device', () => {
    render(<CameraPane camera={camera()} now={NOW} />)
    const image = screen.getByRole('img', { name: /live camera/i })

    // Straight at the device: the stream never passes through the ledger.
    expect(image).toHaveAttribute('src', 'http://device.local:8090/stream')
  })

  it('says the stream is not recorded', () => {
    // ADR 0011: live only. Nothing is stored, so there is nothing to retain.
    render(<CameraPane camera={camera()} now={NOW} />)
    expect(screen.getByText(/not recorded/i)).toBeInTheDocument()
  })

  it('shows the latest camera event alongside the video', () => {
    render(<CameraPane camera={camera({ event: 'motion_detected' })} now={NOW} />)
    expect(screen.getByText(/motion detected/i)).toBeInTheDocument()
    expect(screen.getByText(/1m ago/)).toBeInTheDocument()
  })
})

describe('states a naive pane would merge', () => {
  it('separates "no camera events" from "no stream address"', () => {
    const { unmount } = render(<CameraPane camera={undefined} now={NOW} />)
    expect(screen.getByText(/has not announced a stream/i)).toBeInTheDocument()
    unmount()

    render(<CameraPane camera={camera({ stream_url: undefined })} now={NOW} />)
    expect(screen.getByText(/has not announced a stream address/i)).toBeInTheDocument()
  })

  it('says a dead stream is not a ledger problem', () => {
    // The events may be arriving perfectly while the video source is down.
    // Conflating the two sends someone to debug the wrong system.
    render(<CameraPane camera={camera()} now={NOW} />)
    fireEvent.error(screen.getByRole('img', { name: /live camera/i }))

    expect(screen.getByText(/Stream unreachable/i)).toBeInTheDocument()
    expect(screen.getByText(/ledger feed is unaffected/i)).toBeInTheDocument()
  })

  it('still shows the last event when the stream is down', () => {
    render(<CameraPane camera={camera({ event: 'motion_detected' })} now={NOW} />)
    fireEvent.error(screen.getByRole('img', { name: /live camera/i }))

    expect(screen.getByText(/motion detected/i)).toBeInTheDocument()
  })

  it('retries when the camera announces a new address', () => {
    // One failure must not permanently hide a camera that came back elsewhere.
    const { rerender } = render(<CameraPane camera={camera()} now={NOW} />)
    fireEvent.error(screen.getByRole('img', { name: /live camera/i }))
    expect(screen.getByText(/Stream unreachable/i)).toBeInTheDocument()

    rerender(
      <CameraPane camera={camera({ stream_url: 'http://moved.local:8090/stream' })} now={NOW} />,
    )
    expect(screen.getByRole('img', { name: /live camera/i })).toHaveAttribute(
      'src',
      'http://moved.local:8090/stream',
    )
  })
})

describe('no media ever reaches the browser through a block', () => {
  it('renders only from an address, never from frame data', () => {
    // A regression guard for ADR 0011: if a future change starts passing frames
    // through the ledger, the pane would need a data: URL and this would fail.
    render(<CameraPane camera={camera()} now={NOW} />)
    const src = screen.getByRole('img', { name: /live camera/i }).getAttribute('src')!

    expect(src.startsWith('http')).toBe(true)
    expect(src).not.toContain('data:')
    expect(src).not.toContain('base64')
  })
})

describe('a stream that dies mid-flight', () => {
  it('does not keep claiming to be live for ever', async () => {
    // The image already loaded, so no error fires and the last frame stays on
    // screen. A frozen frame captioned "streaming" is worse than no frame.
    vi.useFakeTimers()
    try {
      render(<CameraPane camera={camera()} now={NOW} watchdogMs={1000} />)
      const before = screen.getByRole('img', { name: /live camera/i }).getAttribute('src')

      await act(async () => {
        vi.advanceTimersByTime(1000)
      })

      // Reconnected: a new request, which will error if the source is gone.
      const after = screen.getByRole('img', { name: /live camera/i }).getAttribute('src')
      expect(after).not.toBe(before)
      expect(after).toContain('device.local:8090/stream')
    } finally {
      vi.useRealTimers()
    }
  })

  it('reports the device saying its stream went offline', () => {
    // More trustworthy than any probe, when the device bothers to say so.
    render(<CameraPane camera={camera({ event: 'stream_offline' })} now={NOW} />)

    expect(screen.getByText(/went offline/i)).toBeInTheDocument()
    expect(screen.queryByRole('img', { name: /live camera/i })).not.toBeInTheDocument()
  })

  it('stops the watchdog once the stream is known to be unreachable', () => {
    vi.useFakeTimers()
    try {
      render(<CameraPane camera={camera()} now={NOW} watchdogMs={1000} />)
      fireEvent.error(screen.getByRole('img', { name: /live camera/i }))
      act(() => {
        vi.advanceTimersByTime(10_000)
      })
      // Still reporting unreachable, not flickering back to a live caption.
      expect(screen.getByText(/Stream unreachable/i)).toBeInTheDocument()
    } finally {
      vi.useRealTimers()
    }
  })
})
