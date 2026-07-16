import winkPOS from 'wink-pos-tagger';
import { v4 as uuidv4 } from 'uuid';

const tagger = winkPOS();

const STOPS = new Set([
  'a', 'an', 'the', 'i', 'me', 'my', 'we', 'you', 'he', 'she', 'it', 'they',
  'is', 'are', 'was', 'were', 'be', 'been', 'have', 'has', 'had', 'do', 'does', 'did',
  'will', 'would', 'can', 'could', 'and', 'but', 'or', 'if', 'of', 'to', 'in', 'for',
  'on', 'with', 'at', 'by', 'from', 'so', 'as', 'that', 'this', 'what', 'which', 'who',
  'how', 'when', 'where', 'why', 'all', 'some', 'any', 'no', 'not', 'just', 'also',
  'very', 'too', 'really', 'like', 'get', 'got', 'ok', 'yeah', 'yes', 'hey', 'hi', 'hello',
  'gemma', 'anna', 'channel', 'track'
]);

const POS = {
  NN: 'noun', NNS: 'noun', NNP: 'noun',
  VB: 'verb', VBD: 'verb', VBG: 'verb', VBN: 'verb', VBP: 'verb', VBZ: 'verb',
  JJ: 'adj', JJR: 'adj', JJS: 'adj',
  RB: 'adv'
};

export function createCorrelator(db, { query, get, run, scheduleSave }) {
  function tokenize(text) {
    const clean = text.toLowerCase().replace(/[^\w\s'-]/g, ' ').trim();
    if (!clean) return [];
    return tagger.tagSentence(clean)
      .filter(t => !STOPS.has(t.value) && t.value.length >= 3)
      .map((t, i) => ({ word: t.value, pos: t.tag, spos: POS[t.tag] || 'noun', idx: i }));
  }

  function score(p1, p2, dist) {
    const cat = (p1 === 'noun' && p2 === 'noun') ? 0.3
      : (p1 === 'adj' || p2 === 'adj') ? 0.2 : 0.1;
    const prox = dist === 0 ? 0.4 : dist === 1 ? 0.3 : dist <= 3 ? 0.2 : 0.1;
    return Math.min(0.15 + cat + prox, 1.0);
  }

  function tier(s) {
    return s >= 0.65 ? 'long_term' : s >= 0.25 ? 'medium' : 'short';
  }

  function nextIdx() {
    run('UPDATE counter SET val = val + 1 WHERE id = 1');
    return get('SELECT val FROM counter WHERE id = 1')?.val || 1;
  }

  function processMsg(text, source = 'user', options = {}) {
    if (source !== 'user' || !text) return { added: 0, reinforced: 0 };

    const boost = options.boost || 1;
    const idx = nextIdx();
    const tokens = tokenize(text);
    if (tokens.length < 2) return { added: 0, reinforced: 0 };

    const now = Date.now();
    let added = 0;
    let reinforced = 0;

    for (let i = 0; i < tokens.length; i++) {
      for (let j = i + 1; j < Math.min(i + 5, tokens.length); j++) {
        const a = tokens[i];
        const b = tokens[j];
        const pk = [a.word, b.word].sort().join('_');
        const sc = score(a.spos, b.spos, j - i - 1) * boost;

        let existing = null;
        let existingTier = null;
        for (const t of ['long_term', 'medium', 'short']) {
          const found = get(`SELECT * FROM ${t} WHERE pk = ?`, [pk]);
          if (found) {
            existing = found;
            existingTier = t;
            break;
          }
        }

        if (existing) {
          const newScore = Math.min(1, existing.score + sc);
          const newTier = tier(newScore);
          run(`DELETE FROM ${existingTier} WHERE pk = ?`, [pk]);
          run(`INSERT INTO ${newTier} VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?)`, [
            existing.id, pk, a.word, b.word, a.pos, b.pos, `${a.spos}+${b.spos}`,
            text.slice(0, 100), newScore, existing.reinf + 1, idx + 200, idx,
            existing.created, now
          ]);
          reinforced++;
        } else {
          const t = tier(sc);
          run(`INSERT INTO ${t} VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?)`, [
            uuidv4(), pk, a.word, b.word, a.pos, b.pos, `${a.spos}+${b.spos}`,
            text.slice(0, 100), sc, 1, idx + 100, idx, now, now
          ]);
          added++;
        }
      }
    }

    scheduleSave();
    return { added, reinforced };
  }

  function search(word) {
    const w = word.toLowerCase();
    const results = [];
    for (const t of ['long_term', 'medium', 'short']) {
      query(`SELECT * FROM ${t} WHERE w1 = ? OR w2 = ? ORDER BY score DESC LIMIT 10`, [w, w])
        .forEach(r => { r.tier = t; results.push(r); });
    }
    return results;
  }

  function memoryContext(text) {
    const words = text.toLowerCase()
      .replace(/[^\w\s]/g, '')
      .split(/\s+/)
      .filter(w => w.length > 2 && !STOPS.has(w));
    if (!words.length) return '';

    const results = [];
    for (const w of words.slice(0, 6)) {
      results.push(...search(w));
    }

    const unique = [...new Map(results.map(r => [r.id, r])).values()]
      .sort((a, b) => (b.score || 0) - (a.score || 0))
      .slice(0, 12);

    if (!unique.length) return '';

    return 'Past mixing conversations suggest: '
      + unique.map(c => `"${c.w1}" and "${c.w2}" often appear together (${c.tier.replace('_', ' ')})`)
        .join('; ')
      + '.';
  }

  function stats() {
    return {
      short: get('SELECT COUNT(*) as c FROM short')?.c || 0,
      medium: get('SELECT COUNT(*) as c FROM medium')?.c || 0,
      long: get('SELECT COUNT(*) as c FROM long_term')?.c || 0,
      messages: get('SELECT val FROM counter WHERE id = 1')?.val || 0
    };
  }

  return { processMsg, memoryContext, search, stats, tokenize };
}

export const MIXING_SEEDS = [
  ['flat', 'compressor'], ['flat', 'velocity'], ['flat', 'dynamic'],
  ['muddy', 'low'], ['muddy', 'eq'], ['muddy', 'gain'],
  ['thin', 'reverb'], ['thin', 'high'], ['thin', 'eq'],
  ['punch', 'attack'], ['punch', 'compressor'], ['punch', 'transient'],
  ['harsh', 'high'], ['harsh', 'eq'], ['harsh', 'gain'],
  ['dull', 'high'], ['dull', 'eq'], ['dull', 'brightness'],
  ['clipping', 'gain'], ['clipping', 'peak'], ['clipping', 'compressor'],
  ['wide', 'reverb'], ['wide', 'stereo'], ['narrow', 'pan'],
  ['better', 'fixed'], ['fixed', 'sounds'], ['improved', 'mix']
];

export function seedMixingMemory(run, get, scheduleSave) {
  const existing = get('SELECT COUNT(*) as c FROM medium')?.c || 0;
  if (existing >= MIXING_SEEDS.length) return 0;

  const now = Date.now();
  let seeded = 0;

  for (const [w1, w2] of MIXING_SEEDS) {
    const pk = [w1, w2].sort().join('_');
    if (get('SELECT id FROM medium WHERE pk = ?', [pk])) continue;

    run(`INSERT INTO medium VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?)`, [
      uuidv4(), pk, w1, w2, 'NN', 'NN', 'noun+noun',
      `mixing: ${w1} ${w2}`, 0.45, 2, 50, 1, now, now
    ]);
    seeded++;
  }

  if (seeded > 0) scheduleSave();
  return seeded;
}
