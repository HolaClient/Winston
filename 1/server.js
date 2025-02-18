const net = require('net');
const cluster = require('cluster');
const os = require('os');

const a = new Map();
const b = new Map();

const c = Buffer.from('\r\n');
const d = Buffer.from('\r\n\r\n');
const e = Buffer.from('HTTP/1.1 200 OK\r\n');
const f = Buffer.from('HTTP/1.1 404 Not Found\r\n');
const g = Buffer.from(
    'Content-Type: text/plain\r\n' +
    'Connection: keep-alive\r\n' +
    'Keep-Alive: timeout=5, max=1000\r\n'
);

exports.get = (h, i) => {
    a.set(h, i);
    const j = i();
    if (typeof j === 'string') {
        const k = Buffer.from(`Content-Length: ${Buffer.byteLength(j)}\r\n`);
        const l = Buffer.from(j);
        b.set(h, Buffer.concat([e, g, k, c, l]));
    }
};

exports.start = () => {
    if (cluster.isPrimary) {
        const m = os.cpus().length;
        console.log(`Primary ${process.pid} is running`);
        
        for (let n = 0; n < m; n++) {
            cluster.fork();
        }
        
        cluster.on('exit', (o) => {
            console.log(`Worker ${o.process.pid} died`);
            cluster.fork();
        });
    } else {
        p();
    }
};

function p() {
    const q = new SharedArrayBuffer(4);
    const r = new Int32Array(q);
    let s = 0;

    server = net.createServer({ noDelay: true }, (t) => {
        let u = Buffer.alloc(0);
        s++;

        t.setNoDelay(true);
        t.setKeepAlive(true, 1000);

        t.on('error', v => {
            if (v.code !== 'ECONNRESET') console.error('Socket error:', v.message);
            t.destroy();
        });

        t.on('close', () => s--);

        t.on('data', w => {
            u = Buffer.concat([u, w]);
            
            const x = u.indexOf('\r\n\r\n');
            if (x !== -1) {
                const y = z(u);
                
                const aa = b.get(y);
                if (aa) {
                    t.write(aa);
                } else {
                    const ab = a.get(y);
                    if (ab) {
                        const ac = ab();
                        const ad = ae(200, ac);
                        t.write(ad);
                    } else {
                        t.write(af());
                    }
                }
                
                u = u.slice(x + 4);
                Atomics.add(r, 0, 1);
            }
        });
    });

    function af() {
        return Buffer.concat([f, g, Buffer.from('Content-Length: 9\r\n\r\nNot Found')]);
    }

    function z(ag) {
        const ah = ag.indexOf(' ') + 1;
        const ai = ag.indexOf(' ', ah);
        return ag.slice(ah, ai).toString().split('?')[0];
    }

    function ae(aj, ak) {
        const al = Buffer.from(`Content-Length: ${Buffer.byteLength(ak)}\r\n`);
        return Buffer.concat([
            aj === 200 ? e : f,
            g,
            al,
            c,
            Buffer.from(ak)
        ]);
    }

    setInterval(() => {
        const am = Atomics.exchange(r, 0, 0);
        console.log({
            rps: `${am}/sec`,
            activeConnections: s,
            memoryUsage: `${Math.round(process.memoryUsage().heapUsed / 1024 / 1024)}MB`,
            cpuUsage: `${Math.round(os.loadavg()[0] * 100)}%`
        });
        Atomics.store(r, 0, 0);
    }, 1000);

    server.maxConnections = 1000000;
    server.listen(3000, () => {
        console.log(`Worker ${process.pid} started`);
    });
}