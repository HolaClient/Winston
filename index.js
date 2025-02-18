const a = require("./server");
a.get("/", () => "Hello, world!");
a.start();
