const BASE = process.env.ASSISTANT_URL || 'http://127.0.0.1:3921';

async function main() {
  const health = await fetch(`${BASE}/health`);
  if (!health.ok) throw new Error(`health failed: ${health.status}`);
  const healthJson = await health.json();
  console.log('health:', healthJson.status, healthJson.model);

  const mockContext = {
    scope: 'channel',
    channel: {
      index: 0,
      label: 'Kick',
      gain: 0.8,
      peakLevel: 0.4,
      steps: { activeSteps: 4, avgVelocity: 0.9, velocityVariance: 0.01 }
    }
  };

  const chat = await fetch(`${BASE}/api/chat`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({
      message: 'Why might this kick sound flat?',
      trackContext: mockContext,
      history: []
    })
  });

  if (!chat.ok) {
    const err = await chat.text();
    throw new Error(`chat failed: ${chat.status} ${err}`);
  }

  const chatJson = await chat.json();
  if (!chatJson.response) throw new Error('missing response');
  console.log('response length:', chatJson.response.length);
  console.log('memory:', chatJson.memoryStats);

  const stats = await fetch(`${BASE}/api/memory/stats`);
  const statsJson = await stats.json();
  console.log('stats:', statsJson);
  console.log('smoke ok');
}

main().catch(e => {
  console.error('smoke failed:', e.message);
  process.exit(1);
});