const net = require('net');
const cluster = require('cluster');
const os = require('os');
const { Worker } = require('worker_threads');

const a = new Map();
const b = new Map();
const c = [];
const d = 10000;
const e = 4096;

const f = Buffer.from('\r\n');
const g = Buffer.from('\r\n\r\n');
const h = Buffer.from('HTTP/1.1 200 OK\r\n');
const i = Buffer.from('HTTP/1.1 404 Not Found\r\n');
const j = Buffer.from(
    'Content-Type: text/plain\r\n' +
    'Connection: keep-alive\r\n' +
    'Keep-Alive: timeout=5, max=10000\r\n'
);

for (let k = 0; k < 1000; k++) {
    c.push(Buffer.allocUnsafe(e));
}

function l() {
    return c.pop() || Buffer.allocUnsafe(e);
}

function m(n) {
    if (c.length < d) {
        c.push(n);
    }
}

exports.get = (o, p) => {
    a.set(o, p);
    const q = p();
    if (typeof q === 'string') {
        const r = Buffer.from(`Content-Length: ${Buffer.byteLength(q)}\r\n`);
        const s = Buffer.from(q);
        b.set(o, Buffer.concat([h, j, r, f, s]));
    }
};

exports.start = () => {
    if (cluster.isPrimary) {
        const t = os.cpus().length;
        console.log(`Primary ${process.pid} is running`);
        
        for (let u = 0; u < t; u++) {
            cluster.fork();
        }
        
        cluster.on('exit', (v) => {
            console.log(`Worker ${v.process.pid} died`);
            cluster.fork();
        });
    } else {
        w();
    }
};

function w() {
    const x = new SharedArrayBuffer(4);
    const y = new Int32Array(x);
    let z = 0;

    const aa = Buffer.concat([i, j, Buffer.from('Content-Length: 9\r\n\r\nNot Found')]);
    
    const ab = net.createServer({
        noDelay: true,
        keepAlive: true,
        keepAliveInitialDelay: 0,
        highWaterMark: 1024 * 64
    }, ac);

    function ac(ad) {
        let ae = l();
        let af = 0;
        z++;

        ad.setNoDelay(true);
        ad.setKeepAlive(true, 1000);
        ad.setTimeout(30000);
        ad.setDefaultEncoding('utf8');

        ad.on('end', ag);
        ad.on('error', ag);
        ad.on('timeout', ag);

        function ag() {
            z--;
            m(ae);
            ad.destroy();
        }

        ad.on('data', ah => {
            const ai = ah.length;
            
            if (af + ai > e) {
                af = 0;
            }

            ah.copy(ae, af);
            af += ai;

            const aj = ae.indexOf(g, 0, af);
            if (aj !== -1) {
                let ak = ae.indexOf(Buffer.from(' '), 0, aj) + 1;
                let al = ae.indexOf(Buffer.from(' '), ak, aj);
                let am = ae.slice(ak, al).toString();

                const an = am.indexOf('?');
                if (an !== -1) {
                    am = am.slice(0, an);
                }

                const ao = b.get(am);
                if (ao) {
                    ad.write(ao, ap => {
                        if (!ap) Atomics.add(y, 0, 1);
                    });
                } else {
                    const aq = a.get(am);
                    if (aq) {
                        const ar = aq();
                        if (ar) {
                            const as = at(200, ar);
                            ad.write(as, au => {
                                if (!au) Atomics.add(y, 0, 1);
                            });
                        }
                    } else {
                        ad.write(aa, av => {
                            if (!av) Atomics.add(y, 0, 1);
                        });
                    }
                }

                af = 0;
            }
        });
    }

    function at(aw, ax) {
        const ay = Buffer.from(`Content-Length: ${Buffer.byteLength(ax)}\r\n`);
        return Buffer.concat([
            aw === 200 ? h : i,
            j,
            ay,
            f,
            Buffer.from(ax)
        ]);
    }

    setInterval(() => {
        const az = Atomics.exchange(y, 0, 0);
        console.log({
            rps: `${az}/sec`,
            activeConnections: z,
            memoryUsage: `${Math.round(process.memoryUsage().heapUsed / 1024 / 1024)}MB`,
            cpuUsage: `${Math.round(os.loadavg()[0] * 100)}%`
        });
        Atomics.store(y, 0, 0);
    }, 1000);

    ab.maxConnections = 1000000;
    ab.on('error', ba => console.error('Server error:', ba));

    if (global.gc) {
        setInterval(global.gc, 5000);
    }

    process.env.UV_THREADPOOL_SIZE = '128';

    ab.listen(3000, () => {
        process.setMaxListeners(0);
        
        if (process.platform === 'linux') {
            try {
                require('fs').writeFileSync('/proc/self/oom_score_adj', '-1000');
            } catch (bb) {
                if (bb.code !== 'EACCES') {
                    console.error('System optimization error:', bb);
                }
            }

            try {
                if (process.getuid && process.getuid() === 0) {
                    process.setuid(1000);
                }
            } catch (bc) {
                if (bc.code !== 'EPERM') {
                    console.error('UID setting error:', bc);
                }
            }
        }

        try {
            if (process.setrlimit) {
                process.setrlimit('nofile', { soft: 65535, hard: 65535 });
            }
        } catch (bd) {}

        console.log(`Worker ${process.pid} started`);
    });
}