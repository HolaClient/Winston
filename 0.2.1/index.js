const { Server } = require('./hcw.node');

let server = null;

try {
    server = new Server();
    server.listen(2080);
    console.log('Server started on port 2080');
    
    setInterval(() => {}, 1000);
} catch (err) {
    console.error('Failed to start server:', err);
    process.exit(1);
}

['SIGINT', 'SIGTERM', 'SIGHUP'].forEach(signal => {
    process.on(signal, () => {
        console.log(`\nReceived ${signal}, shutting down...`);
        if (server) {
            server.stop();
            server = null;
        }
        process.exit(0);
    });
});