import 'dotenv/config';
import { execSync } from 'child_process';

const OLLAMA_HOST = process.env.OLLAMA_HOST || 'http://localhost:11434';

const MODEL_PRIORITY = [
  'llama3.1:8b',
  'gemma3:4b',
  'gemma2:2b',
  'gemma2:9b',
  'gemma:7b',
  'gemma2',
  'qwen2.5:14b-instruct',
  'gemma3:12b'
];

let activeModel = process.env.OLLAMA_MODEL || 'llama3.1:8b';
let ollamaReachable = false;
let modelAvailable = false;

function listLocalModels() {
  try {
    const out = execSync('ollama list', { encoding: 'utf8', timeout: 8000 });
    const names = [];
    for (const line of out.split('\n').slice(1)) {
      const name = line.trim().split(/\s+/)[0];
      if (name) names.push(name);
    }
    return names;
  } catch {
    return [];
  }
}

function pickModel(installed, preferred) {
  if (preferred && installed.some(n => n === preferred || n.startsWith(`${preferred}:`))) {
    return installed.find(n => n === preferred || n.startsWith(`${preferred}:`));
  }

  for (const candidate of MODEL_PRIORITY) {
    const hit = installed.find(n => n === candidate || n.startsWith(`${candidate}:`));
    if (hit) return hit;
  }

  return installed.find(n => n.startsWith('llama3.1'))
      || installed.find(n => n.startsWith('gemma'))
      || installed[0]
      || null;
}

export async function initModel() {
  const preferred = process.env.OLLAMA_MODEL || 'llama3.1:8b';

  try {
    const res = await fetch(`${OLLAMA_HOST}/api/tags`, { signal: AbortSignal.timeout(5000) });
    if (res.ok) {
      ollamaReachable = true;
      const data = await res.json();
      const installed = (data.models || []).map(m => m.name);
      const picked = pickModel(installed, preferred);
      if (picked) {
        activeModel = picked;
        modelAvailable = true;
        console.log(`[Ollama] Using model: ${activeModel}`);
        return activeModel;
      }
    }
  } catch {
    ollamaReachable = false;
  }

  const local = listLocalModels();
  const picked = pickModel(local, preferred);
  if (picked) {
    ollamaReachable = true;
    activeModel = picked;
    modelAvailable = true;
    console.log(`[Ollama] Using model (local list): ${activeModel}`);
    return activeModel;
  }

  activeModel = preferred;
  modelAvailable = false;
  console.warn(`[Ollama] No ANNA-compatible local model found. Defaulting to ${activeModel}`);
  console.warn('         Run: ollama pull llama3.1:8b');
  return activeModel;
}

export async function callOllama(messages, options = {}) {
  if (!modelAvailable) {
    await initModel();
    if (!modelAvailable)
      throw new Error(`ANNA local model unavailable. Run: ollama pull ${activeModel}`);
  }

  const controller = new AbortController();
  const timeout = setTimeout(() => controller.abort(), options.timeoutMs || 18000);

  try {
    const res = await fetch(`${OLLAMA_HOST}/api/chat`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        model: activeModel,
        messages,
        stream: false,
        options: {
          temperature: options.temperature ?? 0.6,
          num_ctx: options.contextTokens ?? 2048,
          num_predict: options.maxTokens ?? 96
        }
      }),
      signal: controller.signal
    });

    if (!res.ok) {
      const err = await res.text();
      if (res.status === 404 && err.includes('not found')) {
        throw new Error(`Model "${activeModel}" not in Ollama. Run: ollama pull ${activeModel}`);
      }
      throw new Error(`Ollama error ${res.status}: ${err}`);
    }

    ollamaReachable = true;
    const data = await res.json();
    return data.message?.content || '';
  } catch (e) {
    if (e?.name === 'AbortError') {
      throw new Error('ANNA local model timed out while generating a response.');
    }

    throw e;
  } finally {
    clearTimeout(timeout);
  }
}

export function buildSystemPrompt({ memoryContext, trackContext }) {
  const contextJson = trackContext
    ? JSON.stringify(trackContext).slice(0, 3500)
    : '(no track context attached)';

  const memoryBlock = memoryContext
    ? `\nMemory: ${memoryContext.slice(0, 500)}\n`
    : '';

  return `You are ANNA, the local mixing engineer inside the ANNA music production app.

Track context JSON: ${contextJson}
${memoryBlock}
- Be conversational and practical - 1-3 concise sentences unless the user asks for detail
- Diagnose issues like "flat", "muddy", "thin", "harsh" using the numeric context above
- Suggest concrete FX/mixer changes (EQ cuts/boosts in dB, compressor threshold/ratio, gain adjustments)
- Do NOT invent spectral analysis, LUFS, or waveform data that is not in the JSON
- If data is missing, say what to check
- You give advice only - you cannot change settings in the app`;
}

export async function generateResponse({ message, trackContext, history, memoryContext }) {
  const systemPrompt = buildSystemPrompt({ memoryContext, trackContext });

  const messages = [{ role: 'system', content: systemPrompt }];

  if (Array.isArray(history)) {
    for (const entry of history.slice(-4)) {
      if (!entry?.content) continue;
      const role = entry.role === 'assistant' ? 'assistant' : 'user';
      messages.push({ role, content: String(entry.content) });
    }
  }

  messages.push({ role: 'user', content: message });

  try {
    return await callOllama(messages);
  } catch (e) {
    if (String(e?.message || '').toLowerCase().includes('timed out'))
      return buildFallbackResponse(trackContext);

    throw e;
  }
}

export function getModelInfo() {
  return { host: OLLAMA_HOST, model: activeModel, ollamaReachable, modelAvailable };
}

function buildFallbackResponse(trackContext) {
  const channel = trackContext?.channel || null;
  const label = channel?.label || 'this track';
  const peak = Number.isFinite(channel?.peakLevel) ? channel.peakLevel : null;
  const gain = Number.isFinite(channel?.gain) ? channel.gain : null;
  const activeSteps = channel?.steps?.activeSteps;
  const velocity = channel?.steps?.avgVelocity;

  const details = [];

  if (gain !== null)
    details.push(`gain is ${gain.toFixed(2)}`);

  if (peak !== null)
    details.push(`peak is ${peak.toFixed(2)}`);

  if (Number.isFinite(activeSteps))
    details.push(`${activeSteps} active steps`);

  if (Number.isFinite(velocity))
    details.push(`average velocity is ${velocity.toFixed(2)}`);

  const basis = details.length ? ` I am reading ${details.join(', ')}.` : '';
  return `ANNA's local model took too long, so here is the fast mix check for ${label}.${basis} If it sounds flat, raise variation first: change a few step velocities, add a small EQ presence boost around 2-5 kHz, and use light compression only if the peak is jumping too much.`;
}
