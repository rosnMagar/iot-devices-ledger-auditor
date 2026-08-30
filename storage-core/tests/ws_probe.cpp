// IOT-45 build probe: proves the vendored IXWebSocket compiles, links and can
// bind a listener. Not part of the product — the real handler is IOT-28.
#include <ixwebsocket/IXWebSocketServer.h>
#include <cstdio>

int main() {
    ix::WebSocketServer server(8099, "127.0.0.1");
    server.setOnClientMessageCallback(
        [](std::shared_ptr<ix::ConnectionState>, ix::WebSocket& ws,
           const ix::WebSocketMessagePtr& msg) {
            if (msg->type == ix::WebSocketMessageType::Message) ws.send(msg->str);
        });
    auto res = server.listen();
    std::printf("listen: %s\n", res.first ? "ok" : res.second.c_str());
    if (!res.first) return 1;
    server.stop();
    std::printf("probe ok\n");
    return 0;
}
