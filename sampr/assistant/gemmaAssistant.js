import 'dotenv/config';
import { execSync } from 'child_process';

const OLLAMA_HOST = process.env.OLLAMA_HOST || 'http://localhost:11434';

const MODEL_PRIORITY = [
  'gemma2:9b',
  'gemma:7b',
  'gemma2:2b',
  'gemma3:4b',
  'gemma2',
  'gemma3:12b'
];

let activeModel = process.env.OLLAMA_MODEL || 'gemma2:9b';
let ollamaReachable = false;

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

  return installed.find(n => n.startsWith('gemma')) || null;
}

export async function initModel() {
  const preferred = process.env.OLLAMA_MODEL || 'gemma2:9b';

  try {
    const res = await fetch(`${OLLAMA_HOST}/api/tags`, { signal: AbortSignal.timeout(5000) });
    if (res.ok) {
      ollamaReachable = true;
      const data = await res.json();
      const installed = (data.models || []).map(m => m.name);
      const picked = pickModel(installed, preferred);
      if (picked) {
        activeModel = picked;
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
    console.log(`[Ollama] Using model (local list): ${activeModel}`);
    return activeModel;
  }

  activeModel = preferred;
  console.warn(`[Ollama] No Gemma model found. Defaulting to ${activeModel}`);
  console.warn('         Run: ollama pull gemma2:9b  (or gemma3:4b)');
  return activeModel;
}

export async function callOllama(messages, options = {}) {
  const controller = new AbortController();
  const timeout = setTimeout(() => controller.abort(), options.timeoutMs || 90000);

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
          num_predict: options.maxTokens ?? 600
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
  } finally {
    clearTimeout(timeout);
  }
}

export function buildSystemPrompt({ memoryContext, trackContext }) {
  const contextJson = trackContext
    ? JSON.stringify(trackContext, null, 2)
    : '(no track context attached)';

  const memoryBlock = memoryContext
    ? `\n## Conversation memory (secondary)\n${memoryContext}\n`
    : '';

  return `You are Gemma, a SAMPR mixing engineer assistant. You help producers diagnose and improve their beats using numeric track data.

## Primary data - current track context (JSON)
Use ONLY these numbers when making claims about the mix. Cite specific values (gain, pan, peak, velocity stats, EQ dB, compressor settings, etc.).

\`\`\`json
${contextJson}
\`\`\`
${memoryBlock}
## Rules
- Be conversational and practical - 2-5 sentences unless the user asks for detail
- Diagnose issues like "flat", "muddy", "thin", "harsh" using the numeric context above
- Suggest concrete FX/mixer changes (EQ cuts/boosts in dB, compressor threshold/ratio, gain adjustments)
- Do NOT invent spectral analysis, LUFS, or waveform data that is not in the JSON
- Do NOT mention databases, correlations, or how your memory works
- If track context is empty or missing channels, say what data you need
- You give advice only - you cannot change settings in the app`;
}

export async function generateResponse({ message, trackContext, history, memoryContext }) {
  const systemPrompt = buildSystemPrompt({ memoryContext, trackContext });

  const messages = [{ role: 'system', content: systemPrompt }];

  if (Array.isArray(history)) {
    for (const entry of history.slice(-8)) {
      if (!entry?.content) continue;
      const role = entry.role === 'assistant' ? 'assistant' : 'user';
      messages.push({ role, content: String(entry.content) });
    }
  }

  messages.push({ role: 'user', content: message });

  return callOllama(messages);
}

export function getModelInfo() {
  return { host: OLLAMA_HOST, model: activeModel, ollamaReachable };
}