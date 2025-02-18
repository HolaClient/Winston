const net = require("net");
const cluster = require("cluster");
const os = require("os");

const BUFFER_VIEWS = new Uint8Array(new SharedArrayBuffer(1024 * 1024 * 16));
const ROUTES_BUFFER = new Uint32Array(new SharedArrayBuffer(65536 * 4));
const RESPONSE_VIEWS = new Uint8Array(new SharedArrayBuffer(1024 * 1024 * 32));
const PATH_LOOKUP = new Uint32Array(65536);
const OFFSET_TABLE = new Uint32Array(65536);
const constants = {
  BUFFER_SIZE: 4096,
  RING_SIZE: 16384,
  CACHE_SIZE: 2048,
  BATCH_SIZE: 32,
  POOL_SIZE: 10000,
  MAX_BUFFERS: 32,
  PORT: process.env.PORT || 3000,
  RESPONSE_CHUNK: 65536,
  MAX_PATHS: 65536,
  CPU_OFFSET: 1,
};
const CRLF = Buffer.from("\r\n");
const DOUBLE_CRLF = Buffer.from("\r\n\r\n");
const HTTP_OK = Buffer.from("HTTP/1.1 200 OK\r\n");
const HTTP_NOT_FOUND = Buffer.from("HTTP/1.1 404 Not Found\r\n");
const COMMON_HEADERS = Buffer.from(
  "Content-Type: text/plain\r\nConnection: keep-alive\r\nKeep-Alive: timeout=5, max=10000"
);
const HTTP_COMPONENTS = { OK: HTTP_OK, HEADERS: COMMON_HEADERS };
const state = {
  activeConnections: 0,
  currentBuffer: 0,
  poolIndex: 0,
  routes: new Map(),
  requestCounter: null,
  routeCount: 0,
};
const sharedBuffers = new Array(constants.MAX_BUFFERS)
  .fill(null)
  .map(() => Buffer.allocUnsafe(constants.BUFFER_SIZE * 2));
const PATH_CACHE = new Uint32Array(65536);
const RESPONSE_POOL = new Array(constants.POOL_SIZE);
const bufferPool = new Array(1000)
  .fill(null)
  .map(() => Buffer.allocUnsafe(constants.BUFFER_SIZE));
const RESPONSE_LENGTHS = new Uint32Array(new SharedArrayBuffer(65536 * 4));

class FastCache {
  constructor(a) {
    this.size = a;
    this.data = new Array(a);
    this.hash = new Uint32Array(a);
  }
  get(a) {
    const b = this.hashCode(a),
      c = b % this.size;
    return this.hash[c] === b ? this.data[c] : null;
  }
  set(a, b) {
    const c = this.hashCode(a),
      d = c % this.size;
    this.hash[d] = c;
    this.data[d] = b;
  }
  hashCode(a) {
    let b = 0;
    for (let c = 0; c < a.length; c++)
      (b = (b << 5) - b + a.charCodeAt(c)), (b &= b);
    return b >>> 0;
  }
}

const responseCache = new FastCache(constants.CACHE_SIZE);

function initResponseTemplates() {
  let a = 0;
  for (let b = 0; b < 1000; b++) {
    const c = b + 1;
    OFFSET_TABLE[b] = a;
    const d = Buffer.concat([
      HTTP_COMPONENTS.OK,
      HTTP_COMPONENTS.HEADERS,
      Buffer.from(`Content-Length: ${c}\r\n\r\n`),
    ]);
    d.copy(RESPONSE_VIEWS, a);
    a += d.length + c;
    RESPONSE_LENGTHS[b] = d.length + c;
  }
}

