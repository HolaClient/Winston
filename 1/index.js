const server = require("./server");

server.get("/", () => {
    return "Hello, world!";
});

server.start();