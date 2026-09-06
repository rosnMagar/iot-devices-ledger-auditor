// IOT-40 — the devices table: status rendering and the sort controls.

import { render, screen, within } from '@testing-library/react'
import userEvent from '@testing-library/user-event'
import { describe, expect, it, vi } from 'vitest'

import type { Device } from './api'
import DevicesTable from './DevicesTable'

const NOW = new Date('2026-09-06T12:00:00Z')

const device = (overrides: Partial<Device> = {}): Device => ({
  device_id: 'esp32-01',
  location_id: 'warehouse-a',
  device_type: 'DHT22',
  registered_at: '2026-01-01T00:00:00+00:00',
  last_seen: '2026-09-06T11:58:00+00:00',
  status: 'active',
  ...overrides,
})

function renderTable(devices: Device[], props: Partial<Parameters<typeof DevicesTable>[0]> = {}) {
  const onSort = vi.fn()
  render(
    <DevicesTable
      devices={devices}
      now={NOW}
      sort="last_seen"
      order="desc"
      onSort={onSort}
      {...props}
    />,
  )
  return { onSort }
}

// Row lookup by device id, so assertions cannot drift onto a neighbouring row.
const rowFor = (id: string) => screen.getByRole('cell', { name: id }).closest('tr')!

describe('rendering', () => {
  it('shows an active device with its relative last-seen time', () => {
    renderTable([device()])
    const row = within(rowFor('esp32-01'))

    expect(row.getByText('active')).toBeInTheDocument()
    expect(row.getByText('2m ago')).toBeInTheDocument()
    expect(row.getByText('DHT22')).toBeInTheDocument()
    expect(row.getByText('warehouse-a')).toBeInTheDocument()
  })

  it('distinguishes inactive from active', () => {
    renderTable([
      device({ device_id: 'up', status: 'active' }),
      device({ device_id: 'down', status: 'inactive', last_seen: '2026-09-06T09:00:00+00:00' }),
    ])

    expect(within(rowFor('up')).getByText('active')).toBeInTheDocument()
    expect(within(rowFor('down')).getByText('inactive')).toBeInTheDocument()
    expect(within(rowFor('down')).getByText('3h ago')).toBeInTheDocument()
  })

  it('shows a device that has never reported as "never", not as 56 years ago', () => {
    renderTable([device({ device_id: 'fresh', last_seen: null, status: 'inactive' })])
    expect(within(rowFor('fresh')).getByText('never')).toBeInTheDocument()
  })

  it('marks a device with no location as unassigned', () => {
    renderTable([device({ device_id: 'nowhere', location_id: null })])
    expect(within(rowFor('nowhere')).getByText('unassigned')).toBeInTheDocument()
  })

  it('says nothing is registered when the fleet is empty', () => {
    renderTable([])
    expect(screen.getByText('No devices registered yet.')).toBeInTheDocument()
    expect(screen.queryByRole('table')).not.toBeInTheDocument()
  })

  it('distinguishes an empty fleet from filters that match nothing', () => {
    // Collapsing these makes a bad filter look like an empty fleet.
    renderTable([], { emptyMessage: 'No devices match these filters.' })
    expect(screen.getByText('No devices match these filters.')).toBeInTheDocument()
  })
})

describe('sorting', () => {
  it('offers only the columns the backend can sort by', () => {
    renderTable([device()])
    const headers = screen.getAllByRole('button').map((b) => b.textContent)

    // Type and Status are not sortable, so they must not look clickable.
    expect(headers).toEqual(['Name↕', 'Location↕', 'Last seen▼'])
  })

  it('asks for a new column when an inactive header is clicked', async () => {
    const { onSort } = renderTable([device()])
    await userEvent.click(screen.getByRole('button', { name: /Name/ }))

    expect(onSort).toHaveBeenCalledWith('name')
  })

  it('asks again for the same column when the active header is clicked', async () => {
    // The parent turns a repeat click into a direction flip (see query.test.ts).
    const { onSort } = renderTable([device()])
    await userEvent.click(screen.getByRole('button', { name: /Last seen/ }))

    expect(onSort).toHaveBeenCalledWith('last_seen')
  })

  it.each([
    ['desc', 'descending', '▼'],
    ['asc', 'ascending', '▲'],
  ] as const)('marks the active column as %s for assistive tech', (order, aria, arrow) => {
    renderTable([device()], { sort: 'name', order })

    const header = screen.getByRole('columnheader', { name: /Name/ })
    expect(header).toHaveAttribute('aria-sort', aria)
    expect(header.textContent).toContain(arrow)
  })

  it('leaves the inactive columns unsorted rather than claiming a direction', () => {
    renderTable([device()], { sort: 'name', order: 'asc' })

    expect(screen.getByRole('columnheader', { name: /Location/ })).toHaveAttribute(
      'aria-sort',
      'none',
    )
  })
})