function handleConnection(a) {
  a.setNoDelay(true);
  state.activeConnections++;
  let b = Buffer.allocUnsafe(constants.BUFFER_SIZE),
    c = 0;
  function d() {
    state.activeConnections--;
    a.destroy();
  }
  a.on("data", (e) => {
    e.copy(b, c);
    c += e.length;
    if (b.includes(DOUBLE_CRLF)) {
      const f = b.indexOf("\r\n");
      if (f !== -1) {
        const g = b.toString("utf8", 0, f).split(" ")[1] || "/",
          h = g.split("?")[0],
          i =
            ((j) => {
              let k = 0;
              for (let l = 0; l < j.length; l++)
                k = (k << 5) - k + j.charCodeAt(l);
              return k >>> 0;
            })(h) & 0xffff,
          l = PATH_LOOKUP[i];
        if (l) {
          const m = OFFSET_TABLE[l],
            n = RESPONSE_LENGTHS[l],
            o = Buffer.from(RESPONSE_VIEWS.buffer, m, n);
          a.write(o);
        } else a.write(notFoundResponse);
      }
      c = 0;
      Atomics.add(state.requestCounter, 0, 1);
    }
  });
  a.on("error", (e) => {
    e.code !== "ECONNRESET" && console.error("Socket error:", e);
    d();
  });
  a.on("close", d);
}

function startWorker() {
  state.requestCounter = new Int32Array(new SharedArrayBuffer(4));
  initResponseTemplates();
  const a = net.createServer({
    noDelay: true,
    keepAlive: true,
    keepAliveInitialDelay: 0,
  });
  a.on("connection", handleConnection);
  a.on("error", (b) => console.error("Server error:", b));
  setInterval(() => {
    const b = Atomics.exchange(state.requestCounter, 0, 0);
    b > 0 &&
      console.log({
        rps: `${b}/sec`,
        activeConnections: state.activeConnections,
        memoryUsage: `${Math.round(
          process.memoryUsage().heapUsed / 1024 / 1024
        )}MB`,
        cpuUsage: `${Math.round(os.loadavg()[0] * 100)}%`,
      });
    Atomics.store(state.requestCounter, 0, 0);
  }, 1000);
  a.maxConnections = 1000000;
  process.env.UV_THREADPOOL_SIZE = "128";
  a.listen(constants.PORT, "0.0.0.0", () => {
    process.setMaxListeners(0);
    if (process.platform === "linux") {
      try {
        require("fs").writeFileSync("/proc/self/oom_score_adj", "-1000");
      } catch (b) {
        b.code !== "EACCES" && console.error("System optimization error:", b);
      }
      try {
        process.getuid && process.getuid() === 0 && process.setuid(1000);
      } catch (b) {
        b.code !== "EPERM" && console.error("UID setting error:", b);
      }
    }
    try {
      process.setrlimit &&
        process.setrlimit("nofile", { soft: 65535, hard: 65535 });
    } catch (b) { }
    console.log(`Worker ${process.pid} started on port ${constants.PORT}`);
  });
}

const notFoundResponse = Buffer.concat([
  HTTP_NOT_FOUND,
  COMMON_HEADERS,
  Buffer.from("\r\nContent-Length: 9\r\n\r\nNot Found"),
]);

exports.get = (a, b) => {
  const c = b() || "",
    d = ((e) => {
      let f = 0;
      for (let g = 0; g < e.length; g++) f = (f << 5) - f + e.charCodeAt(g);
      return f >>> 0;
    })(a),
    e = state.routeCount++;
  PATH_LOOKUP[d & 0xffff] = e;
  const f = Buffer.concat([
    HTTP_OK,
    COMMON_HEADERS,
    Buffer.from(`\r\nContent-Length: ${Buffer.byteLength(c)}\r\n\r\n`),
  ]),
    g = OFFSET_TABLE[e];
  f.copy(RESPONSE_VIEWS, g);
  Buffer.from(c).copy(RESPONSE_VIEWS, g + f.length);
  RESPONSE_LENGTHS[e] = f.length + Buffer.byteLength(c);
};

exports.start = () => {
  if (cluster.isPrimary) {
    const a = os.cpus().length;
    console.log(`Primary ${process.pid} is running`);
    for (let b = 0; b < a; b++) cluster.fork();
    cluster.on("exit", (b) => {
      console.log(`Worker ${b.process.pid} died`);
      cluster.fork();
    });
  } else startWorker();
};
