// "2m ago" from an ISO timestamp.

const MINUTE = 60
const HOUR = 60 * MINUTE
const DAY = 24 * HOUR

// JS parses an ISO string with no offset as *local* time, which would skew every
// reading by the viewer's UTC offset. The API sends offsets, so this only guards
// against a regression there.
function parseUtc(iso: string): number {
  const hasZone = /(Z|[+-]\d{2}:?\d{2})$/.test(iso)
  return Date.parse(hasZone ? iso : `${iso}Z`)
}

export function relativeTime(iso: string | null, now: Date = new Date()): string {
  if (iso === null) return 'never'

  const then = parseUtc(iso)
  if (Number.isNaN(then)) return 'unknown'

  const seconds = Math.round((now.getTime() - then) / 1000)
  // Negative means the box clock is ahead of the browser's, not a future reading.
  if (seconds < 1) return 'just now'
  if (seconds < MINUTE) return `${seconds}s ago`
  if (seconds < HOUR) return `${Math.floor(seconds / MINUTE)}m ago`
  if (seconds < DAY) return `${Math.floor(seconds / HOUR)}h ago`
  return `${Math.floor(seconds / DAY)}d ago`
}
