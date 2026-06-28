import { Handler } from 'aws-lambda'

const STORAGE_CORE_URL = process.env.STORAGE_CORE_URL ?? 'http://localhost:8080'

export const handler: Handler = async (event) => {
  console.log('auditor stub invoked', JSON.stringify(event))

  // Validates that a non-VPC Lambda can reach the EC2 box over the public internet.
  const url = `${STORAGE_CORE_URL}/verify`
  console.log(`calling storage-core: GET ${url}`)

  const resp = await fetch(url)
  const body = await resp.json()

  console.log('storage-core /verify response:', JSON.stringify(body))
  return { statusCode: 200, body: JSON.stringify(body) }
}
