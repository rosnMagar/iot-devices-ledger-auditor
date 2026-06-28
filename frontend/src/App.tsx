import { useEffect, useState } from 'react'

const API = (import.meta.env.VITE_API_BASE_URL as string | undefined) ?? 'http://localhost:8000'

interface Block {
  index: number
  timestamp: string
  event: {
    event_type: string
    location_id: string
    actor: string
    description: string
  }
  prev_hash: string
  hash: string
}

interface User {
  id: number
  username: string
  role: string
}

export default function App() {
  const [blocks, setBlocks] = useState<Block[]>([])
  const [users, setUsers] = useState<User[]>([])
  const [error, setError] = useState<string | null>(null)

  useEffect(() => {
    Promise.all([
      fetch(`${API}/blocks`).then((r) => r.json()),
      fetch(`${API}/users`).then((r) => r.json()),
    ])
      .then(([blocksData, usersData]: [{ blocks: Block[] }, User[]]) => {
        setBlocks(blocksData.blocks ?? [])
        setUsers(usersData)
      })
      .catch((e: unknown) => setError(String(e)))
  }, [])

  if (error) return <p style={{ color: 'red' }}>Error: {error}</p>

  return (
    <main style={{ fontFamily: 'monospace', padding: '2rem', maxWidth: '900px' }}>
      <h1>IoT Devices Ledger Auditor</h1>
      <p style={{ color: '#888' }}>Phase 0 walking skeleton — stub data via backend-api → storage-core</p>

      <h2>Blocks ({blocks.length})</h2>
      <pre style={{ background: '#f4f4f4', padding: '1rem', overflowX: 'auto' }}>
        {JSON.stringify(blocks, null, 2)}
      </pre>

      <h2>Users ({users.length})</h2>
      <pre style={{ background: '#f4f4f4', padding: '1rem', overflowX: 'auto' }}>
        {JSON.stringify(users, null, 2)}
      </pre>
    </main>
  )
}
