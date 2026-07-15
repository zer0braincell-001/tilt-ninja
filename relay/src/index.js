// tilt-ninja relay — Durable Object per room.
// controller -> broadcast ke semua display; display -> kirim ke controller (echo latency).
// Stateless: nggak nyimpen apa-apa ke storage.

export class Room {
  constructor() {
    this.controllers = new Set();
    this.displays = new Set();
  }

  async fetch(request) {
    if (request.headers.get('Upgrade') !== 'websocket') {
      return new Response('expected websocket', { status: 426 });
    }
    const url = new URL(request.url);
    const role = url.searchParams.get('role') === 'display' ? 'display' : 'controller';

    const pair = new WebSocketPair();
    const [client, server] = Object.values(pair);
    server.accept();

    const mine = role === 'display' ? this.displays : this.controllers;
    const targets = role === 'display' ? this.controllers : this.displays;
    mine.add(server);

    server.addEventListener('message', (ev) => {
      for (const ws of targets) {
        try { ws.send(ev.data); } catch (e) { targets.delete(ws); }
      }
    });
    const cleanup = () => mine.delete(server);
    server.addEventListener('close', cleanup);
    server.addEventListener('error', cleanup);

    return new Response(null, { status: 101, webSocket: client });
  }
}

export default {
  async fetch(request, env) {
    const url = new URL(request.url);
    if (url.pathname === '/ws') {
      const room = url.searchParams.get('room');
      if (!room) return new Response('missing room', { status: 400 });
      const id = env.ROOM.idFromName(room);
      return env.ROOM.get(id).fetch(request);
    }
    return new Response('tilt-ninja relay ok', { status: 200 });
  }
};
