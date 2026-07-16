import 'dotenv/config';
import express from 'express';
import fs from 'fs';
import path from 'path';
import { fileURLToPath } from 'url';
import { v4 as uuidv4 } from 'uuid';
import initSqlJs from 'sql.js';

import { createCorrelator, seedMixingMemory } from './correlator.js';
import { generateResponse, getModelInfo, initModel } from './gemmaAssistant.js';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const PORT = parseInt(process.env.PORT || '3921', 10);

const appData = process.env.ANNA_APPDATA
  || path.join(process.env.APPDATA || path.join(__dirname, 'data'), 'ANNA');
const DB_DIR = process.env.ANNA_DB_DIR || path.join(appData, 'assistant');
const DB_PATH = path.join(DB_DIR, 'assistant.db');

if (!fs.existsSync(DB_DIR)) fs.mkdirSync(DB_DIR, { recursive: true });

let db = null;
let saveTimer = null;
let correlator = null;

function save() {
  if (db) fs.writeFileSync(DB_PATH, Buffer.from(db.export()));
}

function scheduleSave() {
  if (saveTimer) clearTimeout(saveTimer);
  saveTimer = setTimeout(save, 800);
}

function query(sql, params = []) {
  try {
    const stmt = db.prepare(sql);
    if (params.length) stmt.bind(params);
    const rows = [];
    while (stmt.step()) rows.push(stmt.getAsObject());
    stmt.free();
    return rows;
  } catch {
    return [];
  }
}

function get(sql, params = []) {
  return query(sql, params)[0] || null;
}

function run(sql, params = []) {
  try {
    db.run(sql, params);
    scheduleSave();
  } catch (e) {
    console.error('[DB]', e.message);
  }
}

async function initDb() {
  const SQL = await initSqlJs();
  db = fs.existsSync(DB_PATH)
    ? new SQL.Database(fs.readFileSync(DB_PATH))
    : new SQL.Database();

  db.run(`CREATE TABLE IF NOT EXISTS messages (
    id TEXT PRIMARY KEY, role TEXT, content TEXT, created_at INTEGER, metadata TEXT)`);
  db.run(`CREATE TABLE IF NOT EXISTS counter (id INTEGER PRIMARY KEY, val INTEGER)`);
  db.run(`INSERT OR IGNORE INTO counter VALUES(1, 0)`);

  const schema = `id TEXT PRIMARY KEY, pk TEXT UNIQUE, w1 TEXT, w2 TEXT, p1 TEXT, p2 TEXT,
    rel TEXT, sent TEXT, score REAL, reinf INTEGER, decay_at INTEGER, last_msg INTEGER,
    created INTEGER, updated INTEGER`;

  db.run(`CREATE TABLE IF NOT EXISTS short (${schema})`);
  db.run(`CREATE TABLE IF NOT EXISTS medium (${schema})`);
  db.run(`CREATE TABLE IF NOT EXISTS long_term (${schema})`);

  save();

  correlator = createCorrelator(db, { query, get, run, scheduleSave });
  const seeded = seedMixingMemory(run, get, scheduleSave);
  if (seeded > 0) console.log(`[DB] Seeded ${seeded} mixing memory pairs`);
  console.log('[DB]', DB_PATH);
}

function insertMessage(role, content, meta = {}) {
  const id = uuidv4();
  const ts = Date.now();
  run('INSERT INTO messages VALUES(?,?,?,?,?)', [
    id, role, content, ts, JSON.stringify(meta)
  ]);
  return { id, role, content, created_at: ts };
}

const app = express();
app.use(express.json({ limit: '2mb' }));
app.use((req, res, next) => {
  res.header('Access-Control-Allow-Origin', '*');
  res.header('Access-Control-Allow-Methods', 'GET,POST,OPTIONS');
  res.header('Access-Control-Allow-Headers', 'Content-Type');
  if (req.method === 'OPTIONS') return res.sendStatus(204);
  next();
});

app.get('/health', (_, res) => {
  const model = getModelInfo();
  res.json({
    status: 'ok',
    db: DB_PATH,
    llm: 'ollama',
    model: model.model,
    host: model.host,
    ollamaReachable: model.ollamaReachable
  });
});

app.get('/api/memory/stats', (_, res) => {
  res.json(correlator?.stats() || {});
});

app.get('/api/memory/search', (req, res) => {
  const q = req.query.q || '';
  res.json(correlator?.search(q) || []);
});

app.post('/api/chat', async (req, res) => {
  const { message, trackContext, history } = req.body || {};
  if (!message || typeof message !== 'string') {
    return res.status(400).json({ error: 'Need message' });
  }

  try {
    insertMessage('user', message, { scope: trackContext?.scope });

    const mem = correlator.memoryContext(message);
    const positiveFeedback = /\b(fixed|better|improved|sounds good|that worked|perfect|helped|great)\b/i.test(message);
    correlator.processMsg(message, 'user', { boost: positiveFeedback ? 2.5 : 1 });

    const response = await generateResponse({
      message,
      trackContext: trackContext || null,
      history: history || [],
      memoryContext: mem
    });

    insertMessage('assistant', response, { model: getModelInfo().model });

    res.json({
      response,
      memoryStats: correlator.stats()
    });
  } catch (e) {
    console.error('[chat]', e.message);
    res.status(500).json({ error: e.message || 'Internal error' });
  }
});

async function start() {
  console.log('ANNA Gemma Assistant');
  await initDb();
  await initModel();

  app.listen(PORT, '127.0.0.1', () => {
    const { model, host } = getModelInfo();
    console.log(`Listening on http://127.0.0.1:${PORT}`);
    console.log(`Ollama: ${host}  model: ${model}`);
  });
}

process.on('SIGINT', () => { save(); process.exit(0); });
process.on('SIGTERM', () => { save(); process.exit(0); });

start().catch(e => { console.error('Fatal:', e); process.exit(1); });
