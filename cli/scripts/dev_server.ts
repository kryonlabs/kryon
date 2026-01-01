/**
 * Kryon Dev Server (Bun)
 * Simple static file server for development
 */

const port = parseInt(Bun.argv[2] || "3000");
const root = Bun.argv[3] || "./build";

console.log(`\n╔══════════════════════════════════════════════════════════════╗`);
console.log(`║  Kryon Dev Server (Bun)                                      ║`);
console.log(`╠══════════════════════════════════════════════════════════════╣`);
console.log(`║  Local:   http://127.0.0.1:${port.toString().padEnd(5)}                            ║`);
console.log(`║  Root:    ${root.slice(0, 50).padEnd(50)}║`);
console.log(`╠══════════════════════════════════════════════════════════════╣`);
console.log(`║  Press Ctrl+C to stop                                        ║`);
console.log(`╚══════════════════════════════════════════════════════════════╝\n`);

Bun.serve({
  port,
  async fetch(req) {
    let path = new URL(req.url).pathname;

    // Directory without trailing slash -> add it
    if (!path.endsWith("/") && !path.includes(".")) {
      path += "/";
    }

    // Directory -> index.html
    if (path.endsWith("/")) {
      path += "index.html";
    }

    const file = Bun.file(root + path);
    if (await file.exists()) {
      return new Response(file);
    }

    // 404
    return new Response("Not Found", { status: 404 });
  }
});
