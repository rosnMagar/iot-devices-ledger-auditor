// uPlot line chart. Chosen over a React charting library because several panels
// redraw on every reading and uPlot is roughly an order of magnitude lighter.

import { useEffect, useRef, useState } from 'react'
import uPlot from 'uplot'
import 'uplot/dist/uPlot.min.css'

import type { Series } from './chartData'

const STROKES = ['#2b7a3d', '#b3541e', '#2f5d94']

// uPlot needs pixel dimensions, so the container is measured rather than sized
// in CSS. Without this the chart keeps whatever width it was born with and a
// wider panel buys nothing.
function useContainerWidth(ref: React.RefObject<HTMLDivElement | null>, fallback: number) {
  const [width, setWidth] = useState(fallback)

  useEffect(() => {
    const element = ref.current
    if (element === null) return
    const measure = () => setWidth(Math.max(160, element.clientWidth))
    measure()

    // Absent in jsdom, and in older browsers. The fallback width still renders.
    if (typeof ResizeObserver === 'undefined') return
    const observer = new ResizeObserver(measure)
    observer.observe(element)
    return () => observer.disconnect()
  }, [ref, fallback])

  return width
}

export default function SeriesChart({
  series,
  unit,
  height = 180,
  minWidth = 280,
}: {
  series: Series
  unit: string | null
  height?: number
  minWidth?: number
}) {
  const host = useRef<HTMLDivElement | null>(null)
  const plot = useRef<uPlot | null>(null)
  const width = useContainerWidth(host, minWidth)

  useEffect(() => {
    if (host.current === null) return

    const options: uPlot.Options = {
      width,
      height,
      // The reading's own unit, so a swapped probe is not drawn on the wrong axis.
      axes: [{}, { label: unit ?? '' }],
      legend: { show: series.labels.length > 1 },
      series: [
        {},
        ...series.labels.map((label, i) => ({
          label,
          stroke: STROKES[i % STROKES.length],
          width: 1.5,
          points: { show: series.data[0].length < 40 },
          // Break the line on null rather than joining across a failed read.
          spanGaps: false,
        })),
      ],
    }

    plot.current = new uPlot(options, series.data as uPlot.AlignedData, host.current)
    return () => {
      plot.current?.destroy()
      plot.current = null
    }
    // Rebuilt rather than updated: the axis count changes when a sensor switches
    // between scalar and vector, which uPlot cannot do in place.
  }, [series, unit, width, height])

  return <div ref={host} data-testid="series-chart" style={{ width: '100%' }} />
}
