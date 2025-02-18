const { Server } = require('./hcw.node');

const server = new Server();
server.listen(8080);

process.on('SIGINT', () => {
    server.stop();
    process.exit(0);
});
