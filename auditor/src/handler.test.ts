describe('auditor stub', () => {
  it('exports a handler function', async () => {
    const { handler } = await import('./handler')
    expect(typeof handler).toBe('function')
  })
})
