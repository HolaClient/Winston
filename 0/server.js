const net = require('net');
const cluster = require('cluster');
const os = require('os');

const a = new Map();
let b;

exports.get = (c, d) => {
    a.set(c, d);
};

exports.start = () => {
    if (cluster.isPrimary) {
        const e = os.cpus().length;
        console.log(`Primary ${process.pid} is running`);
        
        for (let f = 0; f < e; f++) {
            cluster.fork();
        }
        
        cluster.on('exit', (g) => {
            console.log(`Worker ${g.process.pid} died`);
            cluster.fork();
        });
    } else {
        h();
    }
};

function h() {
    const i = new SharedArrayBuffer(4);
    const j = new Int32Array(i);
    let k = 0;

    b = net.createServer({ noDelay: true }, l);
    
    function l(m) {
        let n = '';
        k++;

        m.setNoDelay(true);
        m.setKeepAlive(true, 1000);

        m.on('error', o => {
            if (o.code !== 'ECONNRESET') console.error('Socket error:', o.message);
            m.destroy();
        });

        m.on('close', () => k--);

        m.on('data', p => {
            n += p;
            if (n.includes('\r\n\r\n')) {
                q(m, n);
                n = '';
                Atomics.add(j, 0, 1);
            }
        });
    }

    function q(r, s) {
        const [t] = s.split('\r\n');
        const [, u] = t.split(' ');
        const v = u.split('?')[0];
        
        const w = a.get(v);
        const x = w ? w() : 'Not Found';
        const y = w ? 200 : 404;
        
        const z = aa({
            status: y,
            body: x
        });

        r.write(z);
    }

    function aa({ status = 200, body = '' }) {
        const ab = {
            'Content-Type': 'text/plain',
            'Content-Length': Buffer.byteLength(body),
            'Connection': 'keep-alive',
            'Keep-Alive': 'timeout=5, max=1000'
        };

        return `HTTP/1.1 ${status} ${status === 200 ? 'OK' : 'Not Found'}\r\n${
            Object.entries(ab)
                .map(([ac, ad]) => `${ac}: ${ad}`)
                .join('\r\n')
        }\r\n\r\n${body}`;
    }

    setInterval(() => {
        const ae = Atomics.exchange(j, 0, 0);
        console.log({
            rps: `${ae}/sec`,
            activeConnections: k,
            memoryUsage: `${Math.round(process.memoryUsage().heapUsed / 1024 / 1024)}MB`,
            cpuUsage: `${Math.round(os.loadavg()[0] * 100)}%`
        });
        Atomics.store(j, 0, 0);
    }, 1000);

    b.maxConnections = 1000000;
    b.listen(3000, () => {
        console.log(`Worker ${process.pid} started`);
    });
}